zephyr_library()

zephyr_library_sources(m90e26_uart_rtio.c)

# 核心修正：設定原始碼檔案的屬性，明確其目標檔案(.obj)的依賴項
# 這樣 CMake 就會確保在編譯 m90e26_uart_rtio.c 之前，offsets.h 檔案已經存在
set_source_files_properties(m90e26_uart_rtio.c
    PROPERTIES OBJECT_DEPENDS
    "${ZEPHYR_BINARY_DIR}/include/generated/zephyr/offsets.h"
)