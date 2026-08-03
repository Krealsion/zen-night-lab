add_library(scribe-pad SHARED pad.cpp)
target_link_libraries(scribe-pad PRIVATE loom::switchboard loom::core zengine-vocab)
target_compile_options(scribe-pad PRIVATE -fno-gnu-unique)
set_target_properties(scribe-pad PROPERTIES PREFIX "")
