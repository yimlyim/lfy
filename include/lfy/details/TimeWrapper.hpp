#pragma once

#if defined(_WIN32)
// Prevent Windows.h from defining min/max macros
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
// Reduce size of Windows headers
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include "lfy/details/TimeWindows.hpp"
#elif defined(__linux__)
#include "lfy/details/TimeLinux.hpp"
#else
#error "Unsupported platform for time functions"
#endif