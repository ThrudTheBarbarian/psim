//
//  value.h
//  6502e
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#ifndef value_h
#define value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

/*****************************************************************************\
|* Types that can be represented in a Value
\*****************************************************************************/
typedef enum
    {
    VAL_BOOL,       // Bool
    VAL_NIL,        // Null
    VAL_NUMBER,     // Number
    VAL_OBJ,        // String or object
    } ValueType;

/*****************************************************************************\
|* Basic type will be a 64-bit int. That ought to cover most of the needs of a
|* simulation-type environment
\*****************************************************************************/

//#define INTEGER_ONLY

#ifdef INTEGER_ONLY
#  define VALUE_TYPE    int64_t
#  define VALUE_FORMAT_STRING "%lld"
#else
#  define VALUE_TYPE    double
#  define VALUE_FORMAT_STRING "%lg"
#endif

/*****************************************************************************\
|* How we check a psim type is of a given type
\*****************************************************************************/
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

/*****************************************************************************\
|* How we obtain a 'C' type from a psim type
\*****************************************************************************/
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)

/*****************************************************************************\
|* How we promote a 'C' type to a psim type
\*****************************************************************************/
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

typedef struct
    {
    ValueType type;
    union
        {
        bool boolean;
        VALUE_TYPE number;
        Obj* obj;
        } as;
    } Value;


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
|* Determine if two values are equal
\*****************************************************************************/
bool valuesEqual(Value a, Value b);

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
