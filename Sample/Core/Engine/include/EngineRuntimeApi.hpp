#pragma once

#if defined(_WIN32)
#if defined(LUDORK_ENGINE_EXPORTS)
#define LUDORK_ENGINE_API __declspec(dllexport)
#else
#define LUDORK_ENGINE_API __declspec(dllimport)
#endif
#else
#define LUDORK_ENGINE_API __attribute__((visibility("default")))
#endif
