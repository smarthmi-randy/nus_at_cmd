cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nus_at_cmd)

# 納入 src 和 drivers 目錄
add_subdirectory(drivers)

target_sources(app PRIVATE
        src/main.c
        src/at_command.c
        src/hmi_uart.c
        src/cat.c
        src/value_reporter.c
        src/sensor_handler.c
)
target_link_libraries(app PRIVATE drivers__sensor__atmel__m90e26)