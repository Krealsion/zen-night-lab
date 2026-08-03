add_library(gremlin-liar SHARED gremlin.cpp)
target_link_libraries(gremlin-liar PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(gremlin-liar PRIVATE -fno-gnu-unique)
set_target_properties(gremlin-liar PROPERTIES PREFIX "")

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
