set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SFML_INCLUDE_DIR ${PROJECT_ROOT}/C_Extensions/SFML/include)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SFML_LIB_DIR ${PROJECT_ROOT}/lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(SFML_LIB_DIR ${PROJECT_ROOT}/Sample/Engine/pysf)
endif()
set(SFML_BIN_DIR ${PROJECT_ROOT}/Sample/Engine/pysf)

function(ConfigureSFMLTarget TARGET_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${SFML_INCLUDE_DIR}
    )

    target_link_directories(${TARGET_NAME} PRIVATE
        ${SFML_LIB_DIR}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
    )
endfunction()