# The Workshop: one special shell, ordinary loadable weaves for the rest.

# Loadable weaves build with -fno-gnu-unique (the law that keeps dlclose real)
# and no "lib" prefix, so a stem resolves to exactly "<stem>.so".

add_library(workshop-registry SHARED registry.cpp)
target_link_libraries(workshop-registry PRIVATE loom::switchboard loom::core)
target_compile_options(workshop-registry PRIVATE -fno-gnu-unique)
set_target_properties(workshop-registry PROPERTIES PREFIX "")

add_library(workshop-skin-log SHARED skin_log.cpp)
target_link_libraries(workshop-skin-log PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(workshop-skin-log PRIVATE -fno-gnu-unique)
set_target_properties(workshop-skin-log PROPERTIES PREFIX "")

add_executable(workshop shell.cpp)
target_link_libraries(workshop PRIVATE loom::kernel loom::switchboard loom::core zengine-vocab)
target_compile_definitions(workshop PRIVATE
    WORKSHOP_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    WORKSHOP_BINARY_DIR="${CMAKE_BINARY_DIR}")
