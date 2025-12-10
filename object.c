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
    Obj* object     = (Obj*)reallocate(NULL, 0, size);
    object->type    = type;
    
    object->next    = vm.objects;
    vm.objects      = object;
    return object;
    }

/*****************************************************************************\
|* Print an object's value
\*****************************************************************************/
void printObject(Value value)
    {
    switch (OBJ_TYPE(value))
        {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        }
    }

#pragma mark Strings

/*****************************************************************************\
|* Helper function: Do the real allocation for a string
\*****************************************************************************/
static ObjString* allocateString(char* chars, int length, uint32_t hash)
    {
    ObjString* string   = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length      = length;
    string->chars       = chars;
    string->hash        = hash;
    
    // Add it to the set of unique strings
    tableSet(&vm.strings, string, NIL_VAL);
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


#pragma mark Functions

/*****************************************************************************\
|* Create a new function
\*****************************************************************************/
ObjFunction* newFunction(void)
    {
    ObjFunction* function   = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity         = 0;
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

