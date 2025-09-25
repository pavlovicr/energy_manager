# Energy Manager - Glavni projektni CMakeLists.txt
# Ta datoteka mora biti v root direktoriju projekta

cmake_minimum_required(VERSION 3.16)

# ESP-IDF build sistem MORA biti vključen PRED project() klicom
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Ime projekta
project(energy_manager) 