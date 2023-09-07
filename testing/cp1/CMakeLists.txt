file(GLOB files "testcase*.cpp")
foreach(file ${files})
    get_filename_component(testcase ${file} NAME_WE)
    add_executable(${testcase} ${file})

    set_target_properties(${testcase}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/cp1"
    )
    target_link_libraries(${testcase} testing)
    if(MATH_LIBRARY)
        target_link_libraries(${testcase} ${MATH_LIBRARY})
    endif()
endforeach()
