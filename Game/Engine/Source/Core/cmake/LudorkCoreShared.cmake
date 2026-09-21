include_guard(GLOBAL)

function(ludork_add_core_shared target)
    set(shared_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/Shared")
    target_include_directories(${target} PRIVATE "${shared_root}/include")
    target_sources(${target} PRIVATE
        "${shared_root}/src/SpriteVisuals.cpp"
        "${shared_root}/src/Text/TextConfigCodec.cpp"
        "${shared_root}/src/JoystickDevice/JoystickDevice.cpp"
        "${shared_root}/src/JoystickDevice/JoystickDeviceImpl.cpp")
endfunction()
