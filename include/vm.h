//
//  vm.h
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#ifndef vm_h
#define vm_h

#include <stdio.h>
#include "chunk.h"

#define STACK_MAX 1024

/*****************************************************************************\
|* Define the virtual machine state
\*****************************************************************************/
typedef struct
    {
    Chunk* chunk;               // The chunk we're working on
    uint8_t* ip;                // Thw instruction pointer to the current insn
    Value stack[STACK_MAX];     // Intermediate storage
    Value* stackTop;
    } VM;

typedef enum
    {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
    } InterpretResult;

/*****************************************************************************\
|* Initialise the virtual machine
\*****************************************************************************/
void initVM(void);

/*****************************************************************************\
|* Free the virtual machine
\*****************************************************************************/
void freeVM(void);

/*****************************************************************************\
|* Run the VM and interpret a chunk
\*****************************************************************************/
InterpretResult interpret(const char* source);

/*****************************************************************************\
|* Push a value onto the stack and update
\*****************************************************************************/
void push(Value value);

/*****************************************************************************\
|* Pop a value off the stack and update
\*****************************************************************************/
Value pop(void);

#endif /* vm_h */
