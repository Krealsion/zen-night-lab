add_library(pond-firefly SHARED firefly.cpp)
target_link_libraries(pond-firefly PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(pond-firefly PRIVATE -fno-gnu-unique)
set_target_properties(pond-firefly PROPERTIES PREFIX "")

add_library(pond-canvas SHARED canvas.cpp)
target_link_libraries(pond-canvas PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(pond-canvas PRIVATE -fno-gnu-unique)
set_target_properties(pond-canvas PROPERTIES PREFIX "")
