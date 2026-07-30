# The lab's suite. It needs a Loom that can host .so weaves, for the same reason
# the kitchen does: every case here drives real libraries through the real kernel.
if(NOT TARGET loom::kernel)
    message(STATUS "night-lab: tests skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

add_executable(night-lab-tests test_kitchen.cpp)
target_include_directories(night-lab-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                   ${CMAKE_SOURCE_DIR})
target_link_libraries(night-lab-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            night-lab-warnings)
target_compile_definitions(night-lab-tests
    PRIVATE NIGHT_LAB_RUNTIME_DIR="${NIGHT_LAB_RUNTIME_DIR}")
set_target_properties(night-lab-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${NIGHT_LAB_RUNTIME_DIR})

# The suite loads the weaves it tests, so it must not run before they exist.
add_dependencies(night-lab-tests
    kitchen-expediter kitchen-policy-house kitchen-policy-rush
    kitchen-grill kitchen-grill-2 kitchen-fryer)

add_test(NAME kitchen COMMAND night-lab-tests)
