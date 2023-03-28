
#pragma once

// Memory utilities
void* ReserveMemory(size_t size);
void CommitMemory(void* mem, size_t size);
void FreeMemory(void* mem, size_t size);

// Timing utilities
static inline uint64 GetRdtscFreq();