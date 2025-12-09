//
//  memory.h
//  6502e
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#ifndef memory_h
#define memory_h

#include "common.h"
#include "object.h"

/*****************************************************************************\
|* Allocate space in the heap
\*****************************************************************************/
#define ALLOCATE(type, count)                                               \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

/*****************************************************************************\
|* The number of bytes by which dynamic arrays are grown on demand
\*****************************************************************************/
#define GROW_CAPACITY(capacity)     ((capacity) < 8 ? 8 : (capacity) * 2)

/*****************************************************************************\
|* Update the memory storage of a dynamic array
\*****************************************************************************/
#define GROW_ARRAY(type, pointer, oldCount, newCount)                       \
    (type*)reallocate(pointer,                                              \
                      sizeof(type) * (oldCount),                            \
                      sizeof(type) * (newCount))

/*****************************************************************************\
|* Release the memory storage of a dynamic array
\*****************************************************************************/
#define FREE_ARRAY(type, pointer, oldCount)                                 \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/*****************************************************************************\
|* Release memory allocated with ALLOCATE
\*****************************************************************************/
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

/*****************************************************************************\
|* The actual code that reallocates memory
\*****************************************************************************/
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

/*****************************************************************************\
|* Free all the objects that the VM knows about
\*****************************************************************************/
void freeObjects(void);

#endif /* memory_h */
