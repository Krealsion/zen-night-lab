cmake_minimum_required(VERSION 3.22)
project(saleroom CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The whole Zen-facing part of this build. An installed prefix, three imported
# targets, and one call per loadable artifact.
find_package(loom REQUIRED)
message(STATUS "saleroom: Loom from ${loom_DIR}")

if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "saleroom: this Loom install cannot host loadable weaves (no loom::kernel), "
        "and a bidder whose limit the saleroom can read is not a bidder")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# The room. Four separately built people who share a vocabulary and nothing
# else — four artifacts compiled against one header, which is exactly the
# situation loom_weave_build_contract() exists for.
set(ROOM paddle-3 paddle-7 paddle-11 paddle-14)
set(paddle-3_SRC bidder_ledbury.cpp)
set(paddle-7_SRC bidder_kestrel.cpp)
set(paddle-11_SRC bidder_selwood.cpp)
set(paddle-14_SRC bidder_hallam.cpp)

foreach(bidder IN LISTS ROOM)
    add_library(${bidder} SHARED ${${bidder}_SRC})
    set_target_properties(${bidder} PROPERTIES PREFIX "")
    target_link_libraries(${bidder} PRIVATE loom::core loom::switchboard)
    loom_weave_build_contract(${bidder})
endforeach()

add_executable(sale sale.cpp)
target_link_libraries(sale PRIVATE loom::core loom::switchboard loom::kernel)

enable_testing()

set(PADDLES
    $<TARGET_FILE:paddle-3>
    $<TARGET_FILE:paddle-7>
    $<TARGET_FILE:paddle-11>
    $<TARGET_FILE:paddle-14>)

add_test(NAME sale-day COMMAND sale ${PADDLES})
add_test(NAME control-take-the-board-as-read
         COMMAND sale ${PADDLES} --take-the-board-as-read)
add_test(NAME control-run-the-book-up
         COMMAND sale ${PADDLES} --run-the-book-up)
