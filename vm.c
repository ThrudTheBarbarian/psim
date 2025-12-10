//
//  vm.c
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include <stdarg.h>
#include <string.h>

#include "compiler.h"
#include "debug.h"
#include "vm.h"
#include "object.h"
#include "memory.h"

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
    vm.objects = NULL;
    initTable(&(vm.strings));
    initTable(&(vm.globals));
    }

/*****************************************************************************\
|* Free the virtual machine
\*****************************************************************************/
void freeVM(void)
    {
    freeObjects();
    freeTable(&(vm.strings));
    freeTable(&(vm.globals));
    }


/*****************************************************************************\
|* Implement runtime errors
\*****************************************************************************/
static void runtimeError(const char* format, ...)
    {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
    }

/*****************************************************************************\
|* Return a value from the stack but don't pop it
\*****************************************************************************/
static Value peek(int distance)
    {
    return vm.stackTop[-1 - distance];
    }

/*****************************************************************************\
|* "False" is a fairly permissive definition
\*****************************************************************************/
static bool isFalsey(Value value)
    {
    return IS_NIL(value)
        || (IS_BOOL(value) && !AS_BOOL(value))
        || (IS_NUMBER(value) && !AS_NUMBER(value));
    }

/*****************************************************************************\
|* Add 2 strings by concatenation
\*****************************************************************************/
static void concatenate(void)
    {
    ObjString* b    = AS_STRING(pop());
    ObjString* a    = AS_STRING(pop());

    int length      = a->length + b->length;
    char* chars     = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
    }

/*****************************************************************************\
|* Run the VM and return the result, the actual implementation
\*****************************************************************************/
static InterpretResult run(void)
    {
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    #define BINARY_OP(valueType, op)                                        \
        do                                                                  \
            {                                                               \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))                 \
                {                                                           \
                runtimeError("Operands must be numbers.");                  \
                return INTERPRET_RUNTIME_ERROR;                             \
                }                                                           \
            int64_t b = AS_NUMBER(pop());                                   \
            int64_t a = AS_NUMBER(pop());                                   \
            push(valueType(a op b));                                        \
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
 
            case OP_NIL:
                push(NIL_VAL);
                break;
                
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
                
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
  
            case OP_POP:
                pop();
                break;

            case OP_GET_GLOBAL:
                {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value))
                    {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                    }
                push(value);
                break;
                }

            case OP_SET_GLOBAL:
                {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0)))
                    {
                    // tableSet always store, so delete the zonbie
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                    }
                break;
                }

            case OP_DEFINE_GLOBAL:
                {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
                }

            case OP_EQUAL:
                {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
                }

            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
      
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;

            case OP_ADD:
                if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
                    {
                    concatenate();
                    }
                else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
                    {
                    int64_t b = AS_NUMBER(pop());
                    int64_t a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                    }
                else
                    {
                    runtimeError( "Operands must be 2 numbers or 2 strings.");
                    return INTERPRET_RUNTIME_ERROR;
                    }
                break;
     
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
      
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
      
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;

            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;

            case OP_NEGATE:
                if (!IS_NUMBER(peek(0)))
                    {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                    }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;

            case OP_PRINT:
                printValue(pop());
                printf("\n");
                break;
                
            case OP_RETURN:
                return INTERPRET_OK;

            }
        }
    
    #undef READ_STRING
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
