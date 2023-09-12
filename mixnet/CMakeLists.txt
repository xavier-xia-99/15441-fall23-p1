add_executable(node node.c packet_init.c)
target_link_libraries(node
    fragment
)
