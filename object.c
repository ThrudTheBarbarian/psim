//
//  object.c
//  psim
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"


static void printFunction(ObjFunction* function);

/*****************************************************************************\
|* Allocate space on the heap for an object
\*****************************************************************************/
#define ALLOCATE_OBJ(type, objectType)                                      \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type)
    {
    Obj* object         = (Obj*)reallocate(NULL, 0, size);
    object->type        = type;
    object->isMarked    = false;

    object->next        = vm.objects;
    vm.objects          = object;
    
    #ifdef DEBUG_LOG_GC
        printf("%p allocate %zu for type %d\n", (void*)object, size, type);
    #endif

    return object;
    }

/*****************************************************************************\
|* Print an object's value
\*****************************************************************************/
void printObject(Value value)
    {
    switch (OBJ_TYPE(value))
        {
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;

        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;

        case OBJ_NATIVE:
            printf("<native fn>");
            break;
   
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
 
        case OBJ_UPVALUE:
            printf("upvalue");
            break;

        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
       }
    }

#pragma mark - Strings

/*****************************************************************************\
|* Helper function: Do the real allocation for a string
\*****************************************************************************/
static ObjString* allocateString(char* chars, int length, uint32_t hash)
    {
    ObjString* string   = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length      = length;
    string->chars       = chars;
    string->hash        = hash;
    
    // Protect against GC
    push(OBJ_VAL(string));
    
    // Add it to the set of unique strings
    tableSet(&vm.strings, string, NIL_VAL);
    
    // relinquish the protection
    pop();
    
    return string;
    }

/*****************************************************************************\
|* Helper function: Compute a hash for a string using FNV-1a
\*****************************************************************************/
static uint32_t hashString(const char* key, int length)
    {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
        {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
        }
    return hash;
    }

/*****************************************************************************\
|* Take a copy of a C string and put it into an ObjString*
\*****************************************************************************/
ObjString* copyString(const char* chars, int length)
    {
    uint32_t hash       = hashString(chars, length);
    
    // Check to see if this string has been interned into the unique-strings
    // table in the VM
    ObjString* interned = tableFindString(&(vm.strings), chars, length,
                                        hash);
    if (interned != NULL)
        return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length]   = '\0';

    return allocateString(heapChars, length, hash);
    }
    
/*****************************************************************************\
|* Take a copy of a C string and put it into an ObjString. Takes ownership of
|* the passed-in pointer
\*****************************************************************************/
ObjString* takeString(char* chars, int length)
    {
    uint32_t hash       = hashString(chars, length);
    ObjString* interned = tableFindString(&(vm.strings), chars, length,
                                        hash);
    if (interned != NULL)
        {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
        }

    return allocateString(chars, length, hash);
    }


#pragma mark - Functions

/*****************************************************************************\
|* Create a new function
\*****************************************************************************/
ObjFunction* newFunction(void)
    {
    ObjFunction* function   = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity         = 0;
    function->upvalueCount  = 0;
    function->name          = NULL;
    
    initChunk(&function->chunk);
    return function;
    }

/*****************************************************************************\
|* Print a function's name out
\*****************************************************************************/
static void printFunction(ObjFunction* function)
    {
    if (function->name == NULL)
        {
        printf("<script>");
        return;
        }

    printf("<fn %s>", function->name->chars);
    }


#pragma mark - Native Functions


/*****************************************************************************\
|* Create a new native function
\*****************************************************************************/
ObjNative* newNative(NativeFn function)
    {
    ObjNative* native   = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function    = function;
    return native;
    }


#pragma mark - Closures

/*****************************************************************************\
|* Create a new native closure
\*****************************************************************************/
ObjClosure* newClosure(ObjFunction* function)
    {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++)
        upvalues[i] = NULL;
        
    ObjClosure* closure     = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function       = function;
    closure->upvalues       = upvalues;
    closure->upvalueCount   = function->upvalueCount;
    return closure;
    }



#pragma mark - Up-values

/*****************************************************************************\
|* Create a new upValue
\*****************************************************************************/
ObjUpvalue* newUpvalue(Value* slot)
    {
    ObjUpvalue* upvalue     = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location       = slot;
    upvalue->next           = NULL;
    upvalue->closed         = NIL_VAL;
    return upvalue;
    }


#pragma mark - Classes and instances

/*****************************************************************************\
|* Create a new class
\*****************************************************************************/
ObjClass* newClass(ObjString* name)
    {
    ObjClass* klass         = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name             = name;
    return klass;
    }

/*****************************************************************************\
|* Create a new instance
\*****************************************************************************/
ObjInstance* newInstance(ObjClass* klass)
    {
    ObjInstance* instance   = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass         = klass;
    initTable(&instance->fields);
    return instance;
    }
