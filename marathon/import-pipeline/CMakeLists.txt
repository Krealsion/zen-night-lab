# Project 4 — the media/import pipeline.
set(IMPORT_RUNTIME_DIR ${CMAKE_BINARY_DIR}/import-pipeline)

# One source, three libraries. `-narrow` ships a DIFFERENT CATALOGUE NAME, so a
# candidate has something honest to refuse about during preparation: two
# generations of an importer may legitimately disagree about what they can read.
marathon_weave(import-pipeline importer.cpp)
target_link_libraries(import-pipeline PRIVATE zengine-timer-vocabulary)

marathon_weave(import-pipeline-v2 importer.cpp)
target_compile_definitions(import-pipeline-v2 PRIVATE MARATHON_IMPORT_LABEL="v2")
target_link_libraries(import-pipeline-v2 PRIVATE zengine-timer-vocabulary)

marathon_weave(import-pipeline-narrow importer.cpp)
target_compile_definitions(import-pipeline-narrow
    PRIVATE MARATHON_IMPORT_LABEL="narrow" MARATHON_IMPORT_CATALOGUE_NAME="archive")
target_link_libraries(import-pipeline-narrow PRIVATE zengine-timer-vocabulary)

set(import_weaves import-pipeline import-pipeline-v2 import-pipeline-narrow)

set_target_properties(${import_weaves} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${IMPORT_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${IMPORT_RUNTIME_DIR})

file(COPY ${MARATHON_ZENGINE_LIB}/zengine-timer-virtual.so DESTINATION ${IMPORT_RUNTIME_DIR})

add_executable(import-tests test_import.cpp)
target_include_directories(import-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(import-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            marathon-doctest marathon-warnings)
target_compile_definitions(import-tests PRIVATE MARATHON_IMPORT_RUNTIME_DIR="${IMPORT_RUNTIME_DIR}")
set_target_properties(import-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${IMPORT_RUNTIME_DIR})

add_dependencies(import-tests ${import_weaves})

add_test(NAME import-pipeline COMMAND import-tests)
