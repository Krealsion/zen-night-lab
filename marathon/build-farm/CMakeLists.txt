# Project 3 — the build farm.
set(FARM_RUNTIME_DIR ${CMAKE_BINARY_DIR}/build-farm)

marathon_weave(farm-dispatcher dispatcher.cpp)
target_link_libraries(farm-dispatcher PRIVATE zengine-timer-vocabulary)

# One worker source, four libraries. `farm-worker-a2` is the SUCCESSOR: the same
# rules under a different library name, which is the point — what crosses a
# replacement here is the intent, not the code and not the progress.
# `farm-worker-a-clang` is the same slot built for a toolchain the farm does not
# run, so a candidate has something honest to refuse about.
marathon_weave(farm-worker-a worker.cpp)
target_compile_definitions(farm-worker-a PRIVATE FARM_WORKER_NAME="a")
target_link_libraries(farm-worker-a PRIVATE zengine-timer-vocabulary)

marathon_weave(farm-worker-a2 worker.cpp)
target_compile_definitions(farm-worker-a2 PRIVATE FARM_WORKER_NAME="a" FARM_WORKER_LABEL="gen2")
target_link_libraries(farm-worker-a2 PRIVATE zengine-timer-vocabulary)

marathon_weave(farm-worker-a-clang worker.cpp)
target_compile_definitions(farm-worker-a-clang
    PRIVATE FARM_WORKER_NAME="a" FARM_WORKER_LABEL="clang" FARM_WORKER_TOOLCHAIN="clang")
target_link_libraries(farm-worker-a-clang PRIVATE zengine-timer-vocabulary)

marathon_weave(farm-worker-b worker.cpp)
target_compile_definitions(farm-worker-b PRIVATE FARM_WORKER_NAME="b")
target_link_libraries(farm-worker-b PRIVATE zengine-timer-vocabulary)

set(farm_weaves farm-dispatcher farm-worker-a farm-worker-a2 farm-worker-a-clang farm-worker-b)

set_target_properties(${farm_weaves} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${FARM_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${FARM_RUNTIME_DIR})

file(COPY ${MARATHON_ZENGINE_LIB}/zengine-timer-virtual.so DESTINATION ${FARM_RUNTIME_DIR})

add_executable(farm-tests test_farm.cpp)
target_include_directories(farm-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(farm-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            marathon-doctest marathon-warnings)
target_compile_definitions(farm-tests PRIVATE MARATHON_FARM_RUNTIME_DIR="${FARM_RUNTIME_DIR}")
set_target_properties(farm-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FARM_RUNTIME_DIR})

add_dependencies(farm-tests ${farm_weaves})

add_test(NAME build-farm COMMAND farm-tests)
