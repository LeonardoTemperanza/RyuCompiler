
#pragma once

// This is the platform layer. It provides the platform dependent behavior
// needed specifically by the compiler (and not by any utility program i might write).

#include "memory_management.h"

// Setup
void OS_Init();
void OS_OutputColorInit();

void SetThreadContext(void* ptr);
void* GetThreadContext();

// Timing utilities
static inline uint64 GetRdtscFreq();

// Printing utilities
void SetErrorColor();
void ResetColor();

// Linker
char* GetPlatformLinkerName();
int RunPlatformLinker(char* outputPath, char** objFiles, int objFileCount);