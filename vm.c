//
//  vm.c
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include "compiler.h"
#include "debug.h"
#include "vm.h"

/*****************************************************************************\
|* Declare the virtual machine instance
\*****************************************************************************/
VM vm;

/*****************************************************************************\
|* Reset the stack pointer
\*****************************************************************************/
static void resetStack(void)
    {
    vm.stackTop = vm.stack;
    }


/*****************************************************************************\
|* Initialise the virtual machine
\*****************************************************************************/
void initVM(void)
    {
    resetStack();
    }

/*****************************************************************************\
|* Free the virtual machine
\*****************************************************************************/
void freeVM(void)
    {
    }


/*****************************************************************************\
|* Run the VM and return the result, the actual implementation
\*****************************************************************************/
static InterpretResult run(void)
    {
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    #define BINARY_OP(op)                                                   \
        do                                                                  \
            {                                                               \
            Value b = pop();                                                \
            Value a = pop();                                                \
            push(a op b);                                                   \
            }                                                               \
        while (false)

    for (;;)
        {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("              ");
            for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
                {
                printf("[");
                printValue(*slot);
                printf("] ");
                }
            printf("\n");
            
            disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
            {
            case OP_CONSTANT:
                {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
                }
 
            case OP_ADD:
                BINARY_OP(+);
                break;
      
            case OP_SUBTRACT:
                BINARY_OP(-);
                break;
      
            case OP_MULTIPLY:
                BINARY_OP(*);
                break;
      
            case OP_DIVIDE:
                BINARY_OP(/);
                break;

            case OP_NEGATE:
                push(-pop());
                break;
            
            case OP_RETURN:
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;

            }
        }
    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef BINARY_OP
    }

/*****************************************************************************\
|* Run the virtual machine and return a result code, public interface
\*****************************************************************************/
InterpretResult interpret(const char* source)
    {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk))
        {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
        }

    vm.chunk    = &chunk;
    vm.ip       = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
    }


/*****************************************************************************\
|* Push a value onto the stack and update
\*****************************************************************************/
void push(Value value)
    {
    *(vm.stackTop) = value;
    vm.stackTop ++;
    }

/*****************************************************************************\
|* Pop a value off the stack and update
\*****************************************************************************/
Value pop(void)
    {
    vm.stackTop --;
    return *(vm.stackTop);
    }
