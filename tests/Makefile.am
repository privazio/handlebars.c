
EXTRA_DIST = utils.h

check_PROGRAMS = \
	test_main \
	test_ast \
	test_ast_helpers \
	test_ast_list \
	test_cache \
	test_compiler \
	test_map \
	test_opcode_printer \
	test_opcodes \
	test_scanners \
	test_string \
	test_token \
	test_utils \
	test_value \
	test_spec_handlebars_parser \
	test_spec_handlebars_tokenizer \
	test_spec_handlebars_compiler \
	test_spec_handlebars \
	test_spec_mustache

COMMONFILES = utils.h utils.c fixtures.c adler32.c

test_main_SOURCES = $(COMMONFILES) test_main.c
test_ast_SOURCES = $(COMMONFILES) test_ast.c
test_ast_helpers_SOURCES = $(COMMONFILES) test_ast_helpers.c
test_ast_list_SOURCES = $(COMMONFILES) test_ast_list.c
test_cache_SOURCES = $(COMMONFILES) test_cache.c
test_compiler_SOURCES = $(COMMONFILES) test_compiler.c
test_map_SOURCES = $(COMMONFILES) test_map.c
test_opcode_printer_SOURCES = $(COMMONFILES) test_opcode_printer.c
test_opcodes_SOURCES = $(COMMONFILES) test_opcodes.c
test_scanners_SOURCES = $(COMMONFILES) test_scanners.c
test_string_SOURCES = $(COMMONFILES) test_string.c
test_token_SOURCES = $(COMMONFILES) test_token.c
test_utils_SOURCES = $(COMMONFILES) test_utils.c
test_value_SOURCES = $(COMMONFILES) test_value.c
test_spec_handlebars_parser_SOURCES = $(COMMONFILES) test_spec_handlebars_parser.c
test_spec_handlebars_tokenizer_SOURCES = $(COMMONFILES) test_spec_handlebars_tokenizer.c
test_spec_handlebars_compiler_SOURCES = $(COMMONFILES) test_spec_handlebars_compiler.c
test_spec_handlebars_SOURCES = $(COMMONFILES) test_spec_handlebars.c
test_spec_mustache_SOURCES = $(COMMONFILES) test_spec_mustache.c

TESTS = \
	test_main \
	test_ast \
	test_ast_helpers \
	test_ast_list \
	test_cache \
	test_compiler \
	test_map \
	test_opcode_printer \
	test_opcodes \
	test_scanners \
	test_string \
	test_token \
	test_utils \
	test_value \
	test_spec_handlebars_parser \
	test_spec_handlebars_tokenizer \
	test_spec_handlebars_compiler \
	test_spec_handlebars \
	test_spec_mustache

if HANDLEBARS_MEMORY
check_PROGRAMS += test_random_alloc_fail
test_random_alloc_fail_SOURCES = $(COMMONFILES) test_random_alloc_fail.c
TESTS += test_random_alloc_fail
endif

TESTS_ENVIRONMENT = \
	top_srcdir=$(top_srcdir) \
	top_builddir=$(top_builddir) \
	handlebars_export_dir=@HANDLEBARS_SPEC_DIR@/export \
	handlebars_spec_dir=@HANDLEBARS_SPEC_DIR@/spec \
	handlebars_tokenizer_spec=@HANDLEBARS_SPEC_DIR@/spec/tokenizer.json \
	handlebars_parser_spec=@HANDLEBARS_SPEC_DIR@/spec/parser.json \
	mustache_spec_dir=@MUSTACHE_SPEC_DIR@/specs \
	CK_VERBOSITY=normal \
	CK_TAP_LOG_FILE_NAME=-

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/build/tap-driver.sh

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src
AM_LDFLAGS = -static  # for speed and Valgrind
AM_CFLAGS = @CHECK_CFLAGS@
LDADD = $(top_builddir)/src/libhandlebars.la @CHECK_LIBS@

