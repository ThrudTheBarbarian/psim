//
//  table.c
//  psim
//
//  Created by ThrudTheBarbarian on 10/12/2025.
//

#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

// This is the ratio of count over capacity. We aim to keep this lower than
// the defined value
#define TABLE_MAX_LOAD 0.75


/*****************************************************************************\
|* Initialise the hashtable
\*****************************************************************************/
void initTable(Table* table)
    {
    table->count    = 0;
    table->capacity = 0;
    table->entries  = NULL;
    }

/*****************************************************************************\
|* Free the hashtable
\*****************************************************************************/
void freeTable(Table* table)
    {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
    }


/*****************************************************************************\
|* Helper function - find an entry in the hashtable. Re-use tombstones if we
|* find them from prior deletes
\*****************************************************************************/
static Entry* findEntry(Entry* entries, int capacity, ObjString* key)
    {
    uint32_t index      = key->hash % capacity;
    Entry* tombstone    = NULL;

    for (;;)
        {
        Entry* entry = &entries[index];
    
        // Handle tombstone entries from prior deletes
        if (entry->key == NULL)
            {
            if (IS_NIL(entry->value))
                {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
                }
            else
                {
                // We found a tombstone.
                if (tombstone == NULL)
                    tombstone = entry;
                }
            }
        else if (entry->key == key)
            {
            // We found the key.
            return entry;
            }

        index = (index + 1) % capacity;
        }
    }

/*****************************************************************************\
|* Helper function - update the capacity of the hashtable so we don't run out
|* of slots
\*****************************************************************************/
static void adjustCapacity(Table* table, int capacity)
    {
    // Create a new table of the correct size
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++)
        {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
        }

    // Copy over the entries
    table->count = 0;
    for (int i = 0; i < table->capacity; i++)
        {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL)
            continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key   = entry->key;
        dest->value = entry->value;
        table->count++;
        }
    
    // Free the old memory
    FREE_ARRAY(Entry, table->entries, table->capacity);

    // Set the parameters in the passed-in object
    table->entries = entries;
    table->capacity = capacity;
    }

    
/*****************************************************************************\
|* Insert a value into the table
\*****************************************************************************/
bool tableSet(Table* table, ObjString* key, Value value)
    {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
        {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
        }

    Entry* entry    = findEntry(table->entries, table->capacity, key);
    bool isNewKey   = entry->key == NULL;
    
    // Only increment count if not a tombstone
    if (isNewKey && IS_NIL(entry->value))
        table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
    }

/*****************************************************************************\
|* Merge one hashtable into another
\*****************************************************************************/
void tableAddAll(Table* from, Table* to)
    {
    for (int i = 0; i < from->capacity; i++)
        {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL)
            tableSet(to, entry->key, entry->value);
        }
    }

/*****************************************************************************\
|* Fetch a value from a hashtable, returns whether it found one or not
\*****************************************************************************/
bool tableGet(Table* table, ObjString* key, Value* value)
    {
    if (table->count == 0)
        return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    *value = entry->value;
    return true;
    }

/*****************************************************************************\
|* Remove a value from a hashtable, returns whether it found one to delete.
|* Note this actually inserts a tombstone entry, rather than really deleting
|* otherwise the hash-collision code gets a lot more complicated
\*****************************************************************************/
bool tableDelete(Table* table, ObjString* key)
    {
    if (table->count == 0)
        return false;

    // Find the entry.
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    // Place a tombstone in the entry.
    entry->key      = NULL;
    entry->value    = BOOL_VAL(true);
    return true;
    }

/*****************************************************************************\
|* Special case to find a string in the unique-strings table
\*****************************************************************************/
ObjString* tableFindString(Table* table,
                           const char* chars,
                           int length,
                           uint32_t hash)
    {
    if (table->count == 0)
        return NULL;

    uint32_t index = hash % table->capacity;
    for (;;)
        {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL)
            {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value))
                return NULL;
            }
        else if ((entry->key->length == length)
             &&  (entry->key->hash == hash)
             &&  (memcmp(entry->key->chars, chars, length) == 0))
            {
            // We found it.
            return entry->key;
            }

        index = (index + 1) % table->capacity;
        }
    }


/*****************************************************************************\
|* GC: Mark objects within the table as valid
\*****************************************************************************/
void markTable(Table* table)
    {
    for (int i = 0; i < table->capacity; i++)
        {
        Entry* entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
        }
    }
    
/*****************************************************************************\
|* GC: Remove anything not marked as part of the grey/black list
\*****************************************************************************/
void tableRemoveWhite(Table* table)
    {
    for (int i = 0; i < table->capacity; i++)
        {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked)
            tableDelete(table, entry->key);
        }
    }
