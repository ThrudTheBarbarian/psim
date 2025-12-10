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
#include "table.h"

/*****************************************************************************\
|* Enumerate the types of object (basically structured data) we understand
\*****************************************************************************/
typedef enum
    {
    OBJ_CLOSURE,
    OBJ_NATIVE,
    OBJ_FUNCTION,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    } ObjType;

/*****************************************************************************\
|* Define the base object structure holding any state for all object types
\*****************************************************************************/
struct Obj
    {
    ObjType type;           // Type of the object
    bool isMarked;          // Marked to prevent GC reaping it
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
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)

/*****************************************************************************\
|* Get either an ObjString or C-style string from a value (make sure to use
|* the IS_STRING() macro above first)
\*****************************************************************************/
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))

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
    int upvalueCount;       // Number of captured-from-enclosure vars
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
    Obj obj;                // Parent object data
    NativeFn function;      // The actual C function
    } ObjNative;

/*****************************************************************************\
|* Create a new native function
\*****************************************************************************/
ObjNative* newNative(NativeFn function);



#pragma mark - Up-values

typedef struct ObjUpvalue
    {
    Obj obj;                    // Parent object data
    Value* location;            // Pointer to closed-over value
    struct ObjUpvalue* next;    // Pointer to next in list
    Value closed;               // The value of a closed-over up-value
    } ObjUpvalue;

/*****************************************************************************\
|* Create a new upValue
\*****************************************************************************/
ObjUpvalue* newUpvalue(Value* slot);



#pragma mark - Closures

typedef struct
    {
    Obj obj;                // Parent object data
    ObjFunction* function;  // Closures are sort of functions with data
    ObjUpvalue** upvalues;  // upvalues in this closure
    int upvalueCount;       // Number of upvalues
    } ObjClosure;

/*****************************************************************************\
|* Create a new closure
\*****************************************************************************/
ObjClosure* newClosure(ObjFunction* function);




#pragma mark - Classes and instances

typedef struct
    {
    Obj obj;                // Parent object data
    ObjString* name;        // Name of the class
    } ObjClass;

typedef struct
    {
    Obj obj;                // Parent object data
    ObjClass* klass;        // The class object
    Table fields;           // A list of fields
    } ObjInstance;

/*****************************************************************************\
|* Create a new class or instance
\*****************************************************************************/
ObjClass* newClass(ObjString* name);
ObjInstance* newInstance(ObjClass* klass);

#endif /* object_h */
