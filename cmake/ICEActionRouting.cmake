include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ICEStandards.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEExpected.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEPlatform.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEInput.cmake")

get_filename_component(ICE_ACTION_ROUTING_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(ICEActionRouterRuntime STATIC "${ICE_ACTION_ROUTING_ROOT}/engine/src/input/action_router.cpp")
add_library(ice::action_router ALIAS ICEActionRouterRuntime)
target_link_libraries(ICEActionRouterRuntime PUBLIC ice::input)
set_target_properties(ICEActionRouterRuntime PROPERTIES
    POSITION_INDEPENDENT_CODE ON XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
ice_strict_target(ICEActionRouterRuntime)
