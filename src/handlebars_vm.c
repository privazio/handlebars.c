
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "handlebars.h"
#include "handlebars_private.h"
#include "handlebars_compiler.h"
#include "handlebars_value.h"
#include "handlebars_map.h"
#include "handlebars_memory.h"
#include "handlebars_opcodes.h"
#include "handlebars_opcode_printer.h"
#include "handlebars_stack.h"
#include "handlebars_utils.h"
#include "handlebars_vm.h"



#define OPCODE_NAME(name) handlebars_opcode_type_ ## name
#define ACCEPT(name) case OPCODE_NAME(name) : ACCEPT_FN(name)(vm, opcode); break;
#define ACCEPT_FN(name) accept_ ## name
#define ACCEPT_NAMED_FUNCTION(name) static void name (struct handlebars_vm * vm, struct handlebars_opcode * opcode)
#define ACCEPT_FUNCTION(name) ACCEPT_NAMED_FUNCTION(ACCEPT_FN(name))

#define LEN(stack) stack.i
#define BOTTOM(stack) stack.v[0]
#define GET(stack, pos) handlebars_value_addref2(stack.v[stack.i - pos - 1])
#define TOP(stack) handlebars_value_addref2(stack.i > 0 ? stack.v[stack.i - 1] : NULL)
#define POP(stack) (stack.i > 0 ? stack.v[--stack.i] : NULL)
#define PUSH(stack, value) do { \
        if( stack.i < HANDLEBARS_VM_STACK_SIZE ) { \
            stack.v[stack.i++] = value; \
        } else { \
            handlebars_context_throw(vm, HANDLEBARS_STACK_OVERFLOW, "Stack overflow in %s", #stack); \
        } \
    } while(0)

#define TOPCONTEXT TOP(vm->contextStack)


struct setup_ctx {
    //const char * name;
    struct handlebars_string * name;
    size_t param_size;
    struct handlebars_stack * params;
    struct handlebars_options * options;
    //bool is_block_helper;
};

ACCEPT_FUNCTION(push_context);




#undef CONTEXT
#define CONTEXT ctx

struct handlebars_vm * handlebars_vm_ctor(struct handlebars_context * ctx)
{
    return MC(handlebars_talloc_zero(ctx, struct handlebars_vm));
}

#undef CONTEXT
#define CONTEXT HBSCTX(vm)


void handlebars_vm_dtor(struct handlebars_vm * vm)
{
    handlebars_talloc_free(vm);
}

static inline struct handlebars_value * call_helper(struct handlebars_options * options, const char * name, unsigned int len)
{
    struct handlebars_value * helper;
    struct handlebars_value * result;
    handlebars_helper_func fn;
    if( NULL != (helper = handlebars_value_map_str_find(options->vm->helpers, name, len)) ) {
        result = handlebars_value_call(helper, options);
        handlebars_value_delref(helper);
        return result;
    } else if( NULL != (fn = handlebars_builtins_find(name, len)) ) {
        return fn(options);
    }
    return NULL;
}

static inline void setup_options(struct handlebars_vm * vm, struct setup_ctx * ctx)
{
    struct handlebars_options * options = MC(handlebars_talloc_zero(vm, struct handlebars_options));
    struct handlebars_value * inverse;
    struct handlebars_value * program;
    size_t i, j;
    struct handlebars_value * params[ctx->param_size];

    ctx->options = options;

    options->name = ctx->name ? MC(handlebars_talloc_strndup(options, ctx->name->val, ctx->name->len)) : NULL;
    options->hash = POP(vm->stack);
    options->scope = TOPCONTEXT;
    options->vm = vm;

    // programs
    inverse = POP(vm->stack);
    program = POP(vm->stack);
    options->inverse = inverse ? handlebars_value_get_intval(inverse) : -1;
    options->program = program ? handlebars_value_get_intval(program) : -1;
    handlebars_value_try_delref(inverse);
    handlebars_value_try_delref(program);

    // params
    if( !ctx->params ) {
        ctx->params = handlebars_stack_ctor(CONTEXT);
    }

    i = ctx->param_size;
    while( i-- ) {
        params[i] = POP(vm->stack);
    }
    for( i = 0; i < ctx->param_size; i++ ) {
        handlebars_stack_set(ctx->params, i, params[i]);
        handlebars_value_delref(params[i]);
    }

    // Params
    options->params = talloc_steal(options, ctx->params);

    // Data
    // @todo check useData
    if( vm->data ) {
        options->data = vm->data;
        handlebars_value_addref(vm->data);
    //} else if( vm->flags & handlebars_compiler_flag_use_data ) {
    } else {
        options->data = handlebars_value_ctor(CONTEXT);
    }
}

static inline void append_to_buffer(struct handlebars_vm * vm, struct handlebars_value * result, bool escape)
{
    if( result ) {
        vm->buffer = handlebars_value_expression_append_buffer(vm->buffer, result, escape);
        handlebars_value_delref(result);
    }
}

static inline void depthed_lookup(struct handlebars_vm * vm, const char * key)
{
    size_t i;
    size_t l;
    struct handlebars_value * value = NULL;
    struct handlebars_value * tmp;

    for( i = 0, l = LEN(vm->contextStack); i < l; i++ ) {
        value = GET(vm->contextStack, i);
        if( !value ) continue;
        if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_MAP ) {
            tmp = handlebars_value_map_str_find(value, key, strlen(key));
            if( tmp != NULL ) {
                handlebars_value_delref(tmp);
                break;
            }
        }
        handlebars_value_delref(value);
    }

    if( !value ) {
        value = handlebars_value_ctor(CONTEXT);
    }

    PUSH(vm->stack, value);
}

static inline char * dump_stack(struct handlebars_stack * stack)
{
    struct handlebars_value * tmp = handlebars_value_ctor(stack->ctx);
    char * str;
    tmp->type = HANDLEBARS_VALUE_TYPE_ARRAY;
    tmp->v.stack = stack;
    str = handlebars_value_dump(tmp, 0);
    talloc_steal(stack, str);
    handlebars_talloc_free(tmp);
    return str;
}










ACCEPT_FUNCTION(ambiguous_block_value)
{
    struct handlebars_value * current;
    struct handlebars_value * result;
    struct handlebars_value * helper;
    struct setup_ctx ctx = {0};

    ctx.name = vm->last_helper;
    ctx.params = handlebars_stack_ctor(CONTEXT);
    handlebars_stack_push(ctx.params, TOPCONTEXT);
    setup_options(vm, &ctx);

    current = POP(vm->stack);
    if( !current ) { // @todo I don't think this should happen
        current = handlebars_value_ctor(CONTEXT);
    }
    handlebars_stack_set(ctx.params, 0, current);

    if( vm->last_helper == NULL ) {
        result = call_helper(ctx.options, "blockHelperMissing", sizeof("blockHelperMissing") - 1);
        append_to_buffer(vm, result, 0);
    }

    handlebars_options_dtor(ctx.options);
    handlebars_value_delref(current);
}

ACCEPT_FUNCTION(append)
{
    struct handlebars_value * value = POP(vm->stack);
    append_to_buffer(vm, value, 0);
}

ACCEPT_FUNCTION(append_escaped)
{
    struct handlebars_value * value = POP(vm->stack);
    append_to_buffer(vm, value, 1);
}

ACCEPT_FUNCTION(append_content)
{
    assert(opcode->type == handlebars_opcode_type_append_content);
    assert(opcode->op1.type == handlebars_operand_type_string);

    vm->buffer = MC(handlebars_talloc_strndup_append_buffer(vm->buffer, opcode->op1.data.string->val, opcode->op1.data.string->len));
}

ACCEPT_FUNCTION(assign_to_hash)
{
    struct handlebars_value * hash = TOP(vm->hashStack);
    struct handlebars_value * value = POP(vm->stack);

    assert(opcode->op1.type == handlebars_operand_type_string);
    assert(hash->type == HANDLEBARS_VALUE_TYPE_MAP);

    handlebars_map_update(hash->v.map, opcode->op1.data.string, value);

    handlebars_value_delref(hash);
    handlebars_value_delref(value);
}

ACCEPT_FUNCTION(block_value)
{
    struct handlebars_value * current;
    struct handlebars_value * result;
    struct setup_ctx ctx = {0};

    assert(opcode->op1.type == handlebars_operand_type_string);

    ctx.params = handlebars_stack_ctor(CONTEXT);
    //handlebars_stack_push(ctx.params, TOPCONTEXT);
    ctx.name = opcode->op1.data.string;
    setup_options(vm, &ctx);

    current = POP(vm->stack);
    assert(current != NULL);
    handlebars_stack_set(ctx.params, 0, current);

    result = call_helper(ctx.options, "blockHelperMissing", sizeof("blockHelperMissing") - 1);
    append_to_buffer(vm, result, 0);

    handlebars_value_delref(current);
    handlebars_options_dtor(ctx.options);
}

ACCEPT_FUNCTION(empty_hash)
{
    struct handlebars_value * value = handlebars_value_ctor(CONTEXT);
    handlebars_value_map_init(value);
    PUSH(vm->stack, value);
}

ACCEPT_FUNCTION(get_context)
{
    assert(opcode->type == handlebars_opcode_type_get_context);
    assert(opcode->op1.type == handlebars_operand_type_long);

    size_t depth = (size_t) opcode->op1.data.longval;
    size_t length = LEN(vm->contextStack);

    if( depth >= length ) {
        // @todo should we throw?
        vm->last_context = NULL;
    } else if( depth == 0 ) {
        vm->last_context = TOP(vm->contextStack);
    } else {
        vm->last_context = GET(vm->contextStack, depth);
    }
}

ACCEPT_FUNCTION(invoke_ambiguous)
{
    struct handlebars_value * value = POP(vm->stack);
    struct setup_ctx ctx = {0};
    struct handlebars_value * result;
    struct handlebars_value * fn;

    ACCEPT_FN(empty_hash)(vm, opcode);

    assert(opcode->op1.type == handlebars_operand_type_string);
    assert(opcode->op2.type == handlebars_operand_type_boolean);

    ctx.name = opcode->op1.data.string;
    //ctx.is_block_helper = opcode->op2.data.boolval;
    setup_options(vm, &ctx);
    vm->last_helper = NULL;

    if( NULL != (result = call_helper(ctx.options, ctx.name->val, ctx.name->len)) ) {
        append_to_buffer(vm, result, 0);
        vm->last_helper = ctx.name;
    } else if( value && handlebars_value_is_callable(value) ) {
        result = handlebars_value_call(value, ctx.options);
        assert(result != NULL);
        PUSH(vm->stack, result);
    } else {
        result = call_helper(ctx.options, "helperMissing", sizeof("helperMissing") - 1);
        append_to_buffer(vm, result, 0);
        PUSH(vm->stack, value);
    }

    handlebars_options_dtor(ctx.options);
}

ACCEPT_FUNCTION(invoke_helper)
{
    struct handlebars_value * value = POP(vm->stack);
    struct handlebars_value * result = NULL;
    struct setup_ctx ctx = {0};

    assert(opcode->op1.type == handlebars_operand_type_long);
    assert(opcode->op2.type == handlebars_operand_type_string);
    assert(opcode->op3.type == handlebars_operand_type_boolean);

    ctx.name = opcode->op2.data.string;
    ctx.param_size = opcode->op1.data.longval;
    setup_options(vm, &ctx);

    if( opcode->op3.data.boolval ) { // isSimple
        if( NULL != (result = call_helper(ctx.options, ctx.name->val, ctx.name->len)) ) {
            goto done;
        }
    }

    if( value && handlebars_value_is_callable(value) ) {
        result = handlebars_value_call(value, ctx.options);
    } else {
        result = call_helper(ctx.options, "helperMissing", sizeof("helperMissing") - 1);
    }

done:
    if( !result ) {
        result = handlebars_value_ctor(CONTEXT);
    }

    PUSH(vm->stack, result);

    handlebars_options_dtor(ctx.options);
    handlebars_value_delref(value);
}

ACCEPT_FUNCTION(invoke_known_helper)
{
    struct setup_ctx ctx = {0};
    struct handlebars_value * result;

    assert(opcode->op1.type == handlebars_operand_type_long);
    assert(opcode->op2.type == handlebars_operand_type_string);

    ctx.param_size = opcode->op1.data.longval;
    ctx.name = opcode->op2.data.string;
    setup_options(vm, &ctx);

    result = call_helper(ctx.options, ctx.name->val, ctx.name->len);

    if( result == NULL ) {
        handlebars_context_throw(CONTEXT, HANDLEBARS_ERROR, "Invalid known helper: %s", ctx.name->val);
    }

    PUSH(vm->stack, result);
    handlebars_options_dtor(ctx.options);
}

ACCEPT_FUNCTION(invoke_partial)
{
    struct setup_ctx ctx = {0};
    struct handlebars_value * tmp;
    char * name = NULL;
    jmp_buf buf;

    assert(opcode->op1.type == handlebars_operand_type_boolean);
    assert(opcode->op2.type == handlebars_operand_type_string || opcode->op2.type == handlebars_operand_type_null || opcode->op2.type == handlebars_operand_type_long);
    assert(opcode->op3.type == handlebars_operand_type_string);

    ctx.params = handlebars_stack_ctor(CONTEXT);
    ctx.param_size = 1;
    setup_options(vm, &ctx);

    if( opcode->op2.type == handlebars_operand_type_long ) {
        name = MC(handlebars_talloc_asprintf(vm, "%ld", opcode->op2.data.longval));
    } else if( opcode->op2.type == handlebars_operand_type_string ) {
        name = opcode->op2.data.string->val;
    }
    if( opcode->op1.data.boolval ) {
        tmp = POP(vm->stack);
        if( tmp ) { // @todo why is this null?
            name = handlebars_value_get_strval(tmp);
            handlebars_value_delref(tmp);
        }
        ctx.options->name = NULL; // fear
    }

    struct handlebars_value * partial = NULL;
    if( /*!opcode->op1.data.boolval*/ name ) {
        partial = handlebars_value_map_str_find(vm->partials, name, strlen(name));
    } /* else {
        partial = handlebars_value_ctor(CONTEXT);
        handlebars_value_string(partial, name);
    } */

    if( !partial ) {
        if( vm->flags & handlebars_compiler_flag_compat ) {
            return;
        } else {
            handlebars_context_throw(CONTEXT, HANDLEBARS_ERROR, "The partial %s could not be found", name);
        }
    }

    // If partial is a function?
    if( handlebars_value_is_callable(partial) ) {
        struct handlebars_value * tmp = partial;
        partial = handlebars_value_call(tmp, ctx.options);
        handlebars_value_delref(tmp);
    }

    // Construct new context
    struct handlebars_context * context = handlebars_context_ctor_ex(vm);

    // Save jump buffer
    context->jmp = &buf;
    if( setjmp(buf) ) {
        handlebars_context_throw_ex(CONTEXT, context->num, &context->loc, context->msg);
    }

    // Construct parser
    struct handlebars_parser * parser = handlebars_parser_ctor(context);
    parser->tmpl = handlebars_value_get_strval(partial);
    if( !*parser->tmpl ) {
        goto done;
    }

    // Construct intermediate compiler and VM
    struct handlebars_compiler * compiler = handlebars_compiler_ctor(context);
    struct handlebars_vm * vm2 = handlebars_vm_ctor(context);

    // Parse
    handlebars_parse(parser);

    // Compile
    handlebars_compiler_set_flags(compiler, vm->flags);
    handlebars_compiler_compile(compiler, parser->program);

    // Get context
    // @todo change parent to new vm?
    struct handlebars_value * context1 = handlebars_stack_get(ctx.options->params, 0);
    struct handlebars_value * context2 = NULL;
    struct handlebars_value_iterator * it;
    if( context1 && handlebars_value_get_type(context1) == HANDLEBARS_VALUE_TYPE_MAP ) {
        context2 = handlebars_value_ctor(&vm2->ctx);
        handlebars_value_map_init(context2);
        it = handlebars_value_iterator_ctor(context1);
        for( ; it->current ; handlebars_value_iterator_next(it) ) {
            handlebars_map_str_update(context2->v.map, it->key, strlen(it->key), it->current);
        }
        handlebars_talloc_free(it);
        if( ctx.options->hash && ctx.options->hash->type == HANDLEBARS_VALUE_TYPE_MAP ) {
            it = handlebars_value_iterator_ctor(ctx.options->hash);
            for( ; it->current ; handlebars_value_iterator_next(it) ) {
                handlebars_map_str_update(context2->v.map, it->key, strlen(it->key), it->current);
            }
            handlebars_talloc_free(it);
        }
    } else if( !context1 || context1->type == HANDLEBARS_VALUE_TYPE_NULL ) {
        context2 = ctx.options->hash;
        if( context2 ) {
            handlebars_value_addref(context2);
        }
    } else {
        context2 = context1;
        context1 = NULL;
    }

    // Setup new VM
    vm2->depth = vm->depth + 1;
    vm2->flags = vm->flags;
    vm2->helpers = vm->helpers;
    vm2->partials = vm->partials;

    // Copy stacks
    memcpy(&vm2->contextStack, &vm->contextStack, offsetof(struct handlebars_vm_stack, v) + (sizeof(struct handlebars_value *) * LEN(vm->contextStack)));
    memcpy(&vm2->blockParamStack, &vm->blockParamStack, offsetof(struct handlebars_vm_stack, v) + (sizeof(struct handlebars_value *) * LEN(vm->blockParamStack)));

    handlebars_vm_execute(vm2, compiler, context2);

    if( vm2->buffer ) {
        char *tmp2 = handlebars_indent(vm2, vm2->buffer, opcode->op3.data.string->val);
        vm->buffer = MC(handlebars_talloc_strdup_append_buffer(vm->buffer, tmp2));
        handlebars_talloc_free(tmp2);
    }

    handlebars_value_try_delref(context1);
    handlebars_value_try_delref(context2);
    handlebars_vm_dtor(vm2);
    handlebars_compiler_dtor(compiler);

done:
    handlebars_context_dtor(context);
    handlebars_value_try_delref(partial);
    handlebars_options_dtor(ctx.options);
}

ACCEPT_FUNCTION(lookup_block_param)
{
    long blockParam1 = -1;
    long blockParam2 = -1;
    struct handlebars_value * value = NULL;

    assert(opcode->op1.type == handlebars_operand_type_array);
    assert(opcode->op2.type == handlebars_operand_type_array);

    sscanf(opcode->op1.data.array[0]->val, "%ld", &blockParam1);
    sscanf(opcode->op1.data.array[1]->val, "%ld", &blockParam2);

    if( blockParam1 >= LEN(vm->blockParamStack) ) goto done;

    struct handlebars_value * v1 = GET(vm->blockParamStack, blockParam1);
    if( !v1 || handlebars_value_get_type(v1) != HANDLEBARS_VALUE_TYPE_ARRAY ) goto done;

    struct handlebars_value * v2 = handlebars_value_array_find(v1, blockParam2);
    if( !v2 ) goto done;

    struct handlebars_string ** arr = opcode->op2.data.array;
    arr++;
    if( *arr ) {
        struct handlebars_value * tmp = v2;
        struct handlebars_value * tmp2;
        for(  ; *arr != NULL; arr++ ) {
            tmp2 = handlebars_value_map_str_find(tmp, (*arr)->val, (*arr)->len);
            if( !tmp2 ) {
                break;
            } else {
                handlebars_value_delref(tmp);
                tmp = tmp2;
            }
        }
        value = tmp;
    } else {
        value = v2;
    }

done:
    if( !value ) {
        value = handlebars_value_ctor(CONTEXT);
    }
    PUSH(vm->stack, value);
}

ACCEPT_FUNCTION(lookup_data)
{
    struct handlebars_value * data = vm->data;
    struct handlebars_value * tmp;
    struct handlebars_value * val = NULL;

    assert(opcode->op1.type == handlebars_operand_type_long);
    assert(opcode->op2.type == handlebars_operand_type_array);
    assert(opcode->op3.type == handlebars_operand_type_boolean || opcode->op3.type == handlebars_operand_type_null);

    size_t depth = opcode->op1.data.longval;
    struct handlebars_string ** arr = opcode->op2.data.array;
    struct handlebars_string * first = *arr++;

    if( depth && data ) {
        handlebars_value_addref(data);
        while( data && depth-- ) {
            tmp = handlebars_value_map_str_find(data, HBS_STRL("_parent"));
            handlebars_value_delref(data);
            data = tmp;
        }
    }

    if( data && (tmp = handlebars_value_map_str_find(data, first->val, first->len)) ) {
        val = tmp;
    } else if( 0 == strcmp(first->val, "root") ) {
        val = BOTTOM(vm->contextStack);
    }

    if( val ) {
        for (; *arr != NULL; arr++) {
            struct handlebars_string * part = *arr;
            if (val == NULL || handlebars_value_get_type(val) != HANDLEBARS_VALUE_TYPE_MAP) {
                break;
            }
            tmp = handlebars_value_map_str_find(val, part->val, part->len);
            handlebars_value_delref(val);
            val = tmp;
        }
    }

    if( !val ) {
        val = handlebars_value_ctor(CONTEXT);
    }

    PUSH(vm->stack, val);
}

ACCEPT_FUNCTION(lookup_on_context)
{
    assert(opcode->op1.type == handlebars_operand_type_array);
    assert(opcode->op2.type == handlebars_operand_type_boolean || opcode->op2.type == handlebars_operand_type_null);
    assert(opcode->op3.type == handlebars_operand_type_boolean || opcode->op3.type == handlebars_operand_type_null);
    assert(opcode->op4.type == handlebars_operand_type_boolean || opcode->op4.type == handlebars_operand_type_null);

    struct handlebars_string ** arr = opcode->op1.data.array;
    long index = -1;

    if( !opcode->op4.data.boolval && (vm->flags & handlebars_compiler_flag_compat) ) {
        depthed_lookup(vm, (*arr)->val);
    } else {
        ACCEPT_FN(push_context)(vm, opcode);
    }

    struct handlebars_value * value = TOP(vm->stack);
    struct handlebars_value * tmp;

    if( value ) {
        do {
            if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_MAP ) {
                tmp = handlebars_value_map_str_find(value, (*arr)->val, (*arr)->len);
                handlebars_value_try_delref(value);
                value = tmp;
            } else if( handlebars_value_get_type(value) == HANDLEBARS_VALUE_TYPE_ARRAY ) {
                if( sscanf((*arr)->val, "%ld", &index) ) {
                    tmp = handlebars_value_array_find(value, index);
                } else {
                    tmp = NULL;
                }
                handlebars_value_try_delref(value);
                value = tmp;
            } else {
                value = NULL;
            }
            if( !value ) {
                break;
            }
        } while( *++arr );
    }

    if( !value ) {
        value = handlebars_value_ctor(CONTEXT);
    }

    handlebars_value_delref(POP(vm->stack));
    PUSH(vm->stack, value);
}

ACCEPT_FUNCTION(pop_hash)
{
    struct handlebars_value * hash = POP(vm->hashStack);
    if( !hash ) {
        hash = handlebars_value_ctor(CONTEXT);
    }
    PUSH(vm->stack, hash);
}

ACCEPT_FUNCTION(push_context)
{
    struct handlebars_value * value = vm->last_context;

    if( !value ) {
        value = handlebars_value_ctor(CONTEXT);
    } else {
        handlebars_value_addref(value);
    }

    PUSH(vm->stack, value);
}

ACCEPT_FUNCTION(push_hash)
{
    struct handlebars_value * hash = handlebars_value_ctor(CONTEXT);
    handlebars_value_map_init(hash);
    PUSH(vm->hashStack, hash);
}

ACCEPT_FUNCTION(push_program)
{
    struct handlebars_value * value = handlebars_value_ctor(CONTEXT);

    if( opcode->op1.type == handlebars_operand_type_long ) {
        handlebars_value_integer(value, opcode->op1.data.longval);
    } else {
        handlebars_value_integer(value, -1);
    }

    PUSH(vm->stack, value);
}

ACCEPT_FUNCTION(push_literal)
{
    struct handlebars_value * value = handlebars_value_ctor(CONTEXT);

    switch( opcode->op1.type ) {
        case handlebars_operand_type_string:
            // @todo we should move this to the parser
            if( 0 == strcmp(opcode->op1.data.string->val, "undefined") ) {
                break;
            } else if( 0 == strcmp(opcode->op1.data.string->val, "null") ) {
                break;
            }
            handlebars_value_str(value, opcode->op1.data.string);
            break;
        case handlebars_operand_type_boolean:
            handlebars_value_boolean(value, opcode->op1.data.boolval);
            break;
        case handlebars_operand_type_long:
            handlebars_value_integer(value, opcode->op1.data.longval);
            break;
        case handlebars_operand_type_null:
            break;

        case handlebars_operand_type_array:
        default:
            assert(0);
            break;
    }

    PUSH(vm->stack, value);
}

ACCEPT_FUNCTION(push_string)
{
    struct handlebars_value * value = handlebars_value_ctor(CONTEXT);

    assert(opcode->op1.type == handlebars_operand_type_string);

    handlebars_value_str(value, opcode->op1.data.string);
    PUSH(vm->stack, value);
}

ACCEPT_FUNCTION(resolve_possible_lambda)
{
    struct handlebars_value * top = TOP(vm->stack);
    struct handlebars_value * result;

    assert(top != NULL);
    if( handlebars_value_is_callable(top) ) {
        struct handlebars_options * options = MC(handlebars_talloc_zero(vm, struct handlebars_options));
        options->params = handlebars_stack_ctor(CONTEXT);
        handlebars_stack_set(options->params, 0, TOPCONTEXT);
        options->vm = vm;
        options->scope = TOPCONTEXT;
        result = handlebars_value_call(top, options);
        if( !result ) {
            result = handlebars_value_ctor(CONTEXT);
        }
        handlebars_value_delref(POP(vm->stack));
        PUSH(vm->stack, result);
        handlebars_options_dtor(options);
    }
    handlebars_value_delref(top);
}

void handlebars_vm_accept(struct handlebars_vm * vm, struct handlebars_compiler * compiler)
{
	size_t i = compiler->opcodes_length;
    struct handlebars_opcodes ** opcodes = compiler->opcodes;

	for( ; i > 0; i-- ) {
		struct handlebars_opcode * opcode = *opcodes++;

        // Print opcode?
#ifndef NDEBUG
        if( getenv("DEBUG") ) {
            char *tmp = handlebars_opcode_print(vm, opcode);
            fprintf(stdout, "V[%d] P[%d] OPCODE: %s\n", vm->depth, compiler->guid, tmp);
            talloc_free(tmp);
        }
#endif

		switch( opcode->type ) {
            ACCEPT(ambiguous_block_value);
            ACCEPT(append)
            ACCEPT(append_escaped)
            ACCEPT(append_content);
            ACCEPT(assign_to_hash);
            ACCEPT(block_value);
            ACCEPT(get_context);
            ACCEPT(empty_hash);
            ACCEPT(invoke_ambiguous);
            ACCEPT(invoke_helper);
            ACCEPT(invoke_known_helper);
            ACCEPT(invoke_partial);
            ACCEPT(lookup_block_param);
            ACCEPT(lookup_data);
            ACCEPT(lookup_on_context);
            ACCEPT(pop_hash);
            ACCEPT(push_context);
            ACCEPT(push_hash);
            ACCEPT(push_program);
            ACCEPT(push_literal);
            ACCEPT(push_string);
            //ACCEPT(push_string_param);
            ACCEPT(resolve_possible_lambda);
            default:
                handlebars_context_throw(CONTEXT, HANDLEBARS_ERROR, "Unhandled opcode: %s\n", handlebars_opcode_readable_type(opcode->type));
                break;
        }
	}
}

char * handlebars_vm_execute_program_ex(
        struct handlebars_vm * vm, int program, struct handlebars_value * context,
        struct handlebars_value * data, struct handlebars_value * block_params)
{
    bool pushed_context = false;
    bool pushed_depths = false;
    bool pushed_block_param = false;

    if( program < 0 ) {
        return NULL;
    }
    if( program >= vm->guid_index ) {
        assert(program < vm->guid_index);
        return NULL;
    }

    // Get compiler
	struct handlebars_compiler * compiler = vm->programs[program];

    // Save and set buffer
    char * prevBuffer = vm->buffer;
    vm->buffer = MC(handlebars_talloc_strdup(vm, ""));

    // Push the context stack
    if( LEN(vm->contextStack) <= 0 || TOP(vm->contextStack) != context ) {
        PUSH(vm->contextStack, context);
        pushed_context = true;
    }

    // Save and set data
    struct handlebars_value * prevData = vm->data;
    if( data ) {
        vm->data = data;
    }

    // Set block params
    if( block_params ) {
        PUSH(vm->blockParamStack, block_params);
        pushed_block_param = true;
    }

    // Execute the program
	handlebars_vm_accept(vm, compiler);

    // Pop context stack
    if( pushed_context ) {
        POP(vm->contextStack);
    }

    // Pop block params
    if( pushed_block_param ) {
        POP(vm->blockParamStack);
    }

    // Restore data
    vm->data = prevData;

    // Restore buffer
    char * buffer = vm->buffer;
    vm->buffer = prevBuffer;

    return buffer;
}

char * handlebars_vm_execute_program(struct handlebars_vm * vm, int program, struct handlebars_value * context)
{
    return handlebars_vm_execute_program_ex(vm, program, context, NULL, NULL);
}

static void preprocess_opcode(struct handlebars_vm * vm, struct handlebars_opcode * opcode, struct handlebars_compiler * compiler)
{
    long program;
    struct handlebars_compiler * child;

    if( opcode->type == handlebars_opcode_type_push_program ) {
        if( opcode->op1.type == handlebars_operand_type_long && !opcode->op4.data.boolval ) {
            program = opcode->op1.data.longval;
            assert(program < compiler->children_length);
            child = compiler->children[program];
            opcode->op1.data.longval = child->guid;
            opcode->op4.data.boolval = 1;
        }
    }
}

static void preprocess_program(struct handlebars_vm * vm, struct handlebars_compiler * compiler) {
    size_t i;

    compiler->guid = vm->guid_index++;

    // Realloc
    if( compiler->guid >= talloc_array_length(vm->programs) ) {
        vm->programs = MC(handlebars_talloc_realloc(vm, vm->programs, struct handlebars_compiler *, talloc_array_length(vm->programs) * 2));
    }

    vm->programs[compiler->guid] = compiler;

    for( i = 0; i < compiler->children_length; i++ ) {
        preprocess_program(vm, compiler->children[i]);
    }

    for( i = 0; i < compiler->opcodes_length; i++ ) {
        preprocess_opcode(vm, compiler->opcodes[i], compiler);
    }
}


void handlebars_vm_execute(
		struct handlebars_vm * vm, struct handlebars_compiler * compiler,
		struct handlebars_value * context)
{
    jmp_buf * prev = HBSCTX(vm)->jmp;
    jmp_buf buf;

    // Save jump buffer
    if( !prev ) {
        if( handlebars_setjmp_ex(vm, &buf) ) {
            goto done;
        }
    }

    // Preprocess
    if( compiler->programs ) {
        vm->programs = compiler->programs;
        vm->guid_index = compiler->programs_index;
    } else {
        vm->programs = MC(handlebars_talloc_array(vm, struct handlebars_compiler *, 32));
        preprocess_program(vm, compiler);
        compiler->programs = talloc_steal(compiler, vm->programs);
        compiler->programs_index = vm->guid_index;
    }

    // Save context
    handlebars_value_addref(context);
    vm->context = context;

    // Execute
    vm->buffer = handlebars_vm_execute_program_ex(vm, 0, context, vm->data, NULL);

done:
    // Release context
    handlebars_value_delref(context);

    HBSCTX(vm)->jmp = prev;
}
