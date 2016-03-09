
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "handlebars.h"
#include "handlebars_map.h"
#include "handlebars_memory.h"
#include "handlebars_private.h"
#include "handlebars_string.h"
#include "handlebars_value.h"



#undef CONTEXT
#define CONTEXT HBSCTX(ctx)

struct handlebars_map * handlebars_map_ctor(struct handlebars_context * ctx)
{
    struct handlebars_map * map = MC(handlebars_talloc_zero(ctx, struct handlebars_map));
    map->ctx = CONTEXT;
    map->table_size = 32;
    map->v = talloc_steal(map, MC(handlebars_talloc_array(ctx, struct handlebars_map_entry, map->table_size)));
    map->table = talloc_steal(map, MC(handlebars_talloc_array(ctx, struct handlebars_map_entry *, map->table_size)));
    memset(map->table, 0, sizeof(struct handlebars_map_entry *) * map->table_size);
    return map;
}

#undef CONTEXT
#define CONTEXT HBSCTX(map->ctx)

void handlebars_map_dtor(struct handlebars_map * map)
{
#ifndef HANDLEBARS_NO_REFCOUNT
    size_t i;

    for( i = 0; i < map->i; i++ ) {
        handlebars_value_delref(map->v[i].value);
    }
#endif

    handlebars_talloc_free(map);
}

static inline bool handlebars_map_entry_eq(struct handlebars_map_entry * entry1, struct handlebars_map_entry * entry2)
{
    return handlebars_string_eq(entry1->key, entry2->key);
}

static inline int _ht_add(struct handlebars_map_entry ** table, size_t table_size, struct handlebars_map_entry * entry)
{
    unsigned long index = entry->key->hash % (unsigned long) table_size;
    struct handlebars_map_entry * parent = table[index];
    if( parent ) {
        assert(!handlebars_map_entry_eq(parent, entry));

        // Append to end of list
        while( parent->child ) {
            parent = parent->child;
        }
        parent->child = entry;
        entry->parent = parent;

#if 0
        fprintf(stderr, "Collision %s (%ld) %s (%ld)\n", parent->key->val, index, entry->key->val, index);
#endif
        return 1;
    } else {
        table[index] = entry;
        return 0;
    }
}

static inline void _rehash(struct handlebars_map * map)
{
    struct handlebars_map_entry * entry;
    size_t i;

    // Reset collisions
    map->collisions = 0;

    // Reset table
    memset(map->table, 0, sizeof(map->table[0]) * map->table_size);

    // Reimport table
    for( i = 0; i < map->i; i++ ) {
        entry = &map->v[i];
        entry->child = NULL;
        entry->parent = NULL;
        map->collisions += _ht_add(map->table, map->table_size, entry);
    }
}

static void _resize(struct handlebars_map * map)
{
    size_t table_size = map->table_size * 2;
    struct handlebars_map_entry * v;
    struct handlebars_map_entry ** table;

#if 0
    fprintf(stderr, "Resize: entries: %d, collisions: %d, old size: %d, new size: %d\n", map->i, map->collisions, map->table_size, table_size);
#endif

    // Realloc internal array
    v = handlebars_talloc_realloc(map, map->v, struct handlebars_map_entry, table_size);
    if( !v ) { // Explode
        map->i = 0;
        MC(v);
    }
    map->v = v;

    // Create new table
    table = talloc_steal(map, MC(handlebars_talloc_array(CONTEXT, struct handlebars_map_entry *, table_size)));
    memset(table, 0, sizeof(struct handlebars_map_entry *) * table_size);

    // Swap with old table
    handlebars_talloc_free(map->table);
    map->table = table;
    map->table_size = table_size;

    // Rehash
    _rehash(map);
}

static inline struct handlebars_map_entry * _entry_find(struct handlebars_map * map, const char * key, size_t length, unsigned long hash)
{
    struct handlebars_map_entry * found = map->table[hash  % map->table_size];
    while( found ) {
        if( handlebars_string_eq_ex(found->key->val, found->key->len, found->key->hash, key, length, hash) ) {
            return found;
        } else {
            found = found->child;
        }
    }
    return NULL;

}

static inline void _entry_add(struct handlebars_map * map, const char * key, size_t len, unsigned long hash, struct handlebars_value * value)
{
    // Add to array
    struct handlebars_map_entry * entry = &map->v[map->i++];
    entry->key = talloc_steal(map /*entry*/, handlebars_string_ctor_ex(CONTEXT, key, len, hash));
    entry->value = value;
    entry->parent = NULL;
    entry->child = NULL;
    handlebars_value_addref(value);

    // Add to hash table
    map->collisions += _ht_add(map->table, map->table_size, entry);

    // Rehash
    if( map->i * 1000 / map->table_size > 500 ) { // @todo adjust
        _resize(map);
    }
}

static inline void _entry_remove(struct handlebars_map * map, struct handlebars_map_entry * entry)
{
    struct handlebars_value * value = entry->value;
    struct handlebars_string * key = entry->key;

    // Remove from array
    size_t i = entry - map->v;
    memmove(&map->v[i], &map->v[i + 1], sizeof(struct handlebars_map_entry) * (map->i - i));
    map->i--;

    // Rehash
    _rehash(map);

    // Free
    handlebars_value_delref(value);
    handlebars_talloc_free(key);
}













bool handlebars_map_add(struct handlebars_map * map, struct handlebars_string * string, struct handlebars_value * value)
{
    _entry_add(map, string->val, string->len, string->hash, value);
    return true;
}

bool handlebars_map_str_add(struct handlebars_map * map, const char * key, size_t len, struct handlebars_value * value)
{
    _entry_add(map, key, len, handlebars_string_hash(key), value);
    return true;
}

bool handlebars_map_remove(struct handlebars_map * map, struct handlebars_string * key)
{
    struct handlebars_map_entry * entry = _entry_find(map, key->val, key->len, key->hash);
    if( entry ) {
        _entry_remove(map, entry);
        return 1;
    }
    return 0;
}

bool handlebars_map_index_remove(struct handlebars_map * map, size_t index)
{
    struct handlebars_map_entry * entry = &map->v[index];
    if( entry ) {
        _entry_remove(map, entry);
        return 1;
    }
    return 0;
}

bool handlebars_map_str_remove(struct handlebars_map * map, const char * key, size_t len)
{
    unsigned long hash = handlebars_string_hash(key);
    struct handlebars_map_entry * entry = _entry_find(map, key, len, hash);
    if( entry ) {
        _entry_remove(map, entry);
        return 1;
    }
    return 0;
}


struct handlebars_value * handlebars_map_find(struct handlebars_map * map, struct handlebars_string * key)
{
    struct handlebars_value * value = NULL;
    struct handlebars_map_entry * entry = _entry_find(map, key->val, key->len, key->hash);

    if( entry ) {
        value = entry->value;
        handlebars_value_addref(value);
    }

    return value;
}

struct handlebars_value * handlebars_map_str_find(struct handlebars_map * map, const char * key, size_t len)
{
    struct handlebars_value * value = NULL;
    struct handlebars_map_entry * entry = _entry_find(map, key, len, handlebars_string_hash(key));

    if( entry ) {
        value = entry->value;
        handlebars_value_addref(value);
    }

    return value;
}


bool handlebars_map_update(struct handlebars_map * map, struct handlebars_string * string, struct handlebars_value * value)
{
    struct handlebars_map_entry * entry = _entry_find(map, string->val, string->len, string->hash);
    if( entry ) {
        handlebars_value_delref(entry->value);
        entry->value = value;
        handlebars_value_addref(entry->value);
        return true;
    } else {
        _entry_add(map, string->val, string->len, string->hash, value);
        return true;
    }
}

bool handlebars_map_str_update(struct handlebars_map * map, const char * key, size_t len, struct handlebars_value * value)
{
    struct handlebars_string * string = talloc_steal(map, handlebars_string_ctor(CONTEXT, key, len));
    struct handlebars_map_entry * entry = _entry_find(map, string->val, string->len, string->hash);
    if( entry ) {
        handlebars_value_delref(entry->value);
        entry->value = value;
        handlebars_value_addref(entry->value);
        handlebars_talloc_free(string);
        return true;
    } else {
        _entry_add(map, string->val, string->len, string->hash, value);
        return true;
    }
}

bool handlebars_map_sort(struct handlebars_map * map, int (*compar)(const void*,const void*))
{
    qsort(map->v, map->i, sizeof(struct handlebars_map_entry), compar);
    _rehash(map);
}
