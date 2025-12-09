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


/*****************************************************************************\
|* Allocate space on the heap for an object
\*****************************************************************************/
#define ALLOCATE_OBJ(type, objectType)                                      \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type)
    {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    return object;
    }


/*****************************************************************************\
|* Helper function: Do the real allocation for a string
\*****************************************************************************/
static ObjString* allocateString(char* chars, int length)
    {
    ObjString* string   = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length      = length;
    string->chars       = chars;
    return string;
    }

/*****************************************************************************\
|* Take a copy of a C string and put it into an ObjString*
\*****************************************************************************/
ObjString* copyString(const char* chars, int length)
    {
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
    }
    
/*****************************************************************************\
|* Take a copy of a C string and put it into an ObjString. Takes ownership of
|* the passed-in pointer
\*****************************************************************************/
ObjString* takeString(char* chars, int length)
    {
    return allocateString(chars, length);
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
        }
    }
