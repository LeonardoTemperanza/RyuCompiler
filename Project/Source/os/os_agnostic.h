
#pragma once

#include "memory_management.h"

// Setup
void OS_Init();
void OS_OutputColorInit();

// Memory utilities
void* ReserveMemory(size_t size);
void CommitMemory(void* mem, size_t size);
void FreeMemory(void* mem, size_t size);

void SetThreadContext(void* ptr);
void* GetThreadContext();

// Timing utilities
// This function should cache the result
static inline uint64 GetRdtscFreq();

// Printing utilities
void SetErrorColor();
void ResetColor();

// Linker
char* GetPlatformLinkerName();
int RunPlatformLinker(char* outputPath, char** objFiles, int objFileCount);