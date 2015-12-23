
/**
 * @file
 * @brief Compiler
 */

#ifndef HANDLEBARS_COMPILER_H
#define HANDLEBARS_COMPILER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stddef.h>

#define HANDLEBARS_COMPILER_STACK_SIZE 64

struct handlebars_ast_node;
struct handlebars_compiler;
struct handlebars_opcode;

/**
 * @brief Array of built-in helpers
 */
extern const char * handlebars_builtins[];

/**
 * @brief Flags to control compiler behaviour
 * @details string_params (1 << 1) and track_ids (1 << 2) were
 *          removed in handlebars.js v5
 */
enum handlebars_compiler_flag {
    /**
     * @brief No flags
     */
    handlebars_compiler_flag_none = 0,
    
    // Option flags

    /**
     * @brief Track depths
     */
    handlebars_compiler_flag_use_depths = (1 << 0),

    /**
     * @brief Disable all escaping
     */
    handlebars_compiler_flag_no_escape = (1 << 3),

    /**
     * @brief Only allow known helpers
     */
    handlebars_compiler_flag_known_helpers_only = (1 << 4),
    
    /**
     * @brief Prevent partial indent
     */
    handlebars_compiler_flag_prevent_indent = (1 << 5),

    handlebars_compiler_flag_use_data = (1 << 6),

    handlebars_compiler_flag_explicit_partial_context = (1 << 7),
    
    handlebars_compiler_flag_ignore_standalone = (1 << 8),
    
    handlebars_compiler_flag_alternate_decorators = (1 << 9),

    // Composite option flags

    /**
     * @brief Mustache compatibility
     */
    handlebars_compiler_flag_compat = (1 << 0),

    /**
     * @brief All flags
     */
    handlebars_compiler_flag_all = ((1 << 10) - 1)
};

enum handlebars_compiler_result_flag {
    handlebars_compiler_result_flag_use_depths = (1 << 0),
    handlebars_compiler_result_flag_use_partial = (1 << 1),
    handlebars_compiler_result_flag_is_simple = (1 << 2),
    handlebars_compiler_result_flag_use_decorators = (1 << 3),
    /**
     * @brief All flags
     */
    handlebars_compiler_result_flag_all = ((1 << 4) - 1)
};

/**
 * @brief Compiler error codes
 */
enum handlebars_compiler_error {
    handlebars_compiler_error_none = 0,

    /**
     * @brief The compiler encountered a memory allocation failure
     */
    handlebars_compiler_error_nomem = 1,

    /**
     * @brief The compiler encountered an unknown helper in known helpers only mode
     */
    handlebars_compiler_error_unknown_helper = 2,

    handlebars_compiler_error_unsupported_partial_args = 3,
    
    handlebars_compiler_error_block_param_stack_blown = 4,
    
    handlebars_compiler_error_source_node_stack_blown = 5
};

/**
 * @brief Sexpr types
 */
enum handlebars_compiler_sexpr_type {
    handlebars_compiler_sexpr_type_ambiguous = 0,
    handlebars_compiler_sexpr_type_helper = 1,
    handlebars_compiler_sexpr_type_simple = 2
};

struct handlebars_block_param_stack {
    /**
     * @brief Block param stack
     */
    struct {
        char * block_param1;
        char * block_param2;
    } s[HANDLEBARS_COMPILER_STACK_SIZE];
    
    /**
     * @brief Block param stack index
     */
    int i;
};

struct handlebars_source_node_stack {
    /**
     * @brief Source node stack
     */
    struct handlebars_ast_node * s[HANDLEBARS_COMPILER_STACK_SIZE];
    
    /**
     * @brief Source node stack index
     */
    int i;
};

/**
 * @brief Main compiler state struct
 */
struct handlebars_compiler {
    enum handlebars_compiler_error errnum;
    char * error;
    
    struct handlebars_opcode ** opcodes;
    size_t opcodes_length;
    size_t opcodes_size;
    
    struct handlebars_compiler ** children;
    size_t children_length;
    size_t children_size;
    
    struct handlebars_compiler ** decorators;
    size_t decorators_length;
    size_t decorators_size;
    
    struct handlebars_block_param_stack * bps;
    struct handlebars_source_node_stack sns;
    
    /**
     * @brief Array of known helpers
     */
    const char ** known_helpers;
    
    /**
     * @brief Symbol index counter
     */
    long guid;
    
    /**
     * @brief Compiler flags
     */
    int flags;
    
    // Option flags
    short string_params;
    short track_ids;
    short use_depths;
    short no_escape;
    short known_helpers_only;
    short prevent_indent;
    short use_data;
    short explicit_partial_context;
    short ignore_standalone;
    short alternate_decorators;
    
    /**
     * @brief Result flags
     */
    int result_flags;
    
    /**
     * @brief Number of block params used
     */
    int block_params;
};

/**
 * @brief Main compile function. Compiles an AST
 *
 * @param[in] compiler The compiler context
 * @param[in] node The AST node to compile
 */
void handlebars_compiler_compile(
        struct handlebars_compiler * compiler, struct handlebars_ast_node * node);

/**
 * @brief Construct a compiler context object.
 * 
 * @param[in] ctx The memory context
 * @return the compiler context pointer
 */
struct handlebars_compiler * handlebars_compiler_ctor(void * ctx);

/**
 * @brief Free a compiler context and it's resources.
 * 
 * @param[in] compiler
 * @return void
 */
void handlebars_compiler_dtor(struct handlebars_compiler * compiler);

/**
 * @brief Get the compiler flags.
 * 
 * @param[in] compiler
 * @return the compiler flags
 */
int handlebars_compiler_get_flags(struct handlebars_compiler * compiler);

/**
 * @brief Set the compiler flags, with handlebars_compiler_flag_all as a 
 *        mask.
 * 
 * @param[in] compiler
 * @param[in] flags
 * @return void
 */
void handlebars_compiler_set_flags(struct handlebars_compiler * compiler, int flags);

#ifdef	__cplusplus
}
#endif

#endif
