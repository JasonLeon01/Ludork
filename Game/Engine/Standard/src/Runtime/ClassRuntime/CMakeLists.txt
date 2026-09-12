add_library(LudorkStandardClassRuntime OBJECT)

add_subdirectory(Detail)
add_subdirectory(Native)
add_subdirectory(Composite)
add_subdirectory(Instance)
add_subdirectory(Class)

set_target_properties(LudorkStandardClassRuntime PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
if(MINGW)
    set_target_properties(LudorkStandardClassRuntime PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
endif()

target_compile_features(LudorkStandardClassRuntime PRIVATE cxx_std_20)
target_compile_definitions(LudorkStandardClassRuntime
    PRIVATE
        LUDORK_STANDARD_EXPORTS=1
        LUDORK_PLATFORM="${LUDORK_PLATFORM}")
target_include_directories(LudorkStandardClassRuntime
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
        ${LUDORK_LUASF_CONSUMER_INCLUDES})
target_link_libraries(LudorkStandardClassRuntime
    PRIVATE
        Ludork::SolConfig
        LuaSF::LuaSF
        LuaSF::Lua)
