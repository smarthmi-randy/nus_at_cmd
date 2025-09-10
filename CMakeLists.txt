cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nus_at_cmd)

target_sources(app PRIVATE
        src/main.c
        src/at_command.c
        src/hmi_uart.c
        src/cat.c
        src/value_reporter.c
)
