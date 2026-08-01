# The job kitchen — three roles, six libraries, one vocabulary.
#
# Everything here needs a Loom that can HOST .so weaves. The honest question is
# "does this install export loom::kernel?" — it does on Linux, and deliberately
# does not on Windows unless the explicit development/demo backend is enabled.
if(NOT TARGET loom::kernel)
    message(STATUS "night-lab: kitchen skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

# One runtime directory for the whole lab: the weaves, the demo host, the suite,
# and the vendored Timer service all land here, so anything that loads a weave
# resolves it beside its own binary. Deployment is the host's concern.
set(NIGHT_LAB_RUNTIME_DIR ${CMAKE_BINARY_DIR}/kitchen CACHE INTERNAL "")

night_lab_weave(kitchen-expediter expediter.cpp)
target_link_libraries(kitchen-expediter PRIVATE zengine-timer-vocabulary)

# Routing policy: one source, two libraries. Swapping one for the other changes
# where an identical order goes, with nothing else in the tree altered.
night_lab_weave(kitchen-policy-house policy.cpp)
night_lab_weave(kitchen-policy-rush policy.cpp)
target_compile_definitions(kitchen-policy-rush PRIVATE KITCHEN_POLICY_RUSH)

# Stations: one source, three libraries. kitchen-grill-2 is byte-for-byte the
# same rules as kitchen-grill under a different library name — it is the HEIR in
# the graceful-replacement case, and being identical is the point: what crosses
# is the letter, not the code.
night_lab_weave(kitchen-grill station.cpp)
target_link_libraries(kitchen-grill PRIVATE zengine-timer-vocabulary)

night_lab_weave(kitchen-grill-2 station.cpp)
target_compile_definitions(kitchen-grill-2 PRIVATE KITCHEN_STATION_LABEL="b")
target_link_libraries(kitchen-grill-2 PRIVATE zengine-timer-vocabulary)

night_lab_weave(kitchen-fryer station.cpp)
target_compile_definitions(kitchen-fryer PRIVATE KITCHEN_STATION_FRYER)
target_link_libraries(kitchen-fryer PRIVATE zengine-timer-vocabulary)

set(night_lab_kitchen_weaves
    kitchen-expediter kitchen-policy-house kitchen-policy-rush
    kitchen-grill kitchen-grill-2 kitchen-fryer)
set_target_properties(${night_lab_kitchen_weaves} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${NIGHT_LAB_RUNTIME_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${NIGHT_LAB_RUNTIME_DIR})

# The vendored Timer service, placed beside the kitchen's own weaves. Two
# builds of the SAME service over two clocks:
#   zengine-timer          the shipped one, over the real monotonic clock —
#                          what the demo host runs on.
#   zengine-timer-virtual  the Timer package's own suite artifact, over a clock
#                          that only moves inside a nap — what the tests run on,
#                          so every deadline in this lab is an exact integer
#                          nobody had to wait for.
file(COPY ${NIGHT_LAB_ZENGINE_LIB}/zengine-timer.so
          ${NIGHT_LAB_ZENGINE_LIB}/zengine-timer-virtual.so
     DESTINATION ${NIGHT_LAB_RUNTIME_DIR})

# The usage example: a scripted, non-interactive service run on the real clock.
add_executable(kitchen-demo demo.cpp)
target_link_libraries(kitchen-demo
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            night-lab-warnings)
set_target_properties(kitchen-demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${NIGHT_LAB_RUNTIME_DIR})
