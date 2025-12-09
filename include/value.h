//
//  value.h
//  6502e
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#ifndef value_h
#define value_h

#include "common.h"

/*****************************************************************************\
|* Basic type will be a 64-bit int. That ought to cover most of the needs of a
|* simulation-type environment
\*****************************************************************************/
typedef int64_t Value;
#define VALUE_FORMAT_STRING "%lld"

/*****************************************************************************\
|* Also handle arrays of types
\*****************************************************************************/
typedef struct
    {
    int capacity;
    int count;
    Value* values;
    } ValueArray;


/*****************************************************************************\
|* Initialise a value array
\*****************************************************************************/
void initValueArray(ValueArray* array);

/*****************************************************************************\
|* Append a value to the end of a value array
\*****************************************************************************/
void writeValueArray(ValueArray* array, Value value);

/*****************************************************************************\
|* Free a value array and re-initialise.
\*****************************************************************************/
void freeValueArray(ValueArray* array);

/*****************************************************************************\
|* Print a value to stdout
\*****************************************************************************/
void printValue(Value value);

#endif /* value_h */
