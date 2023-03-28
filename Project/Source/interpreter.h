
#pragma once

#include "ir_generation.h"

struct VirtualMachine
{
    Arena stack;
    GenValue registers[4000];
    uchar* retStack;
    size_t stackFrameAddress;
    size_t programCounter;
};

void InterpretTestCode();
double Interp_ExecCode(VirtualMachine* vm, IR_Instruction instr[], int min, int max);