include_guard(GLOBAL)

function(ludork_target_precompile_headers target)
    if(LUDORK_WITH_LUA)
        set(headers LuaSF.hpp LuaGlue/LuaGlue.hpp)
    else()
        set(headers
            algorithm memory stdexcept string
            unordered_map utility variant vector)
    endif()
    foreach(header IN LISTS headers)
        target_precompile_headers(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:<${header}$<ANGLE-R>>")
    endforeach()

    get_target_property(sources ${target} SOURCES)
    foreach(source IN LISTS sources)
        if(source MATCHES "\\.(m|mm)$")
            set_source_files_properties("${source}"
                TARGET_DIRECTORY ${target}
                PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
        endif()
    endforeach()
endfunction()
