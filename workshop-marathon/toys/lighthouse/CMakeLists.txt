add_library(lighthouse-lamp SHARED lamp.cpp)
target_link_libraries(lighthouse-lamp PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(lighthouse-lamp PRIVATE -fno-gnu-unique)
set_target_properties(lighthouse-lamp PROPERTIES PREFIX "")

# The same source with the one-character "code edit" already applied — the
# two-heights witness reloads the running lamp from this artifact and pins
# that the glyph changed while the sweep count survived.
add_library(lighthouse-lamp-star SHARED lamp.cpp)
target_link_libraries(lighthouse-lamp-star PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(lighthouse-lamp-star PRIVATE -fno-gnu-unique)
target_compile_definitions(lighthouse-lamp-star PRIVATE LIGHTHOUSE_STAR)
set_target_properties(lighthouse-lamp-star PROPERTIES PREFIX "")
