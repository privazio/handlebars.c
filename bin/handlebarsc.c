
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "handlebars_ast.h"
#include "handlebars_ast_printer.h"
#include "handlebars_context.h"
#include "handlebars_memory.h"
#include "handlebars_token.h"
#include "handlebars_token_list.h"
#include "handlebars_token_printer.h"
#include "handlebars_utils.h"
#include "handlebars.tab.h"
#include "handlebars.lex.h"

#define __BUFF_SIZE 1024
char * stdin_buf;

static void readStdin(void)
{
    char buffer[__BUFF_SIZE];
    stdin_buf = talloc_strdup(NULL, "");
    while (fgets(buffer, __BUFF_SIZE, stdin) != NULL) {
        stdin_buf = talloc_strdup_append_buffer(stdin_buf, buffer);
    }
    
    if( stdin_buf == NULL ) {
        exit(1);
    } else if( strlen(stdin_buf) <= 0 ) {
        exit(1);
    }
}

static int do_usage(void)
{
    fprintf(stderr, "Usage: handlebarsc [lex|parse] [TEMPLATE]\n");
}

static int do_lex(void)
{
    struct handlebars_context * ctx;
    struct handlebars_token * token = NULL;
    int token_int = 0;
    char * output;
    YYSTYPE yylval_param;
    YYLTYPE yylloc_param;
    
    readStdin();
    ctx = handlebars_context_ctor();
    ctx->tmpl = stdin_buf;
    
    // Run
    do {
        token_int = handlebars_yy_lex(&yylval_param, &yylloc_param, ctx->scanner);
        if( token_int == END || token_int == INVALID ) {
            break;
        }
        YYSTYPE * lval = handlebars_yy_get_lval(ctx->scanner);
        
        // Make token object
        char * text = (lval->text == NULL ? "" : lval->text);
        token = handlebars_token_ctor(token_int, text, strlen(text), ctx);
        
        // Print token
        output = handlebars_token_print(token, 0);
        fprintf(stdout, "%s\n", output);
        fflush(stdout);
    } while( token && token_int != END && token_int != INVALID );
    
    return 0;
}

static int do_parse(void)
{
    struct handlebars_context * ctx;
    char * output;
    int retval;
    char errlinestr[32];
    int error = 0;
    
    readStdin();
    ctx = handlebars_context_ctor();
    ctx->tmpl = stdin_buf;
    
    retval = handlebars_yy_parse(ctx);
    
    if( ctx->error != NULL ) {
        snprintf(errlinestr, sizeof(errlinestr), " on line %d, column %d", 
                ctx->errloc->last_line, 
                ctx->errloc->last_column);
        
        output = handlebars_talloc_strdup(ctx, ctx->error);
        output = handlebars_talloc_strdup_append(output, errlinestr);
        fprintf(stdout, "%s\n", output);
        error = 1;
    } else {
        errno = 0;
        
        //char * output = handlebars_ast_print(ctx->program, 0);
        struct handlebars_ast_printer_context printctx = handlebars_ast_print2(ctx->program, 0);
        //_handlebars_ast_print(ctx->program, &printctx);
        char * output = printctx.output;
        
        fprintf(stdout, "%s\n", output);
        
        handlebars_talloc_free(output);
    }
    
    return error;
}

int main( int argc, char * argv[])
{
    if( argc <= 1 ) {
        return do_usage();
    }
    
    if( strcmp(argv[1], "lex") == 0 ) {
        return do_lex();
    } else if( strcmp(argv[1], "parse") == 0 ) {
        return do_parse();
    } else {
        return do_usage();
    }
}