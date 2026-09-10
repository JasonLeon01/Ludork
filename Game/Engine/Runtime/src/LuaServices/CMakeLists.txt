set(LUDORK_RUNTIME_LUA_SERVICE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeReferenceConversion.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeServiceCommon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeServiceInternals.cpp")

target_sources(LudorkRuntime PRIVATE ${LUDORK_RUNTIME_LUA_SERVICE_SOURCES})
