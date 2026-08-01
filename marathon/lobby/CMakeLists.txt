# Project 5 — the lobby / matchmaking service.
set(LOBBY_RUNTIME_DIR ${CMAKE_BINARY_DIR}/lobby)

marathon_weave(lobby-registry registry.cpp)
target_link_libraries(lobby-registry PRIVATE zengine-timer-vocabulary)

# ONE SOURCE, FOUR LIBRARIES, and the pair that matters is the two STYLES:
# whether the office ANNOUNCES a match (nothing to attest) or ANSWERS a request
# for one (attestable, and expensive). Each style also gets a `-v2` so a real
# prepared replacement has a genuinely different artifact to admit.
marathon_weave(lobby-matchmaker-push matchmaker.cpp)
target_link_libraries(lobby-matchmaker-push PRIVATE zengine-timer-vocabulary)

marathon_weave(lobby-matchmaker-push-v2 matchmaker.cpp)
target_compile_definitions(lobby-matchmaker-push-v2 PRIVATE MATCHMAKER_LABEL="v2")
target_link_libraries(lobby-matchmaker-push-v2 PRIVATE zengine-timer-vocabulary)

marathon_weave(lobby-matchmaker-pull matchmaker.cpp)
target_compile_definitions(lobby-matchmaker-pull PRIVATE MATCHMAKER_PULL)
target_link_libraries(lobby-matchmaker-pull PRIVATE zengine-timer-vocabulary)

marathon_weave(lobby-matchmaker-pull-v2 matchmaker.cpp)
target_compile_definitions(lobby-matchmaker-pull-v2 PRIVATE MATCHMAKER_PULL MATCHMAKER_LABEL="v2")
target_link_libraries(lobby-matchmaker-pull-v2 PRIVATE zengine-timer-vocabulary)

set(lobby_weaves lobby-registry lobby-matchmaker-push lobby-matchmaker-push-v2
                 lobby-matchmaker-pull lobby-matchmaker-pull-v2)

set_target_properties(${lobby_weaves} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LOBBY_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${LOBBY_RUNTIME_DIR})

file(COPY ${MARATHON_ZENGINE_LIB}/zengine-timer-virtual.so DESTINATION ${LOBBY_RUNTIME_DIR})

add_executable(lobby-tests test_lobby.cpp)
target_include_directories(lobby-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lobby-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            marathon-doctest marathon-warnings)
target_compile_definitions(lobby-tests PRIVATE MARATHON_LOBBY_RUNTIME_DIR="${LOBBY_RUNTIME_DIR}")
set_target_properties(lobby-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LOBBY_RUNTIME_DIR})

add_dependencies(lobby-tests ${lobby_weaves})

add_test(NAME lobby COMMAND lobby-tests)
