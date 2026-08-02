# Replay B — the build farm. One worker artifact, loaded twice: once holding
# the office, once as the rogue with the same toolkit minus the office.
set(FARM_RUNTIME_DIR ${CMAKE_BINARY_DIR}/build-farm)

followup_weave(farm-worker worker.cpp)
set_target_properties(farm-worker PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${FARM_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${FARM_RUNTIME_DIR})

add_executable(farm-replay-tests test_farm.cpp)
target_include_directories(farm-replay-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(farm-replay-tests
    PRIVATE loom::kernel loom::switchboard loom::core followup-doctest followup-warnings)
target_compile_definitions(farm-replay-tests PRIVATE
    "FARM_SO_WORKER=\"$<TARGET_FILE:farm-worker>\"")
add_dependencies(farm-replay-tests farm-worker)
add_test(NAME farm-replay COMMAND farm-replay-tests --no-skipped-summary)
