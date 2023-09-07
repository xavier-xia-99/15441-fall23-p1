add_library(framework SHARED
    networking.cpp
    message.cpp
)

add_library(fragment SHARED fragment.cpp)
target_link_libraries(fragment
    framework
    message_queue
    Threads::Threads
)

add_library(orchestrator SHARED orchestrator.cpp)
target_link_libraries(orchestrator
    framework
    message_queue
    Threads::Threads
)
