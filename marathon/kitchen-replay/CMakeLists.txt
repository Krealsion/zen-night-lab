# Project 1 — the kitchen replay.
#
# One runtime directory for the project: the weaves, the demo host, the suite and
# the vendored Timer services all land here, so anything that loads a weave
# resolves it beside its own binary.
set(KITCHEN_RUNTIME_DIR ${CMAKE_BINARY_DIR}/kitchen-replay)

marathon_weave(kitchen-expediter expediter.cpp)
target_link_libraries(kitchen-expediter PRIVATE zengine-timer-vocabulary)

# Routing policy: one source, two libraries. Swapping one for the other changes
# where an identical order goes, with nothing else in the tree altered.
marathon_weave(kitchen-policy-house policy.cpp)
marathon_weave(kitchen-policy-rush policy.cpp)
target_compile_definitions(kitchen-policy-rush PRIVATE KITCHEN_POLICY_RUSH)

# Stations: one source, three libraries. kitchen-grill-2 is byte-for-byte the
# same rules as kitchen-grill under a different library name — it is the
# SUCCESSOR in both replacement ceremonies, and being identical is the point:
# what crosses is the arrangement, not the code.
marathon_weave(kitchen-grill station.cpp)
target_link_libraries(kitchen-grill PRIVATE zengine-timer-vocabulary)

marathon_weave(kitchen-grill-2 station.cpp)
target_compile_definitions(kitchen-grill-2 PRIVATE KITCHEN_STATION_LABEL="b")
target_link_libraries(kitchen-grill-2 PRIVATE zengine-timer-vocabulary)

marathon_weave(kitchen-grill-3 station.cpp)
target_compile_definitions(kitchen-grill-3 PRIVATE KITCHEN_STATION_LABEL="c")
target_link_libraries(kitchen-grill-3 PRIVATE zengine-timer-vocabulary)

marathon_weave(kitchen-fryer station.cpp)
target_compile_definitions(kitchen-fryer PRIVATE KITCHEN_STATION_FRYER)
target_link_libraries(kitchen-fryer PRIVATE zengine-timer-vocabulary)

# The fryer again, under a name nothing has loaded. It is the candidate offered
# for the GRILL's role in the authentic-refusal case: a real artifact that loads,
# seals, is asked — and then says no, because it knows it is the fryer. Building
# it as a separate library rather than re-loading `kitchen-fryer` keeps the
# refusal about the candidate's own judgement instead of about a name collision.
marathon_weave(kitchen-fryer-candidate station.cpp)
target_compile_definitions(kitchen-fryer-candidate PRIVATE KITCHEN_STATION_FRYER)
target_link_libraries(kitchen-fryer-candidate PRIVATE zengine-timer-vocabulary)

set(kitchen_weaves
    kitchen-expediter kitchen-policy-house kitchen-policy-rush
    kitchen-grill kitchen-grill-2 kitchen-grill-3 kitchen-fryer kitchen-fryer-candidate)

# The answer-seam re-test: one source, three weave libraries differing only in
# which door they answer through, plus the host that watches the bus.
marathon_weave(repro-answer-direct repro_answer_seam.cpp)
target_compile_definitions(repro-answer-direct PRIVATE REPRO_WEAVE REPRO_DIRECT)
marathon_weave(repro-answer-deferred repro_answer_seam.cpp)
target_compile_definitions(repro-answer-deferred PRIVATE REPRO_WEAVE)
marathon_weave(repro-answer-twice repro_answer_seam.cpp)
target_compile_definitions(repro-answer-twice PRIVATE REPRO_WEAVE REPRO_TWICE)

add_executable(repro-answer-seam repro_answer_seam.cpp)
target_link_libraries(repro-answer-seam
    PRIVATE loom::kernel loom::switchboard loom::core marathon-warnings)

list(APPEND kitchen_weaves repro-answer-direct repro-answer-deferred repro-answer-twice)

set_target_properties(${kitchen_weaves} repro-answer-seam PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${KITCHEN_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${KITCHEN_RUNTIME_DIR})

# The vendored Timer services, placed beside the kitchen's own weaves.
#   zengine-timer          the shipped one, over the real monotonic clock —
#                          what the demo host runs on.
#   zengine-timer-virtual  the Timer package's own suite artifact, over a clock
#                          that only moves inside a nap — what the tests run on,
#                          so every deadline here is an exact integer nobody
#                          waited for.
file(COPY ${MARATHON_ZENGINE_LIB}/zengine-timer.so
          ${MARATHON_ZENGINE_LIB}/zengine-timer-virtual.so
     DESTINATION ${KITCHEN_RUNTIME_DIR})

# The usage example: a scripted, non-interactive service run on the real clock.
add_executable(kitchen-demo demo.cpp)
target_link_libraries(kitchen-demo
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            marathon-warnings)
target_compile_definitions(kitchen-demo
    PRIVATE MARATHON_KITCHEN_RUNTIME_DIR="${KITCHEN_RUNTIME_DIR}")
set_target_properties(kitchen-demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${KITCHEN_RUNTIME_DIR})

# The suite.
add_executable(kitchen-tests test_kitchen.cpp)
target_include_directories(kitchen-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kitchen-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            marathon-doctest marathon-warnings)
target_compile_definitions(kitchen-tests
    PRIVATE MARATHON_KITCHEN_RUNTIME_DIR="${KITCHEN_RUNTIME_DIR}")
set_target_properties(kitchen-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${KITCHEN_RUNTIME_DIR})

# The suite loads the weaves it tests, so it must not run before they exist.
add_dependencies(kitchen-tests ${kitchen_weaves})
add_dependencies(repro-answer-seam repro-answer-direct repro-answer-deferred repro-answer-twice)

add_test(NAME kitchen COMMAND kitchen-tests)
add_test(NAME kitchen-answer-seam COMMAND repro-answer-seam)
