
#ifndef HANDLEBARS_OPCODE_SERIALIZER_H
#define HANDLEBARS_OPCODE_SERIALIZER_H

#include "handlebars.h"

#ifdef	__cplusplus
extern "C" {
#endif

struct handlebars_program_header {
    size_t size;
    size_t program_count;
    size_t program_offset;
    size_t opcode_count;
    size_t opcode_offset;
    size_t data_offset;
};

struct handlebars_module_table_entry {
    long guid;
    size_t opcode_count;
    size_t opcode_offset;
    struct handlebars_opcode * opcode;
};

struct handlebars_module {
    size_t program_count;
    size_t program_offset;
    struct handlebars_module_table_entry * programs;

    size_t opcode_count;
    size_t opcode_offset;
    struct handlebars_opcode * opcodes;

    size_t data_offset;
    void * data;
};

struct handlebars_program_header * handlebars_program_serialize(
    struct handlebars_context * context,
    struct handlebars_program * program
);

#ifdef	__cplusplus
}
#endif

#endif
