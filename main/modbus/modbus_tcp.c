#include "modbus_tcp.h"
#include <string.h>
#include <errno.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

static const char* TAG = "MODBUS_TCP";

uint16_t modbus_htons(uint16_t hostshort) {
    return ((hostshort & 0xff) << 8) | ((hostshort >> 8) & 0xff);
}

void modbus_clear_socket_buffer(int socket) {
    uint8_t buf[128];
    int available = 1;
    struct timeval timeout = {0, 0};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    while (available > 0) {
        available = recv(socket, buf, sizeof(buf), MSG_DONTWAIT);
    }
    
    timeout.tv_sec = MODBUS_TIMEOUT_MS / 1000;
    timeout.tv_usec = (MODBUS_TIMEOUT_MS % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

esp_err_t modbus_tcp_init(modbus_tcp_client_t *client, const char *ip_address, uint8_t unit_id) {
    if (!client || !ip_address) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(client->ip_address, ip_address, sizeof(client->ip_address) - 1);
    client->ip_address[sizeof(client->ip_address) - 1] = '\0';
    client->port = MODBUS_TCP_PORT;
    client->unit_id = unit_id;
    client->socket_fd = -1;
    client->connected = false;
    client->transaction_counter = 1;
    
    return ESP_OK;
}

esp_err_t modbus_tcp_connect(modbus_tcp_client_t *client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->connected && client->socket_fd >= 0) {
        return ESP_OK;  // Already connected
    }
    
    // Close existing connection if any
    modbus_tcp_disconnect(client);
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(client->ip_address);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(client->port);
    
    client->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client->socket_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return ESP_FAIL;
    }
    
    struct timeval timeout;
    timeout.tv_sec = MODBUS_TIMEOUT_MS / 1000;
    timeout.tv_usec = (MODBUS_TIMEOUT_MS % 1000) * 1000;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    int err = connect(client->socket_fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Failed to connect to %s:%d - %s", 
                 client->ip_address, client->port, strerror(errno));
        close(client->socket_fd);
        client->socket_fd = -1;
        return ESP_FAIL;
    }
    
    client->connected = true;
    ESP_LOGI(TAG, "Connected to %s:%d (Unit ID: %d)", 
             client->ip_address, client->port, client->unit_id);
    return ESP_OK;
}

void modbus_tcp_disconnect(modbus_tcp_client_t *client) {
    if (!client) {
        return;
    }
    
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    client->connected = false;
}

esp_err_t modbus_tcp_read_registers(modbus_tcp_client_t *client, uint16_t address, 
                                   uint16_t count, uint16_t *data) {
    if (!client || !data || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Ensure connection
    if (modbus_tcp_connect(client) != ESP_OK) {
        return ESP_FAIL;
    }
    
    modbus_clear_socket_buffer(client->socket_fd);
    
    // Build request
    modbus_tcp_request_t request;
    request.transaction_id = modbus_htons(client->transaction_counter++);
    request.protocol_id = modbus_htons(0);
    request.length = modbus_htons(6);
    request.unit_id = client->unit_id;
    request.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    request.start_address = modbus_htons(address);
    request.count = modbus_htons(count);
    
    // Send request
    int written = send(client->socket_fd, &request, sizeof(request), 0);
    if (written != sizeof(request)) {
        ESP_LOGW(TAG, "Send failed for address 0x%04X", address);
        modbus_tcp_disconnect(client);
        return ESP_FAIL;
    }
    
    // Read response header (7 bytes)
    uint8_t header_buf[7];
    int len_header = recv(client->socket_fd, header_buf, sizeof(header_buf), 0);
    if (len_header != sizeof(header_buf)) {
        ESP_LOGW(TAG, "Header read failed for address 0x%04X", address);
        modbus_tcp_disconnect(client);
        return ESP_FAIL;
    }
    
    // Read response data
    uint8_t data_buf[256];
    int len_data = recv(client->socket_fd, data_buf, sizeof(data_buf), 0);
    if (len_data < 3) {
        ESP_LOGW(TAG, "Data read failed for address 0x%04X", address);
        modbus_tcp_disconnect(client);
        return ESP_FAIL;
    }
    
    // Check function code
    if (data_buf[0] != MODBUS_FC_READ_HOLDING_REGISTERS) {
        if (data_buf[0] == (MODBUS_FC_READ_HOLDING_REGISTERS | 0x80)) {
            ESP_LOGW(TAG, "Exception response for 0x%04X: 0x%02X", address, data_buf[1]);
        } else {
            ESP_LOGW(TAG, "Invalid function code for 0x%04X: 0x%02X", address, data_buf[0]);
        }
        return ESP_FAIL;
    }
    
    // Check byte count
    uint8_t byte_count = data_buf[1];
    if (byte_count != count * 2) {
        ESP_LOGW(TAG, "Invalid byte count for 0x%04X: %d (expected %d)", 
                 address, byte_count, count * 2);
        return ESP_FAIL;
    }
    
    // Extract register data
    for (int i = 0; i < count; i++) {
        data[i] = (data_buf[2 + i * 2] << 8) | data_buf[2 + i * 2 + 1];
    }
    
    return ESP_OK;
}

esp_err_t modbus_tcp_write_register(modbus_tcp_client_t *client, uint16_t address, uint16_t value) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Ensure connection
    if (modbus_tcp_connect(client) != ESP_OK) {
        return ESP_FAIL;
    }
    
    modbus_clear_socket_buffer(client->socket_fd);
    
    // Build request
    typedef struct {
        uint16_t transaction_id;
        uint16_t protocol_id;
        uint16_t length;
        uint8_t unit_id;
        uint8_t function_code;
        uint16_t address;
        uint16_t value;
    } __attribute__((packed)) write_single_request_t;
    
    write_single_request_t request;
    request.transaction_id = modbus_htons(client->transaction_counter++);
    request.protocol_id = modbus_htons(0);
    request.length = modbus_htons(6);
    request.unit_id = client->unit_id;
    request.function_code = MODBUS_FC_WRITE_SINGLE_REGISTER;
    request.address = modbus_htons(address);
    request.value = modbus_htons(value);
    
    // Send request
    int written = send(client->socket_fd, &request, sizeof(request), 0);
    if (written != sizeof(request)) {
        ESP_LOGW(TAG, "Write send failed for address 0x%04X", address);
        modbus_tcp_disconnect(client);
        return ESP_FAIL;
    }
    
    // Read response
    uint8_t response[12];
    int len = recv(client->socket_fd, response, sizeof(response), 0);
    if (len < 12) {
        ESP_LOGW(TAG, "Write response failed for address 0x%04X", address);
        modbus_tcp_disconnect(client);
        return ESP_FAIL;
    }
    
    // Verify response (should echo the request)
    if (response[7] != MODBUS_FC_WRITE_SINGLE_REGISTER) {
        ESP_LOGW(TAG, "Write verification failed for address 0x%04X", address);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t modbus_tcp_write_registers(modbus_tcp_client_t *client, uint16_t address, 
                                    uint16_t count, uint16_t *data) {
    if (!client || !data || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Ensure connection
    if (modbus_tcp_connect(client) != ESP_OK) {
        return ESP_FAIL;
    }
    
    modbus_clear_socket_buffer(client->socket_fd);
    
    // Build request
    uint8_t request_buf[256];
    int pos = 0;
    
    // MBAP Header
    *(uint16_t*)(request_buf + pos) = modbus_htons(client->transaction_counter++); pos += 2;
    *(uint16_t*)(request_buf + pos) = modbus_htons(0); pos += 2;  // Protocol ID
    *(uint16_t*)(request_buf + pos) = modbus_htons(7 + count * 2); pos += 2;  // Length
    request_buf[pos++] = client->unit_id;
    
    // PDU
    request_buf[pos++] = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    *(uint16_t*)(request_buf + pos) = modbus_htons(address); pos += 2;
    *(uint16_t*)(request_buf + pos) = modbus_htons(count); pos += 2;
    request_buf[pos++] = count * 2;  // Byte count
    
    // Data
    for (int i = 0; i < count; i++) {
        *(uint16_t*)(request_buf + pos) = modbus_htons(data[i]);
        pos += 2;
    }
    
    // Send request
    int written = send(client->socket_fd, request_buf, pos, 0);
    if (written != pos) {
        ESP_LOGW(TAG, "Multi-write send failed for address 0x%04X", address);
        modbus_tcp_disconnect(client);
        return ESP_FAIL;
    }
    
    // Read response
    uint8_t response[12];
    int len = recv(client->socket_fd, response, sizeof(response), 0);
    if (len < 12) {
        ESP_LOGW(TAG, "Multi-write response failed for address 0x%04X", address);
        modbus_tcp_disconnect(client);
        return ESP_FAIL;
    }
    
    // Verify response
    if (response[7] != MODBUS_FC_WRITE_MULTIPLE_REGISTERS) {
        ESP_LOGW(TAG, "Multi-write verification failed for address 0x%04X", address);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}