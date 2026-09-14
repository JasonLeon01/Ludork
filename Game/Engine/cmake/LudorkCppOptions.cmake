include_guard(GLOBAL)

if(MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:/GR->")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-fno-rtti>")
else()
    message(FATAL_ERROR
        "Ludork requires an MSVC, GNU, Clang or AppleClang C++ compiler to disable RTTI.")
endif()
