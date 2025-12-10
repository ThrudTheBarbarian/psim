//
//  object.h
//  psim
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#ifndef object_h
#define object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

/*****************************************************************************\
|* Enumerate the types of object (basically structured data) we understand
\*****************************************************************************/
typedef enum
    {
    OBJ_NATIVE,
    OBJ_FUNCTION,
    OBJ_STRING,
    } ObjType;

/*****************************************************************************\
|* Define the base object structure holding any state for all object types
\*****************************************************************************/
struct Obj
    {
    ObjType type;           // Type of the object
    struct Obj* next;       // Pointer to next object so we can garbage collect
    };

/*****************************************************************************\
|* Print an object's value
\*****************************************************************************/
void printObject(Value value);



#pragma mark - Strings

/*****************************************************************************\
|* Use struct inheritance to also define a string object type
\*****************************************************************************/
struct ObjString
    {
    Obj obj;                // Parent properties
    int length;             // Length of the string
    char* chars;            // Actual string data
    uint32_t hash;          // Value used for hashtables
    };

/*****************************************************************************\
|* inline function to speed things up, given it's just a test-and-see op
\*****************************************************************************/
static inline bool isObjType(Value value, ObjType type)
    {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
    }

/*****************************************************************************\
|* Easy way to get the type of any object
\*****************************************************************************/
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

/*****************************************************************************\
|* Determine if a value is a string
\*****************************************************************************/
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)

/*****************************************************************************\
|* Get either an ObjString or C-style string from a value (make sure to use
|* the IS_STRING() macro above first)
\*****************************************************************************/
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)

/*****************************************************************************\
|* Take a copy of a C string and put it into an ObjString. Allocate on heap
\*****************************************************************************/
ObjString* copyString(const char* chars, int length);

/*****************************************************************************\
|* Take a copy of a C string and put it into an ObjString. Takes ownership of
|* the passed-in pointer
\*****************************************************************************/
ObjString* takeString(char* chars, int length);


#pragma mark - Functions

typedef struct
    {
    Obj obj;                // Parent object data
    int arity;              // number of parameters the function expects
    Chunk chunk;            // Bytecode for the function
    ObjString* name;        // Name of the function
    } ObjFunction;


/*****************************************************************************\
|* Create a new function
\*****************************************************************************/
ObjFunction* newFunction(void);


#pragma mark - Native Functions

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct
    {
    Obj obj;                // PArent object data
    NativeFn function;      // The actual C function
    } ObjNative;

/*****************************************************************************\
|* Create a new native function
\*****************************************************************************/
ObjNative* newNative(NativeFn function);

#endif /* object_h */
