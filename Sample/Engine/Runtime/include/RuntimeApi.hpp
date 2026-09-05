#pragma once

#if defined(_WIN32)
#if defined(LUDORK_RUNTIME_EXPORTS)
#define LUDORK_RUNTIME_API __declspec(dllexport)
#else
#define LUDORK_RUNTIME_API __declspec(dllimport)
#endif
#else
#define LUDORK_RUNTIME_API __attribute__((visibility("default")))
#endif
