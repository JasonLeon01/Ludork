set(PROJECT_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(SFML_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/SFML/include)

if(LUDORK_IOS)
    set(SFML_BUILD_AUDIO   OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC     OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SFML_USE_STATIC_STD_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/SFML ${CMAKE_BINARY_DIR}/SFML)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SFML_LIB_DIR ${PROJECT_ROOT}/lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SFML_LIB_DIR ${PROJECT_ROOT}/pysf)
endif()
set(SFML_BIN_DIR ${PROJECT_ROOT}/pysf)

function(ConfigureSFMLTarget TARGET_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${SFML_INCLUDE_DIR}
    )

    if(LUDORK_IOS)
        target_link_libraries(${TARGET_NAME} PRIVATE
            sfml-graphics
            sfml-window
            sfml-system
        )
        return()
    endif()

    target_link_directories(${TARGET_NAME} PRIVATE
        ${SFML_LIB_DIR}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
    )
endfunction()
