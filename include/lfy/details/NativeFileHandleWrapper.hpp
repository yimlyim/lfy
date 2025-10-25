#pragma once

#if defined(_WIN32)
// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Reduce size of Windows headers
#define WIN32_LEAN_AND_MEAN
#include <lfy/details/NativeFileHandleWindows.hpp>

#elif defined(__linux__)
#include <lfy/details/NativeFileHandleLinux.hpp>

#else
#error "Unsupported platform for native file handle"
#endif