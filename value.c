//
//  value.c
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "object.h"

/*****************************************************************************\
|* Initialise a value array
\*****************************************************************************/
void initValueArray(ValueArray* array)
    {
    array->values       = NULL;
    array->capacity     = 0;
    array->count        = 0;
    }

/*****************************************************************************\
|* Append a value to the end of a value array
\*****************************************************************************/
void writeValueArray(ValueArray* array, Value value)
    {
    if (array->capacity < array->count + 1)
        {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values   = GROW_ARRAY(Value,
                                     array->values,
                                     oldCapacity,
                                     array->capacity);
        }

    array->values[array->count] = value;
    array->count++;
    }

/*****************************************************************************\
|* Free a value array and re-initialise.
\*****************************************************************************/
void freeValueArray(ValueArray* array)
    {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
    }

/*****************************************************************************\
|* Print a value to stdout
\*****************************************************************************/
void printValue(Value value)
    {
    switch (value.type)
        {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        
        case VAL_NIL:
            printf("nil");
            break;
    
        case VAL_NUMBER:
            printf("%lld", AS_NUMBER(value));
            break;

        case VAL_OBJ:
            printObject(value);
            break;

        }
    }

/*****************************************************************************\
|* Determine if two values are equal
\*****************************************************************************/
bool valuesEqual(Value a, Value b)
    {
    if (a.type != b.type)
        return false;
  
    switch (a.type)
        {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
    
        case VAL_NIL:
            return true;
    
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);

        case VAL_OBJ:
            {
            ObjString* aString  = AS_STRING(a);
            ObjString* bString  = AS_STRING(b);
            bool matchLength    = (aString->length == bString->length);
            return matchLength && (memcmp(aString->chars,
                                          bString->chars,
                                          aString->length) == 0);
            }
            
        default:
            return false; // Unreachable.
        }
    }


