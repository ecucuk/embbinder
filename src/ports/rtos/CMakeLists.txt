add_library(embbinder_port_rtos
    src/port_rtos.cpp
    src/transport_queue.cpp
)

target_include_directories(embbinder_port_rtos
    PUBLIC include
)

# FreeRTOS include/link ayarlarını projene göre buraya ekle:
# target_include_directories(embbinder_port_freertos PUBLIC <freertos_inc_paths>)
# target_link_libraries(embbinder_port_freertos freertos_kernel)