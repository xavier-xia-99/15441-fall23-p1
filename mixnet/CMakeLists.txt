add_executable(node node.c utils.c)
target_link_libraries(node
    fragment
)
