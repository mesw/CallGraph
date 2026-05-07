cmake_minimum_required(VERSION 3.21)

find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

# Helper to create a test executable
function(add_cg_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE
        cg_core cg_resolver cg_query cg_export
        Qt6::Test
    )
    set_target_properties(${name} PROPERTIES
        MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
    )
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /WX)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_cg_test(test_resolver test_resolver.cpp)
add_cg_test(test_query    test_query.cpp)
add_cg_test(test_export   test_export.cpp)
