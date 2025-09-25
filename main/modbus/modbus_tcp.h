#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdint.h>
#include "esp_err.h"
#include <stdbool.h>

// Modbus TCP configuration
#define MODBUS_TCP_PORT 502
#define MODBUS_TIMEOUT_MS 5000
#define MODBUS_MAX_RETRIES 3

// Modbus function codes
#define MODBUS_FC_READ_HOLDING_REGISTERS    0x03
#define MODBUS_FC_READ_INPUT_REGISTERS      0x04
#define MODBUS_FC_WRITE_SINGLE_REGISTER     0x06
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS  0x10
#define MODBUS_FC_READ_DEVICE_ID            0x2B

// Modbus TCP frame structure
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t count;
} __attribute__((packed)) modbus_tcp_request_t;

typedef struct {
    char ip_address[16];
    uint16_t port;
    uint8_t unit_id;
    int socket_fd;
    bool connected;
    uint16_t transaction_counter;
} modbus_tcp_client_t;

// Function prototypes
esp_err_t modbus_tcp_init(modbus_tcp_client_t *client, const char *ip_address, uint8_t unit_id);
esp_err_t modbus_tcp_connect(modbus_tcp_client_t *client);
void modbus_tcp_disconnect(modbus_tcp_client_t *client);
esp_err_t modbus_tcp_read_registers(modbus_tcp_client_t *client, uint16_t address, uint16_t count, uint16_t *data);
esp_err_t modbus_tcp_write_register(modbus_tcp_client_t *client, uint16_t address, uint16_t value);
esp_err_t modbus_tcp_write_registers(modbus_tcp_client_t *client, uint16_t address, uint16_t count, uint16_t *data);

// Utility functions
uint16_t modbus_htons(uint16_t hostshort);
void modbus_clear_socket_buffer(int socket);

#endif // MODBUS_TCP_H