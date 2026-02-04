set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MSVC)
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
endif()
execute_process(COMMAND python -c "import pybind11; print(pybind11.get_cmake_dir())"
                OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 REQUIRED PATHS ${pybind11_DIR})

function(ConfigureTarget TARGET_NAME)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set_target_properties(${TARGET_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF
            INSTALL_RPATH "@loader_path;@loader_path/../pysf"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endfunction()
