cmake_minimum_required(VERSION 3.13)
project(LLVM_PASS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(DEF_USE_INPUT  "def_use.dot")
set(DEF_USE_OUTPUT "def_use.png")

set(CONTROL_FLOW_INPUT  "control_flow.dot")
set(CONTROL_FLOW_OUTPUT "control_flow.png")

add_library(Pass MODULE src/Pass/Pass.cpp src/Pass/Graphviz.cpp src/Pass/Util.cpp)

target_include_directories(Pass PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(Pass PROPERTIES
  CXX_STANDARD 20
  POSITION_INDEPENDENT_CODE ON
)

llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(Pass PRIVATE ${llvm_libs})

target_include_directories(Pass PRIVATE ${LLVM_INCLUDE_DIRS})

add_custom_command(
    OUTPUT a.out
    COMMAND clang++ -fpass-plugin=$<TARGET_FILE:Pass> ${CMAKE_CURRENT_SOURCE_DIR}/c_examples/fact.c ${CMAKE_CURRENT_SOURCE_DIR}/src/Pass/FOR_LLVM_Log.cpp -I${CMAKE_CURRENT_SOURCE_DIR}/include -O0 -o a.out
    DEPENDS Pass ${CMAKE_CURRENT_SOURCE_DIR}/c_examples/fact.c ${CMAKE_CURRENT_SOURCE_DIR}/src/Pass/FOR_LLVM_Log.cpp
    COMMENT 
    VERBATIM
)

add_custom_target(
    run_test ALL
    DEPENDS a.out
)

add_custom_target(gen_pngs ALL DEPENDS ${PNG_FILES})

add_executable(ConcatCF src/Scripts/ConcatControlFlow.cpp)
add_executable(ConcatDU src/Scripts/ConcatDefUse.cpp)

