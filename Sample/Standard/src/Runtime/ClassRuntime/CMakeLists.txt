add_library(LudorkStandardClassRuntime OBJECT
    ClassNativeInterop.cpp
    Bridge.cpp
    Common.cpp
    Native.cpp
    Composite.cpp
    Instance.cpp
    Copy.cpp
    Module.cpp)

set_target_properties(LudorkStandardClassRuntime PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(LudorkStandardClassRuntime PRIVATE cxx_std_20)
target_compile_definitions(LudorkStandardClassRuntime
    PRIVATE
        LUDORK_STANDARD_EXPORTS=1
        LUDORK_PLATFORM="${LUDORK_PLATFORM}"
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${LUDORK_DEBUG_SOL_SAFETIES}>>:SOL_ALL_SAFETIES_ON=1>
        SOL_SAFE_NUMERICS=1
        SOL_USE_INTEROP=1
        SOL_DEFAULT_AUTOMAGICAL_USERTYPES=0
        SOL_USERTYPE_TYPE_BINDING_INFO=0)
target_include_directories(LudorkStandardClassRuntime
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
        ${LUDORK_LUASF_CONSUMER_INCLUDES})
target_link_libraries(LudorkStandardClassRuntime
    PRIVATE
        LuaSF::LuaSF
        LuaSF::Lua)

ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/Bridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Common.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Native.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Composite.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Instance.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Copy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Module.cpp")
