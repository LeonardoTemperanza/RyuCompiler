
#include "interpreter.h"
#include "ir_generation.h"
#include "defer.h"

void InterpretTestCode()
{
    VirtualMachine vm;
    size_t stackSize = MB(100);
    void* backing = ReserveMemory(stackSize);
    defer(FreeMemory(backing, stackSize));
    Arena_Init(&vm.stack, backing, stackSize, KB(2));
    vm.stackFrameAddress = 0;
    vm.programCounter = 0;
    uchar retStack[64];
    vm.retStack = retStack;
    
    // Alloca twice, stores constants in variables, loads variables to register 0 and 1, adds the values and puts
    // them into register 2, and returns the value 
    // For now all values are double
    IR_Instruction instructions[] = {
        //  operand1         operand2              dest  op           bitfield
        { { 0 },             { 0 },                0,    IR_Alloca,   IR_AllocRet | (3<<6) },  // Allocate the return stack variable
        { { 0 },             { 0 },                0,    IR_Alloca,   (3<<6) },
        { { 0 },             { 0 },                0,    IR_Alloca,   (3<<6) },
        { { .uint64val=0 },  { .doubleval=3.2L },  0,    IR_Store,    IR_IsImmed | (3<<6) },
        { { .uint64val=8 },  { .doubleval=5.7L },  0,    IR_Store,    IR_IsImmed | (3<<6) },
        { { .uint64val=0 },  { 0 },                0,    IR_Load,     3<<6 },
        { { .uint64val=8 },  { 0 },                1,    IR_Load,     3<<6 },
        { { .uint16val=0 },  { .uint16val=1 },     2,    IR_Add,      3<<6 },
        { { .uint64val=0 },  { .uint16val=2 },     0,    IR_Store,    IR_AllocRet | (3<<6) },
        { { 0 },             { 0 },                0,    IR_Ret,      0 }
    };
    
    double val = Interp_ExecCode(&vm, instructions, 0, 10);
    printf("%f\n", val);
}

double Interp_ExecCode(VirtualMachine* vm, IR_Instruction instructions[], int min, int max)
{
    for(int i = min; i < max; ++i)
    {
        IR_Instruction instr = instructions[i];
        if(instr.op < IR_EndArithmetic)
        {
            double lhs = vm->registers[instr.operand1.uint8val].doubleval;
            double rhs;
            if(instr.bitfield & IR_IsImmed)
                rhs = instr.operand2.doubleval;
            else
                rhs = vm->registers[instr.operand2.uint8val].doubleval;
            
            switch(instr.op)
            {
                case IR_Add:
                vm->registers[instr.dest].doubleval = lhs + rhs;
                break;
                case IR_Sub:
                vm->registers[instr.dest].doubleval = lhs - rhs;
                break;
                case IR_Mul:
                vm->registers[instr.dest].doubleval = lhs * rhs;
                break;
                case IR_Div:
                vm->registers[instr.dest].doubleval = lhs / rhs;
                break;
                case IR_Mod:
                Assert(false && "Illegal operation!");
                break;
                default:
                Assert(false && "Not yet implemented!");
                break;
            }
        }
        else
        {
            switch(instr.op)
            {
                case IR_Alloca:
                {
                    uint8 size = IR_GetTypeSize(instr.bitfield);
                    if(size == 0)
                        size = 1;
                    else if(size == 3)
                        size = 8;
                    else
                        size *= 2;
                    if(instr.bitfield & IR_AllocRet){}
                    //Arena_Alloc(&vm->stack, size, size);
                    else
                        Arena_Alloc(&vm->stack, size, size);
                }
                break;
                case IR_Load:
                {
                    uchar* base = instr.bitfield & IR_AllocRet? vm->retStack : vm->stack.buffer;
                    void* toLoad = base + instr.operand1.uint64val;
                    vm->registers[instr.dest].doubleval = *(double*)toLoad;
                }
                case IR_Store:
                {
                    uchar* base = (instr.bitfield & IR_AllocRet)? vm->retStack : vm->stack.buffer;
                    void* destinationAddr = base + instr.operand1.uint64val;
                    double value;
                    if(instr.bitfield & IR_IsImmed)
                        value = instr.operand2.doubleval;
                    else
                        value = vm->registers[instr.operand2.uint64val].doubleval;
                    *(double*)destinationAddr = value;
                }
                break;
                case IR_Ret:
                {
                    return ((double*)vm->retStack)[0];
                }
                break;
            }
        }
    }
    
    Assert(false && "Unreachable code");
    return 0;
}