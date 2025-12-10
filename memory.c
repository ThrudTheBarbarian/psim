//
//  memory.c
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "vm.h"

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
    switch (object->type)
        {
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
