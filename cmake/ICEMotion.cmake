include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/ICEStandards.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEMath.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEExpected.cmake")

get_filename_component(ICE_MOTION_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(ICEMotion STATIC "${ICE_MOTION_ROOT}/engine/src/motion/pose.cpp")
add_library(ice::motion ALIAS ICEMotion)
target_include_directories(ICEMotion PUBLIC "${ICE_MOTION_ROOT}/engine/include")
target_link_libraries(ICEMotion PUBLIC ice::math ice::expected)
ice_strict_target(ICEMotion)
