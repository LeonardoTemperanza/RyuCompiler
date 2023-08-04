
#include "base.h"
#include "interpreter.h"

// Instructions are just offsets (ints)...

int Interp_PrintInstr(Interp* interp, int offset)
{
    printf("%04d ", offset);
    
    uint8 opCode = interp->instrData[offset];
    switch(opCode)
    {
        default:     printf("Unknown Opcode %d\n", opCode); return offset + 1;
        case Op_Ret: printf("OP_RETURN\n"); return offset + 1;
    }
}