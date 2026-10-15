include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ICEExpected.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ICEUri.cmake")
get_filename_component(ICE_DATA_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(ICEDataRuntime STATIC
    "${ICE_DATA_ROOT}/engine/src/data/json.cpp"
    "${ICE_DATA_ROOT}/engine/src/data/binary.cpp"
    "${ICE_DATA_ROOT}/engine/src/data/schema.cpp"
    "${ICE_DATA_ROOT}/engine/src/data/graph.cpp"
    "${ICE_DATA_ROOT}/engine/src/data/activation.cpp")
add_library(ice::data ALIAS ICEDataRuntime)
target_include_directories(ICEDataRuntime PUBLIC "${ICE_DATA_ROOT}/engine/include")
target_link_libraries(ICEDataRuntime PUBLIC ice::expected ice::uri)
target_compile_features(ICEDataRuntime PUBLIC cxx_std_${ICE_CXX_STANDARD})
ice_strict_target(ICEDataRuntime)
