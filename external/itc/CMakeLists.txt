add_compile_options(-Wno-pointer-arith)

add_library(message_queue SHARED
    message_queue.c
)
