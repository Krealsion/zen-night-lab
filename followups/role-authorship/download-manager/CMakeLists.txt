# Replay C — the download manager. One service; the finding is what its source
# does NOT contain (a deferred answer held for the transfer's duration).
set(DL_RUNTIME_DIR ${CMAKE_BINARY_DIR}/download-manager)

followup_weave(dl-service service.cpp)
set_target_properties(dl-service PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${DL_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${DL_RUNTIME_DIR})

add_executable(dl-replay-tests test_download.cpp)
target_include_directories(dl-replay-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dl-replay-tests
    PRIVATE loom::kernel loom::switchboard loom::core followup-doctest followup-warnings)
target_compile_definitions(dl-replay-tests PRIVATE
    "DL_SO_SERVICE=\"$<TARGET_FILE:dl-service>\"")
add_dependencies(dl-replay-tests dl-service)
add_test(NAME download-replay COMMAND dl-replay-tests --no-skipped-summary)
