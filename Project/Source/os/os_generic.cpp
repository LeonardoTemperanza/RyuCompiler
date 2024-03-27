
#ifdef _WIN32
#include "os_windows.cpp"
#elif defined(__linux__)
#include "os_linux.cpp"
#else
#error Unsupported platform
#endif