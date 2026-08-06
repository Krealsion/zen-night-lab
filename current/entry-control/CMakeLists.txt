cmake_minimum_required(VERSION 3.22)
project(entry-control CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The whole Zen-facing part of this build. An installed prefix, three imported
# targets, and one call per loadable artifact.
find_package(loom REQUIRED)
message(STATUS "entry-control: Loom from ${loom_DIR}")

if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "entry-control: this Loom install cannot host loadable weaves (no loom::kernel), "
        "and a wearer whose breathing the board cannot inspect is not a wearer")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# The watch. Six separately built people who share a vocabulary and nothing
# else — six artifacts compiled against one header, which is exactly the
# situation loom_weave_build_contract() exists for.
set(WATCH aish ndlovu farrow teague okonkwo braddock)

foreach(wearer IN LISTS WATCH)
    add_library(${wearer} SHARED wearer_${wearer}.cpp)
    set_target_properties(${wearer} PROPERTIES PREFIX "")
    target_link_libraries(${wearer} PRIVATE loom::core loom::switchboard)
    loom_weave_build_contract(${wearer})
endforeach()

add_executable(incident incident.cpp)
target_link_libraries(incident PRIVATE loom::core loom::switchboard loom::kernel)

enable_testing()

# The order matters: the incident hands them out in watch order.
set(WEARERS
    $<TARGET_FILE:aish>
    $<TARGET_FILE:ndlovu>
    $<TARGET_FILE:farrow>
    $<TARGET_FILE:teague>
    $<TARGET_FILE:okonkwo>
    $<TARGET_FILE:braddock>)

add_test(NAME working-incident COMMAND incident ${WEARERS})
add_test(NAME control-out-on-the-radio COMMAND incident ${WEARERS} --out-on-the-radio)
add_test(NAME control-straight-in COMMAND incident ${WEARERS} --straight-in)
