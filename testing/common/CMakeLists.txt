add_library(testing SHARED
    graph.cpp
    testcase.cpp
)
target_link_libraries(testing
    orchestrator
)
