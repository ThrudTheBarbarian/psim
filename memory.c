//
//  memory.c
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"

/*****************************************************************************\
|* Reallocate memory. 4 cases to consider:
|*
|* oldSize	    newSize	                Operation
|* 0	        Non‑zero	            Allocate new block.
|* Non‑zero	    0	                    Free allocation.
|* Non‑zero	    Smaller than oldSize	Shrink existing allocation.
|* Non‑zero	    Larger than oldSize	    Grow existing allocation
\*****************************************************************************/
void* reallocate(void* pointer, size_t oldSize, size_t newSize)
    {
    #ifdef DEBUG_STRESS_GC
        if (newSize > oldSize)
            collectGarbage();
    #endif

    if (newSize == 0)
        {
        free(pointer);
        return NULL;
        }

    void* result = realloc(pointer, newSize);
    if (result == NULL)
        {
        perror("Couldn't reallocate space");
        exit(1);
        }
    return result;
    }

/*****************************************************************************\
|* Free an object
\*****************************************************************************/
static void freeObject(Obj* object)
    {
    #ifdef DEBUG_LOG_GC
        printf("%p free type %d\n", (void*)object, object->type);
    #endif

    switch (object->type)
        {
        case OBJ_BOUND_METHOD:
            FREE(ObjBoundMethod, object);
            break;

        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;

        case OBJ_STRING:
            {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
            }

        case OBJ_FUNCTION:
            {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
            }

        case OBJ_CLOSURE:
            {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
            }
            
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
        
        case OBJ_CLASS:
            {
            ObjClass* klass = (ObjClass*)object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
            }
            
        case OBJ_INSTANCE:
            {
            ObjInstance* instance = (ObjInstance*)object;
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
            break;
            }
       }
    }

/*****************************************************************************\
|* Free all the objects that the VM knows about
\*****************************************************************************/
void freeObjects(void)
    {
    Obj* object = vm.objects;
    while (object != NULL)
        {
        Obj* next = object->next;
        freeObject(object);
        object = next;
        }
    }

/*****************************************************************************\
|* GC Help: Mark all the active root objects
\*****************************************************************************/
static void markRoots(void)
    {
    // User variables
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
        markValue(*slot);
    
    // Keys and Values within hashtables
    markTable(&vm.globals);

    // Stack frames
    for (int i = 0; i < vm.frameCount; i++)
        markObject((Obj*)vm.frames[i].closure);
    
    // Up-values
    for (ObjUpvalue* o = vm.openUpvalues; o != NULL; o = o->next)
        markObject((Obj*)o);

    // compiler variables
    markCompilerRoots();
    markObject((Obj*)vm.initString);
    }

/*****************************************************************************\
|* GC Help: Mark an array of objects
\*****************************************************************************/
static void markArray(ValueArray* array)
    {
    for (int i = 0; i < array->count; i++)
        markValue(array->values[i]);
    }

/*****************************************************************************\
|* GC Help: Proces a single object and its references
\*****************************************************************************/
static void blackenObject(Obj* object)
    {
    #ifdef DEBUG_LOG_GC
        printf("%p blacken ", (void*)object);
        printValue(OBJ_VAL(object));
        printf("\n");
    #endif

    switch (object->type)
        {
        case OBJ_BOUND_METHOD:
            {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(bound->receiver);
            markObject((Obj*)bound->method);
            break;
            }

        case OBJ_INSTANCE:
            {
            ObjInstance* instance = (ObjInstance*)object;
            markObject((Obj*)instance->klass);
            markTable(&instance->fields);
            break;
            }
           
        case OBJ_CLASS:
            {
            ObjClass* klass = (ObjClass*)object;
            markObject((Obj*)klass->name);
            markTable(&klass->methods);
            break;
            }
            
        case OBJ_CLOSURE:
            {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++)
                markObject((Obj*)closure->upvalues[i]);
            break;
            }
            
       case OBJ_FUNCTION:
            {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
            }
        
        case OBJ_UPVALUE:
            markValue(((ObjUpvalue*)object)->closed);
            break;

        case OBJ_NATIVE:
        case OBJ_STRING:
            break;  // nothing to do
        }
    }

/*****************************************************************************\
|* GC Help: Use the roots to find references we care about
\*****************************************************************************/
static void traceReferences(void)
    {
    while (vm.grayCount > 0)
        {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
        }
    }


/*****************************************************************************\
|* GC Help: Free any object that wasn't marked to be kept
\*****************************************************************************/
static void sweep(void)
    {
    Obj* previous = NULL;
    Obj* object = vm.objects;
  
    while (object != NULL)
        {
        if (object->isMarked)
            {
            object->isMarked = false;
            previous = object;
            object = object->next;
            }
        else
            {
            Obj* unreached = object;
            object = object->next;
      
            if (previous != NULL)
                previous->next = object;
            else
                vm.objects = object;

            freeObject(unreached);
            }
        }
    }

/*****************************************************************************\
|* Perform garbage collection
\*****************************************************************************/
void collectGarbage(void)
    {
    #ifdef DEBUG_LOG_GC
      printf("-- gc begin\n");
    #endif

    markRoots();
    traceReferences();
    tableRemoveWhite(&(vm.strings));
    sweep();

    #ifdef DEBUG_LOG_GC
      printf("-- gc end\n");
    #endif
    }


/*****************************************************************************\
|* GC: Mark a value/object as a root
\*****************************************************************************/
void markObject(Obj* object)
    {
    if (object == NULL)
        return;
    
    // Prevent acyclic loops
    if (object->isMarked)
        return;

    #ifdef DEBUG_LOG_GC
        printf("%p mark ", (void*)object);
        printValue(OBJ_VAL(object));
        printf("\n");
    #endif

    object->isMarked = true;

    // Update the "gray" list, or work-queue of items to process
    if (vm.grayCapacity < vm.grayCount + 1)
        {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack    = (Obj**)realloc(vm.grayStack,
                                  sizeof(Obj*) * vm.grayCapacity);

        if (vm.grayStack == NULL)
            {
            perror("Cannot allocate GC space");
            exit(1);
            }
        }

    vm.grayStack[vm.grayCount++] = object;
    }

void markValue(Value value)
    {
    if (IS_OBJ(value))
        markObject(AS_OBJ(value));
    }
