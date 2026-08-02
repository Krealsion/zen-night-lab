add_library(lighthouse-lamp SHARED lamp.cpp)
target_link_libraries(lighthouse-lamp PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(lighthouse-lamp PRIVATE -fno-gnu-unique)
set_target_properties(lighthouse-lamp PROPERTIES PREFIX "")
