cmake_minimum_required(VERSION 3.21)
project(CallGraph VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC-specific flags
if(MSVC)
    add_compile_options(/W4 /WX /utf-8 /permissive-)
    add_compile_definitions(_UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# Static Qt linking
set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a")
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Find Qt
find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Concurrent
)
qt_standard_project_setup()

# tree-sitter (vendored)
add_library(tree-sitter STATIC
    third_party/tree-sitter/lib/src/lib.c
)
target_include_directories(tree-sitter PUBLIC
    third_party/tree-sitter/lib/include
    third_party/tree-sitter/lib/src
)
target_compile_definitions(tree-sitter PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(tree-sitter PROPERTIES
    C_STANDARD 11
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# tree-sitter-cpp grammar (vendored)
add_library(tree-sitter-cpp STATIC
    third_party/tree-sitter-cpp/src/parser.c
    third_party/tree-sitter-cpp/src/scanner.c
)
target_include_directories(tree-sitter-cpp PUBLIC
    third_party/tree-sitter-cpp/src
)
target_link_libraries(tree-sitter-cpp PUBLIC tree-sitter)
target_compile_definitions(tree-sitter-cpp PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(tree-sitter-cpp PROPERTIES
    C_STANDARD 11
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# Core library (no Qt dependency)
add_library(cg_core STATIC
    src/core/Index.cpp
)
target_include_directories(cg_core PUBLIC src)
target_compile_features(cg_core PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(cg_core PRIVATE /W4 /WX)
endif()
set_target_properties(cg_core PROPERTIES
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# Discovery library (uses Qt for QProcess)
add_library(cg_discovery STATIC
    src/discovery/FileDiscovery.cpp
)
target_include_directories(cg_discovery PUBLIC src)
target_link_libraries(cg_discovery PUBLIC cg_core Qt6::Core)
if(MSVC)
    target_compile_options(cg_discovery PRIVATE /W4 /WX)
endif()
set_target_properties(cg_discovery PROPERTIES
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# Parser library (uses Qt for threading)
add_library(cg_parser STATIC
    src/parser/FactExtractor.cpp
    src/parser/ParserPool.cpp
    src/parser/TreeSitterParser.cpp
)
target_include_directories(cg_parser PUBLIC src)
target_link_libraries(cg_parser PUBLIC cg_core tree-sitter-cpp Qt6::Core Qt6::Concurrent)
if(MSVC)
    target_compile_options(cg_parser PRIVATE /W4 /WX)
endif()
set_target_properties(cg_parser PROPERTIES
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# Resolver library
add_library(cg_resolver STATIC
    src/resolver/Resolver.cpp
)
target_include_directories(cg_resolver PUBLIC src)
target_link_libraries(cg_resolver PUBLIC cg_core)
if(MSVC)
    target_compile_options(cg_resolver PRIVATE /W4 /WX)
endif()
set_target_properties(cg_resolver PROPERTIES
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# Query library
add_library(cg_query STATIC
    src/query/QueryEngine.cpp
)
target_include_directories(cg_query PUBLIC src)
target_link_libraries(cg_query PUBLIC cg_core)
if(MSVC)
    target_compile_options(cg_query PRIVATE /W4 /WX)
endif()
set_target_properties(cg_query PROPERTIES
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# Export library
add_library(cg_export STATIC
    src/export/MermaidExporter.cpp
    src/export/DotExporter.cpp
    src/export/DrawioExporter.cpp
    src/export/JsonExporter.cpp
)
target_include_directories(cg_export PUBLIC src)
target_link_libraries(cg_export PUBLIC cg_core cg_query)
if(MSVC)
    target_compile_options(cg_export PRIVATE /W4 /WX)
endif()
set_target_properties(cg_export PROPERTIES
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

# UI + QML resource file
set(QML_SOURCES
    src/qml/Main.qml
    src/qml/SearchPane.qml
    src/qml/FocusPane.qml
    src/qml/DetailsPane.qml
    src/qml/EdgeKindBadge.qml
    src/qml/ConfidenceBadge.qml
)

qt_add_executable(CallGraph
    src/main.cpp
    src/ui/MainController.cpp
    src/ui/SearchModel.cpp
    src/ui/EdgeListModel.cpp
    src/ui/IndexWorker.cpp
    src/ui/AppSettings.cpp
)

qt_add_qml_module(CallGraph
    URI "CallGraph"
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
)

target_include_directories(CallGraph PRIVATE src)

target_link_libraries(CallGraph PRIVATE
    cg_core
    cg_discovery
    cg_parser
    cg_resolver
    cg_query
    cg_export
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
)

if(MSVC)
    target_compile_options(CallGraph PRIVATE /W4 /WX)
    set_target_properties(CallGraph PROPERTIES
        WIN32_EXECUTABLE TRUE
        MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
    )
endif()

# Copy QML query .scm files next to the executable
file(GLOB QUERY_FILES "src/parser/queries/*.scm")
foreach(QUERY_FILE ${QUERY_FILES})
    get_filename_component(QUERY_FILENAME ${QUERY_FILE} NAME)
    add_custom_command(TARGET CallGraph POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${QUERY_FILE}
            $<TARGET_FILE_DIR:CallGraph>/queries/${QUERY_FILENAME}
    )
endforeach()

# Tests
enable_testing()
add_subdirectory(tests)
