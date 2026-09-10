#pragma once

#if defined(_WIN32)
#if defined(LUDORK_STANDARD_EXPORTS)
#define LUDORK_STANDARD_API __declspec(dllexport)
#else
#define LUDORK_STANDARD_API __declspec(dllimport)
#endif
#else
#define LUDORK_STANDARD_API __attribute__((visibility("default")))
#endif
