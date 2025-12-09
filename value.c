//
//  value.c
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include <stdio.h>

#include "memory.h"
#include "value.h"

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
    printf("%lld", value);
    }
    

