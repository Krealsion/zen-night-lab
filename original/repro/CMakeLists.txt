# The smallest concrete reproducer for the one core seam this lab found.
# One source, three targets: two weave libraries that differ only in which door
# they answer through, and the host that watches the bus.
if(NOT TARGET loom::kernel)
    message(STATUS "night-lab: repro skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

night_lab_weave(repro-answer-direct answer_seam.cpp)
target_compile_definitions(repro-answer-direct PRIVATE REPRO_WEAVE REPRO_DIRECT)

night_lab_weave(repro-answer-deferred answer_seam.cpp)
target_compile_definitions(repro-answer-deferred PRIVATE REPRO_WEAVE)

add_executable(repro-answer-seam answer_seam.cpp)
target_link_libraries(repro-answer-seam
    PRIVATE loom::kernel loom::switchboard loom::core night-lab-warnings)

set_target_properties(repro-answer-direct repro-answer-deferred repro-answer-seam
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${NIGHT_LAB_RUNTIME_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${NIGHT_LAB_RUNTIME_DIR})

add_test(NAME answer-seam COMMAND repro-answer-seam)
