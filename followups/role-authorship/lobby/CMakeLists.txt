# Replay A — the lobby. One matchmaker source, two real artifacts (a genuine
# replacement needs a genuinely different .so), and no pull variant at all —
# its absence is the finding.
set(LOBBY_RUNTIME_DIR ${CMAKE_BINARY_DIR}/lobby)

followup_weave(lobby-matchmaker matchmaker.cpp)
followup_weave(lobby-matchmaker-v2 matchmaker.cpp)
target_compile_definitions(lobby-matchmaker-v2 PRIVATE MATCHMAKER_LABEL="v2")

set_target_properties(lobby-matchmaker lobby-matchmaker-v2 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LOBBY_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${LOBBY_RUNTIME_DIR})

add_executable(lobby-replay-tests test_lobby.cpp)
target_include_directories(lobby-replay-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lobby-replay-tests
    PRIVATE loom::kernel loom::switchboard loom::core followup-doctest followup-warnings)
target_compile_definitions(lobby-replay-tests PRIVATE
    "LOBBY_SO_MATCHMAKER_V1=\"$<TARGET_FILE:lobby-matchmaker>\""
    "LOBBY_SO_MATCHMAKER_V2=\"$<TARGET_FILE:lobby-matchmaker-v2>\"")
add_dependencies(lobby-replay-tests lobby-matchmaker lobby-matchmaker-v2)
add_test(NAME lobby-replay COMMAND lobby-replay-tests --no-skipped-summary)
