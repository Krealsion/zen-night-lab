# Project 6 — the maintenance scheduler.
set(MAINT_RUNTIME_DIR ${CMAKE_BINARY_DIR}/scheduler)

marathon_weave(maint-scheduler scheduler.cpp)
target_link_libraries(maint-scheduler PRIVATE zengine-timer-vocabulary)

marathon_weave(maint-worker worker.cpp)
target_link_libraries(maint-worker PRIVATE zengine-timer-vocabulary)

marathon_weave(maint-worker-v2 worker.cpp)
target_compile_definitions(maint-worker-v2 PRIVATE MAINT_WORKER_LABEL="v2")
target_link_libraries(maint-worker-v2 PRIVATE zengine-timer-vocabulary)

# A worker that services only part of the fleet, so a candidate has something
# honest to refuse about.
marathon_weave(maint-worker-narrow worker.cpp)
target_compile_definitions(maint-worker-narrow PRIVATE MAINT_WORKER_NARROW)
target_link_libraries(maint-worker-narrow PRIVATE zengine-timer-vocabulary)

set(maint_weaves maint-scheduler maint-worker maint-worker-v2 maint-worker-narrow)

set_target_properties(${maint_weaves} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${MAINT_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${MAINT_RUNTIME_DIR})

# THREE Timer artifacts, because this project replaces the CLOCK:
#   -virtual      the incumbent
#   -virtual-v2   a genuinely different artifact of the same service
#   -declines     one that loads, seals, is asked, and authentically says no
file(COPY ${MARATHON_ZENGINE_LIB}/zengine-timer-virtual.so
          ${MARATHON_ZENGINE_LIB}/zengine-timer-virtual-v2.so
          ${MARATHON_ZENGINE_LIB}/zengine-timer-declines.so
     DESTINATION ${MAINT_RUNTIME_DIR})

add_executable(maint-tests test_scheduler.cpp)
target_include_directories(maint-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(maint-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            marathon-doctest marathon-warnings)
target_compile_definitions(maint-tests PRIVATE MARATHON_MAINT_RUNTIME_DIR="${MAINT_RUNTIME_DIR}")
set_target_properties(maint-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${MAINT_RUNTIME_DIR})

add_dependencies(maint-tests ${maint_weaves})

add_test(NAME scheduler COMMAND maint-tests)
