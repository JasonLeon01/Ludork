set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MSVC)
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
endif()

if(LUDORK_IOS)
    if(NOT Python3_EXECUTABLE)
        message(FATAL_ERROR "LUDORK_IOS=ON requires -DPython3_EXECUTABLE pointing to a host Python 3.12 interpreter.")
    endif()
    if(NOT Python3_INCLUDE_DIR OR NOT Python3_LIBRARY)
        message(FATAL_ERROR "LUDORK_IOS=ON requires -DPython3_INCLUDE_DIR and -DPython3_LIBRARY pointing to ios_python.")
    endif()
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import pybind11; print(pybind11.get_cmake_dir())"
                    OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(PYBIND11_NOPYTHON ON)
    find_package(pybind11 REQUIRED PATHS ${pybind11_DIR})
else()
    set(PYBIND11_FINDPYTHON ON)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import pybind11; print(pybind11.get_cmake_dir())"
                    OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    find_package(pybind11 REQUIRED PATHS ${pybind11_DIR})
endif()

if(LUDORK_IOS)
    function(pybind11_add_module TARGET_NAME)
        set(_srcs ${ARGN})
        list(REMOVE_ITEM _srcs MODULE SHARED THIN_LTO NO_EXTRAS WITHOUT_SOABI OPT_SIZE)
        add_library(${TARGET_NAME} STATIC ${_srcs})
        target_link_libraries(${TARGET_NAME} PRIVATE pybind11::headers)
        target_include_directories(${TARGET_NAME} PRIVATE ${Python3_INCLUDE_DIR})
        set_target_properties(${TARGET_NAME} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            XCODE_ATTRIBUTE_ENABLE_BITCODE NO
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
        )
    endfunction()
endif()

function(ConfigureTarget TARGET_NAME)
    if(LUDORK_IOS)
        target_include_directories(${TARGET_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/bindgen
        )
        return()
    endif()

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
        ${CMAKE_SOURCE_DIR}/bindgen
    )
endfunction()
