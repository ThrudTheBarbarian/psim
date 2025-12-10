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
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

/*****************************************************************************\
|* Handles how to set local variables in called functions, even if they are
|* called under different circumstances, and also the instruction pointer for
|* how we return after any given call
\*****************************************************************************/
typedef struct
    {
    ObjFunction* function;          // The function in question
    uint8_t* ip;                    // Where to return to after execution
    Value* slots;                   // Pointer to first variable slot
    } CallFrame;

/*****************************************************************************\
|* Define the virtual machine state
\*****************************************************************************/
typedef struct
    {
    CallFrame frames[FRAMES_MAX];   // How deep we can nest function calls
    int frameCount;                 // Current nested depth of function call
    Chunk* chunk;                   // The chunk we're working on
    uint8_t* ip;                    // Thw instruction pointer to the current insn
    Value stack[STACK_MAX];         // Intermediate storage
    Value* stackTop;                // Pointer to next free value slot
    Obj *objects;                   // Intrinsic list of objects in VM
    Table strings;                  // List of unique strings
    Table globals;                  // List of global variables [21.2]
    } VM;

extern VM vm;

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

/*****************************************************************************\
|* Define a native function
\*****************************************************************************/
void defineNative(const char* name, NativeFn function);

#endif /* vm_h */
