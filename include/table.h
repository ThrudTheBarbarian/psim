//
//  table.h
//  psim
//
//  Created by ThrudTheBarbarian on 10/12/2025.
//

#ifndef table_h
#define table_h

#include "common.h"
#include "value.h"

typedef struct
    {
    ObjString* key;         // The key (!)
    Value value;            // The value (...)
    } Entry;

typedef struct
    {
    int count;              // How many items we have
    int capacity;           // How many we can currently store
    Entry* entries;         // The actual data
    } Table;


/*****************************************************************************\
|* Initialise the hashtable
\*****************************************************************************/
void initTable(Table* table);

/*****************************************************************************\
|* Free the hashtable
\*****************************************************************************/
void freeTable(Table* table);

/*****************************************************************************\
|* Insert a value into the table. Returns true if it created a new entry, so
|* false means it overwrote something
\*****************************************************************************/
bool tableSet(Table* table, ObjString* key, Value value);

/*****************************************************************************\
|* Merge one hashtable into another
\*****************************************************************************/
void tableAddAll(Table* from, Table* to);

/*****************************************************************************\
|* Fetch a value from a hashtable, returns whether it found one or not
\*****************************************************************************/
bool tableGet(Table* table, ObjString* key, Value* value);

/*****************************************************************************\
|* Remove a value from a hashtable, returns whether it found one to delete.
|* Note this actually inserts a tombstone entry, rather than really deleting
|* otherwise the hash-collision code gets a lot more complicated
\*****************************************************************************/
bool tableDelete(Table* table, ObjString* key);

/*****************************************************************************\
|* Special case to find a string in the unique-strings table
\*****************************************************************************/
ObjString* tableFindString(Table* table,
                           const char* chars,
                           int length,
                           uint32_t hash);

#endif /* table_h */
