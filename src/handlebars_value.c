
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>
#include <talloc.h>
#include <yaml.h>

#if defined(HAVE_JSON_C_JSON_H)
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#elif defined(HAVE_JSON_JSON_H)
#include <json/json.h>
#include <json/json_object.h>
#include <json/json_tokener.h>
#endif

#include "handlebars.h"
#include "handlebars_map.h"
#include "handlebars_memory.h"
#include "handlebars_private.h"
#include "handlebars_stack.h"
#include "handlebars_utils.h"
#include "handlebars_value.h"
#include "handlebars_value_handlers.h"



#undef CONTEXT
#define CONTEXT HBSCTX(ctx)

struct handlebars_value * handlebars_value_ctor(struct handlebars_context * ctx)
{
    struct handlebars_value * value = MC(handlebars_talloc_zero(ctx, struct handlebars_value));
    value->ctx = CONTEXT;
    value->refcount = 1;
    value->flags = HANDLEBARS_VALUE_FLAG_HEAP_ALLOCATED;
    return value;
}

#undef CONTEXT
#define CONTEXT HBSCTX(value->ctx)

enum handlebars_value_type handlebars_value_get_type(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		return handlebars_value_get_handlers(value)->type(value);
	} else {
		return value->type;
	}
}

struct handlebars_value * handlebars_value_map_find(struct handlebars_value * value, struct handlebars_string * key)
{
    if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
        if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_MAP ) {
            return handlebars_value_get_handlers(value)->map_find(value, key);
        }
    } else if( value->type == HANDLEBARS_VALUE_TYPE_MAP ) {
        return handlebars_map_find(value->v.map, key);
    }

    return NULL;
}

struct handlebars_value * handlebars_value_map_str_find(struct handlebars_value * value, const char * key, size_t len)
{
    struct handlebars_value * ret = NULL;
    struct handlebars_string * str = handlebars_string_ctor(value->ctx, key, len);

	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_MAP ) {
			ret = handlebars_value_get_handlers(value)->map_find(value, str);
		}
	} else if( value->type == HANDLEBARS_VALUE_TYPE_MAP ) {
        ret = handlebars_map_find(value->v.map, str);
    }

    handlebars_talloc_free(str);
	return ret;
}

struct handlebars_value * handlebars_value_array_find(struct handlebars_value * value, size_t index)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_ARRAY ) {
			return handlebars_value_get_handlers(value)->array_find(value, index);
		}
	} else if( value->type == HANDLEBARS_VALUE_TYPE_ARRAY ) {
        return handlebars_stack_get(value->v.stack, index);
    }

	return NULL;
}

char * handlebars_value_get_strval(struct handlebars_value * value)
{
    char * ret;
    enum handlebars_value_type type = value ? value->type : HANDLEBARS_VALUE_TYPE_NULL;

    switch( type ) {
        case HANDLEBARS_VALUE_TYPE_STRING:
            ret = handlebars_talloc_strdup(value->ctx, value->v.string->val);
            break;
        case HANDLEBARS_VALUE_TYPE_INTEGER:
            ret = handlebars_talloc_asprintf(value->ctx, "%ld", value->v.lval);
            break;
        case HANDLEBARS_VALUE_TYPE_FLOAT:
            ret = handlebars_talloc_asprintf(value->ctx, "%g", value->v.dval);
            break;
        case HANDLEBARS_VALUE_TYPE_BOOLEAN:
            ret = handlebars_talloc_strdup(value->ctx, value->v.bval ? "true" : "false");
            break;
        default:
            ret = handlebars_talloc_strdup(value->ctx, "");
            break;
    }

    MEMCHK(ret);

    return ret;
}

size_t handlebars_value_get_strlen(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_STRING ) {
		return value->v.string->len;
	}

	return 0;
}

bool handlebars_value_get_boolval(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_BOOLEAN ) {
        return value->v.bval;
	}

	return 0;
}

long handlebars_value_get_intval(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_INTEGER ) {
        return value->v.lval;
	}

	return 0;
}

double handlebars_value_get_floatval(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_FLOAT ) {
        return value->v.dval;
	}

	return 0;
}

void handlebars_value_convert_ex(struct handlebars_value * value, bool recurse)
{
    struct handlebars_value_iterator it;

    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_USER:
            handlebars_value_get_handlers(value)->convert(value, recurse);
            break;
        case HANDLEBARS_VALUE_TYPE_MAP:
        case HANDLEBARS_VALUE_TYPE_ARRAY:
            handlebars_value_iterator_init(&it, value);
            for( ; it.current != NULL; handlebars_value_iterator_next(&it) ) {
                handlebars_value_convert_ex(it.current, recurse);
            }
            break;
        default:
            // do nothing
            break;
    }
}

bool handlebars_value_iterator_init(struct handlebars_value_iterator * it, struct handlebars_value * value)
{
    struct handlebars_map_entry * entry;

    memset(it, 0, sizeof(struct handlebars_value_iterator));

    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_ARRAY:
            it->value = value;
            it->current = handlebars_stack_get(value->v.stack, 0);
            it->length = handlebars_stack_length(value->v.stack);
            return true;

        case HANDLEBARS_VALUE_TYPE_MAP:
            if( value->v.map->i > 0 ) {
                entry = &value->v.map->v[0];
                it->index = 0;
                it->value = value;
                it->key = entry->key;
                it->current = entry->value;
                it->length = value->v.map->i;
                handlebars_value_addref(it->current);
                return true;
            }
            break;

        case HANDLEBARS_VALUE_TYPE_USER:
            return handlebars_value_get_handlers(value)->iterator(it, value);

        default:
            //handlebars_context_throw(value->ctx, HANDLEBARS_ERROR, "Cannot iterator over type %d", value->type);
            break;
    }

    return false;
}
/*
struct handlebars_value_iterator * handlebars_value_iterator_ctor(struct handlebars_value * value)
{
    struct handlebars_value_iterator * it;
    struct handlebars_map_entry * entry;

    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_ARRAY:
            it = MC(handlebars_talloc_zero(value, struct handlebars_value_iterator));
            it->value = value;
            it->current = handlebars_stack_get(value->v.stack, 0);
            it->length = handlebars_stack_length(value->v.stack);
            break;
        case HANDLEBARS_VALUE_TYPE_MAP:
            it = MC(handlebars_talloc_zero(value, struct handlebars_value_iterator));
            entry = value->v.map->first;
            if( entry ) {
                it->value = value;
                it->usr = (void *) entry;
                it->key = entry->key;
                it->current = entry->value;
                it->length = value->v.map->i;
                handlebars_value_addref(it->current);
            }
            break;
        case HANDLEBARS_VALUE_TYPE_USER:
            it = value->handlers->iterator(value);
            break;
        default:
            it = MC(handlebars_talloc_zero(value, struct handlebars_value_iterator));
            //handlebars_context_throw(value->ctx, HANDLEBARS_ERROR, "Cannot iterator over type %d", value->type);
            break;
    }

    return it;
}
*/

bool handlebars_value_iterator_next(struct handlebars_value_iterator * it)
{
    struct handlebars_value * value;
    struct handlebars_map * map;
    struct handlebars_map_entry * entry;
    bool ret = false;

    assert(it != NULL);
    assert(it->value != NULL);

    value = it->value;
    if( it->current != NULL ) {
        handlebars_value_delref(it->current);
        it->current = NULL;
    }

    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_ARRAY:
            if( it->index < handlebars_stack_length(value->v.stack) - 1 ) {
                ret = true;
                it->index++;
                it->current = handlebars_stack_get(value->v.stack, it->index);
            }
            break;
        case HANDLEBARS_VALUE_TYPE_MAP:
            map = value->v.map;
            if( it->index < map->i - 1 ) {
                it->index++;
                entry = &map->v[it->index];
                ret = true;
                it->key = entry->key;
                it->current = entry->value;
                handlebars_value_addref(it->current);
            }
            break;
        case HANDLEBARS_VALUE_TYPE_USER:
            ret = handlebars_value_get_handlers(value)->next(it);
            break;
        default:
            // do nothing
            break;
    }

    return ret;
}

struct handlebars_value * handlebars_value_call(struct handlebars_value * value, HANDLEBARS_HELPER_ARGS)
{
    struct handlebars_value * result = NULL;
    if( value->type == HANDLEBARS_VALUE_TYPE_HELPER ) {
        result = value->v.helper(argc, argv, options);
    } else if( value->type == HANDLEBARS_VALUE_TYPE_USER && handlebars_value_get_handlers(value)->call ) {
        result = handlebars_value_get_handlers(value)->call(value, argc, argv, options);
    }
    return result;
}

char * handlebars_value_dump(struct handlebars_value * value, size_t depth)
{
    char * buf = MC(handlebars_talloc_strdup(CONTEXT, ""));
    struct handlebars_value_iterator it;
    char indent[64];
    char indent2[64];

    if( value == NULL ) {
        handlebars_talloc_strdup_append_buffer(buf, "(nil)");
        return buf;
    }

    memset(indent, 0, sizeof(indent));
    memset(indent, ' ', depth * 4);

    memset(indent2, 0, sizeof(indent2));
    memset(indent2, ' ', (depth + 1) * 4);

    switch( handlebars_value_get_type(value) ) {
        case HANDLEBARS_VALUE_TYPE_BOOLEAN:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "boolean(%s)", value->v.bval ? "true" : "false");
            break;
        case HANDLEBARS_VALUE_TYPE_FLOAT:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "float(%f)", value->v.dval);
            break;
        case HANDLEBARS_VALUE_TYPE_INTEGER:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "integer(%ld)", value->v.lval);
            break;
        case HANDLEBARS_VALUE_TYPE_NULL:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "NULL");
            break;
        case HANDLEBARS_VALUE_TYPE_STRING:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "string(%s)", value->v.string->val);
            break;
        case HANDLEBARS_VALUE_TYPE_ARRAY:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "%s\n", "[");
            handlebars_value_iterator_init(&it, value);
            for( ; it.current != NULL; handlebars_value_iterator_next(&it) ) {
                char * tmp = handlebars_value_dump(it.current, depth + 1);
                buf = handlebars_talloc_asprintf_append_buffer(buf, "%s%ld => %s\n", indent2, it.index, tmp);
                handlebars_talloc_free(tmp);
            }
            buf = handlebars_talloc_asprintf_append_buffer(buf, "%s%s", indent, "]");
            break;
        case HANDLEBARS_VALUE_TYPE_MAP:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "%s\n", "{");
            handlebars_value_iterator_init(&it, value);
            for( ; it.current != NULL; handlebars_value_iterator_next(&it) ) {
                char * tmp = handlebars_value_dump(it.current, depth + 1);
                buf = handlebars_talloc_asprintf_append_buffer(buf, "%s%s => %s\n", indent2, it.key->val, tmp);
                handlebars_talloc_free(tmp);
            }
            buf = handlebars_talloc_asprintf_append_buffer(buf, "%s%s", indent, "}");
            break;
        default:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "unknown type %d", value->type);
            break;
    }

    return MC(buf);
}

char * handlebars_value_expression(struct handlebars_value * value, bool escape)
{
    char * buf = MC(handlebars_talloc_strdup(CONTEXT, ""));
    return handlebars_value_expression_append_buffer(buf, value, escape);
}

char * handlebars_value_expression_append_buffer(char * buf, struct handlebars_value * value, bool escape)
{
    struct handlebars_value_iterator it;

    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_BOOLEAN:
            if( value->v.bval ) {
                buf = handlebars_talloc_strdup_append_buffer(buf, "true");
            } else {
                buf = handlebars_talloc_strdup_append_buffer(buf, "false");
            }
            break;

        case HANDLEBARS_VALUE_TYPE_FLOAT:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "%g", value->v.dval);
            break;

        case HANDLEBARS_VALUE_TYPE_INTEGER:
            buf = handlebars_talloc_asprintf_append_buffer(buf, "%ld", value->v.lval);
            break;

        case HANDLEBARS_VALUE_TYPE_STRING:
            if( escape && !(value->flags & HANDLEBARS_VALUE_FLAG_SAFE_STRING) ) {
                buf = handlebars_htmlspecialchars_append_buffer(buf, value->v.string->val, value->v.string->len);
            } else {
                buf = handlebars_talloc_strndup_append_buffer(buf, value->v.string->val, value->v.string->len);
            }
            break;

        case HANDLEBARS_VALUE_TYPE_USER:
            if( handlebars_value_get_type(value) != HANDLEBARS_VALUE_TYPE_ARRAY ) {
                break;
            }
            // fall-through to array

        case HANDLEBARS_VALUE_TYPE_ARRAY:
            handlebars_value_iterator_init(&it, value);
            bool first = true;
            for( ; it.current != NULL; handlebars_value_iterator_next(&it) ) {
                if( !first ) {
                    buf = MC(handlebars_talloc_strndup_append_buffer(buf, ",", 1));
                }
                buf = handlebars_value_expression_append_buffer(buf, it.current, escape);
                first = false;
            }
            break;

        default:
            // nothing
            break;
    }

    MEMCHK(buf);

    return buf;
}

struct handlebars_value * handlebars_value_copy(struct handlebars_value * value)
{
    struct handlebars_value * new_value = NULL;
    struct handlebars_value_iterator it;

    assert(value != NULL);

    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_ARRAY:
            new_value = handlebars_value_ctor(CONTEXT);
            handlebars_value_array_init(new_value);
            handlebars_value_iterator_init(&it, value);
            for( ; it.current != NULL; handlebars_value_iterator_next(&it) ) {
                handlebars_stack_set(new_value->v.stack, it.index, it.current);
            }
            break;
        case HANDLEBARS_VALUE_TYPE_MAP:
            new_value = handlebars_value_ctor(CONTEXT);
            handlebars_value_map_init(new_value);
            handlebars_value_iterator_init(&it, value);
            for( ; it.current != NULL; handlebars_value_iterator_next(&it) ) {
                handlebars_map_update(new_value->v.map, it.key, it.current);
            }
            break;
        case HANDLEBARS_VALUE_TYPE_USER:
            new_value = handlebars_value_get_handlers(value)->copy(value);
            break;

        default:
            new_value = handlebars_value_ctor(CONTEXT);
            memcpy(&new_value->v, &value->v, sizeof(value->v));
            break;
    }

    return MC(new_value);
}

void handlebars_value_dtor(struct handlebars_value * value)
{
    long restore_flags = 0;

    // Release old value
    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_ARRAY:
            handlebars_stack_dtor(value->v.stack);
            break;
        case HANDLEBARS_VALUE_TYPE_MAP:
            handlebars_map_dtor(value->v.map);
            break;
        case HANDLEBARS_VALUE_TYPE_STRING:
            handlebars_talloc_free(value->v.string);
            break;
        case HANDLEBARS_VALUE_TYPE_USER:
            handlebars_value_get_handlers(value)->dtor(value);
            break;
        case HANDLEBARS_VALUE_TYPE_PTR:
            handlebars_talloc_free(value->v.ptr);
            break;
        default:
            // do nothing
            break;
    }

    if( value->flags & HANDLEBARS_VALUE_FLAG_HEAP_ALLOCATED ) {
        talloc_free_children(value);
        restore_flags = HANDLEBARS_VALUE_FLAG_HEAP_ALLOCATED;
    }

    // Initialize to null
    value->type = HANDLEBARS_VALUE_TYPE_NULL;
    memset(&value->v, 0, sizeof(value->v));
    value->flags = restore_flags;
}
