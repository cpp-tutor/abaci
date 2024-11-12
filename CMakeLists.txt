cmake_minimum_required(VERSION 3.25.1)
project(Abaci)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

include(CheckIncludeFileCXX)
if (ABACI_USE_STD_FORMAT)
    check_include_file_cxx("print" HAVE_STD_PRINT)
    check_include_file_cxx("format" HAVE_STD_FORMAT)
    if (NOT HAVE_STD_PRINT OR NOT HAVE_STD_FORMAT)
        message(FATAL_ERROR "Cannot find <print> and <format> headers for use with -DABACI_USE_STD_FORMAT=1")
    endif()
    add_definitions(-DABACI_USE_STD_FORMAT)
else()
    find_package(fmt REQUIRED)
endif()

find_package(Boost COMPONENTS unit_test_framework)

if (Boost_FOUND OR Boost_INCLUDE_DIRS)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if (ABACI_USE_OLDER_BOOST)
    add_definitions(-DABACI_USE_OLDER_BOOST)
endif()

set(SOURCES
    src/lib/Abaci.cpp
    src/utility/Constant.cpp
    src/utility/Operator.cpp
    src/utility/Symbol.cpp
    src/utility/Type.cpp
    src/utility/Utility.cpp
    src/parser/Parse.cpp
    src/codegen/ExprCodeGen.cpp
    src/codegen/StmtCodeGen.cpp
    src/codegen/TypeCodeGen.cpp
    src/engine/Cache.cpp
    src/engine/JIT.cpp
)

add_executable(abaci ${SOURCES} src/main.cpp)

llvm_map_components_to_libnames(LLVM_LIBS core executionengine interpreter analysis native bitwriter orcjit)
target_link_libraries(abaci ${LLVM_LIBS})

if (NOT ABACI_USE_STD_FORMAT)
    target_link_libraries(abaci fmt::fmt)
endif()

set(TEST_SOURCES
    testing/TestJIT.cpp
    testing/TestJITImpl.cpp
    testing/block.cpp
    testing/class.cpp
    testing/expression.cpp
    testing/function.cpp
    testing/method.cpp
    testing/print.cpp
)

add_executable(abacitests EXCLUDE_FROM_ALL ${SOURCES} ${TEST_SOURCES})
target_link_libraries(abacitests ${LLVM_LIBS} Boost::unit_test_framework)
if (NOT ABACI_USE_STD_FORMAT)
    target_link_libraries(abacitests fmt::fmt)
endif()

add_custom_target(test abacitests)
