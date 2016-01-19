
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_JSON_C_JSON_H)
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#elif defined(HAVE_JSON_JSON_H)
#include <json/json.h>
#include <json/json_object.h>
#include <json/json_tokener.h>
#endif

#include "handlebars_memory.h"

#include "handlebars_data.h"


/* JSON object handers */

static void std_json_dtor(struct handlebars_value * value) {
	struct json_object * result = (struct json_object *) value->v.usr;
	// Decrement reference counter
	json_object_put(result);
}

static enum handlebars_value_type std_json_type(struct handlebars_value * value) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	switch( json_object_get_type(intern) ) {
		default:
		case json_type_null: return HANDLEBARS_VALUE_TYPE_NULL;
		case json_type_boolean: return HANDLEBARS_VALUE_TYPE_BOOLEAN;
		case json_type_double: return HANDLEBARS_VALUE_TYPE_FLOAT;
		case json_type_int: return HANDLEBARS_VALUE_TYPE_INTEGER;
		case json_type_object: return HANDLEBARS_VALUE_TYPE_MAP;
		case json_type_array: return HANDLEBARS_VALUE_TYPE_ARRAY;
		case json_type_string: return HANDLEBARS_VALUE_TYPE_STRING;
	}
}

static struct handlebars_value * std_json_map_find(struct handlebars_value * value, const char * key, size_t len) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	struct json_object * item = json_object_object_get(intern, key);
	return item ? handlebars_value_from_json_object(value, item) : NULL;
}

static struct handlebars_value * std_json_array_find(struct handlebars_value * value, size_t index) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	struct json_object * item = json_object_array_get_idx(intern, (int) index);
	return item ? handlebars_value_from_json_object(value, item) : NULL;
}

static const char * std_json_strval(struct handlebars_value * value) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	const char * ret = NULL;
	if( json_object_is_type(intern, json_type_string) ) {
		ret = json_object_get_string(intern);
	}
	return ret;
}

static size_t std_json_strlen(struct handlebars_value * value) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	size_t ret = 0;
	if( json_object_is_type(intern, json_type_string) ) {
		ret = (size_t) json_object_get_string_len(intern);
	}
	return ret;
}

static short std_json_boolval(struct handlebars_value * value) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	json_bool ret = 0;
	if( json_object_is_type(intern, json_type_boolean) ) {
		ret = json_object_get_boolean(intern);
	}
	return (short) ret;
}

static long std_json_intval(struct handlebars_value * value) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	long ret = 0;
	if( json_object_is_type(intern, json_type_int) ) {
		// @todo make sure sizing is correct
		ret = (long) json_object_get_int64(intern);
	}
	return ret;
}

static double std_json_floatval(struct handlebars_value * value) {
	struct json_object * intern = (struct json_object *) value->v.usr;
	double ret = 0;
	if( json_object_is_type(intern, json_type_double) ) {
		// @todo make sure sizing is correct
		ret = json_object_get_double(intern);
	}
	return ret;
}

static struct handlebars_value_handlers handlebars_value_std_json_handlers = {
		&std_json_type,
		&std_json_map_find,
		&std_json_array_find,
		&std_json_strval,
		&std_json_strlen,
		&std_json_boolval,
		&std_json_intval,
		&std_json_floatval
};

struct handlebars_value_handlers * handlebars_value_get_std_json_handlers()
{
	return &handlebars_value_std_json_handlers;
}



/* Generic handlers */

enum handlebars_value_type handlebars_value_get_type(struct handlebars_value * value) {
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		return value->handlers->type(value);
	} else {
		return value->type;
	}
}

struct handlebars_value * handlebars_value_map_find(struct handlebars_value * value, const char * key, size_t len)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_MAP ) {
			return value->handlers->map_find(value, key, len);
		}
	}

	return NULL;
}

struct handlebars_value * handlebars_value_array_find(struct handlebars_value * value, size_t index)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_ARRAY ) {
			return value->handlers->array_find(value, index);
		}
	}

	return NULL;
}

const char * handlebars_value_get_strval(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_STRING ) {
			return value->handlers->strval(value);
		}
	} else if( value->type == HANDLEBARS_VALUE_TYPE_STRING ) {
		// @todo
	}

	return NULL;
}

size_t handlebars_value_get_strlen(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_STRING ) {
			return value->handlers->strlen(value);
		}
	} else if( value->type == HANDLEBARS_VALUE_TYPE_STRING ) {
		// @todo
	}

	return 0;
}

short handlebars_value_get_boolval(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_BOOLEAN ) {
			return value->handlers->boolval(value);
		}
	} else if( value->type == HANDLEBARS_VALUE_TYPE_BOOLEAN ) {
		// @todo
	}

	return 0;
}

long handlebars_value_get_intval(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_INTEGER ) {
			return value->handlers->intval(value);
		}
	} else if( value->type == HANDLEBARS_VALUE_TYPE_INTEGER ) {
		// @todo
	}

	return 0;
}

double handlebars_value_get_floatval(struct handlebars_value * value)
{
	if( value->type == HANDLEBARS_VALUE_TYPE_USER ) {
		if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_FLOAT ) {
			return value->handlers->floatval(value);
		}
	} else if( value->type == HANDLEBARS_VALUE_TYPE_FLOAT ) {
		// @todo
	}

	return 0;
}





/* General */


struct handlebars_value * handlebars_value_from_json_object(void *ctx, struct json_object *json)
{
	struct handlebars_value * ret;

	ret = handlebars_talloc(ctx, struct handlebars_value);
	if( ret ) {
		ret->type = HANDLEBARS_VALUE_TYPE_USER;
		ret->handlers = handlebars_value_get_std_json_handlers();
		ret->v.usr = (void *) json;
	}

	return ret;
}

struct handlebars_value * handlebars_value_from_json_string(void *ctx, const char * json)
{
	struct handlebars_value * ret;
	struct json_object * result = json_tokener_parse(json);
	if( result ) {
		ret = handlebars_value_from_json_object(ctx, result);
		if( ret ) {
			talloc_set_destructor(ret, std_json_dtor);
		}
	}
	return ret;
}