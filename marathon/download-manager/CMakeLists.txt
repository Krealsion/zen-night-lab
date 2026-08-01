# Project 2 — the download manager.
set(DOWNLOADS_RUNTIME_DIR ${CMAKE_BINARY_DIR}/download-manager)

# ONE SOURCE, FOUR LIBRARIES. The pair that matters is the two STYLES: whether
# the service spends its one authenticated answer on the acknowledgment or holds
# it for the whole operation. Each style also gets a `-v2` build so a real
# prepared replacement has a genuinely different artifact to admit.
marathon_weave(download-service service.cpp)
target_link_libraries(download-service PRIVATE zengine-timer-vocabulary)

marathon_weave(download-service-v2 service.cpp)
target_compile_definitions(download-service-v2 PRIVATE DOWNLOAD_SERVICE_LABEL="v2")
target_link_libraries(download-service-v2 PRIVATE zengine-timer-vocabulary)

marathon_weave(download-service-holds service.cpp)
target_compile_definitions(download-service-holds PRIVATE DOWNLOAD_HOLD_THE_ANSWER)
target_link_libraries(download-service-holds PRIVATE zengine-timer-vocabulary)

marathon_weave(download-service-holds-v2 service.cpp)
target_compile_definitions(download-service-holds-v2
    PRIVATE DOWNLOAD_HOLD_THE_ANSWER DOWNLOAD_SERVICE_LABEL="v2")
target_link_libraries(download-service-holds-v2 PRIVATE zengine-timer-vocabulary)

set(downloads_weaves
    download-service download-service-v2 download-service-holds download-service-holds-v2)

set_target_properties(${downloads_weaves} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${DOWNLOADS_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${DOWNLOADS_RUNTIME_DIR})

file(COPY ${MARATHON_ZENGINE_LIB}/zengine-timer-virtual.so
     DESTINATION ${DOWNLOADS_RUNTIME_DIR})

add_executable(downloads-tests test_downloads.cpp)
target_include_directories(downloads-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(downloads-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            marathon-doctest marathon-warnings)
target_compile_definitions(downloads-tests
    PRIVATE MARATHON_DOWNLOADS_RUNTIME_DIR="${DOWNLOADS_RUNTIME_DIR}")
set_target_properties(downloads-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${DOWNLOADS_RUNTIME_DIR})

add_dependencies(downloads-tests ${downloads_weaves})

add_test(NAME downloads COMMAND downloads-tests)
