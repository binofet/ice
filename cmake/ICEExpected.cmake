include_guard(GLOBAL)
add_library(ICEExpected INTERFACE)
add_library(ice::expected ALIAS ICEExpected)
# Keep the public result type identical in C++20 and C++23, including older SDK libraries.
target_include_directories(ICEExpected SYSTEM INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/../engine/3rdparty/expected/include")
