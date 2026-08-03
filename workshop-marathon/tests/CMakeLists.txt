add_executable(workshop-tests test_workshop.cpp)
target_link_libraries(workshop-tests PRIVATE loom::kernel loom::switchboard loom::core
                                             zengine-vocab)
target_include_directories(workshop-tests PRIVATE ${CMAKE_SOURCE_DIR}/workshop
                                                  ${CMAKE_SOURCE_DIR}/toys)

add_test(NAME workshop
         COMMAND workshop-tests
                 ${CMAKE_BINARY_DIR}/workshop
                 ${CMAKE_SOURCE_DIR}/vendor/zengine/lib
                 ${CMAKE_BINARY_DIR}/toys/lighthouse
                 ${CMAKE_BINARY_DIR}/toys/pond)
