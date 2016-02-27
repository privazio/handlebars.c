
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <check.h>
#include <talloc.h>

#include "handlebars.h"
#include "handlebars_memory.h"
#include "handlebars_map.h"
#include "handlebars_value.h"
#include "utils.h"


char mkchar(unsigned long i) {
    return (char) (32 + (i % (126 - 32)));
}


START_TEST(test_map)
{
#define STRSIZE 128
    size_t i, j;
    size_t pos = 0;
    size_t count = 10000;
    char tmp[STRSIZE];
    struct handlebars_map * map = handlebars_map_ctor(context);
    struct handlebars_value * value;
    struct handlebars_map_entry * entry;
    struct handlebars_map_entry * tmp_entry;

    srand(0x5d0);

    for( i = 0; i < count; i++ ) {
        for( j = 0; j < (rand() % STRSIZE); j++ ) {
            tmp[j] = mkchar(rand());
        }
        tmp[j] = 0;

        if( !handlebars_map_str_find(map, tmp, strlen(tmp)) ) {
            value = talloc_steal(map, handlebars_value_ctor(context));
            handlebars_value_integer(value, pos++);
            handlebars_map_str_add(map, tmp, strlen(tmp), value);
        }
    }

    fprintf(stderr, "ENTRIES: %ld, TABLE SIZE: %ld, COLLISIONS: %ld\n", map->i, map->table_size, map->collisions);

    pos = 0;
    for( i = 0; i < map->i; i++ )  {
        ck_assert_uint_eq(pos++, handlebars_value_get_intval(map->v[i].value));
    }

    while( map->i > 0 ) {
        struct handlebars_map_entry * entry = handlebars_map_find(map, map->v[0].key);
        ck_assert_ptr_ne(NULL, entry);
        handlebars_map_remove(map, map->v[0].key);
        ck_assert_uint_eq(--pos, map->i);
    }


    handlebars_map_dtor(map);
}
END_TEST

Suite * parser_suite(void)
{
    Suite * s = suite_create("Map");

    REGISTER_TEST_FIXTURE(s, test_map, "Map");

    return s;
}

int main(void)
{
    int number_failed;
    int memdebug;
    int error;

    talloc_set_log_stderr();

    // Check if memdebug enabled
    memdebug = getenv("MEMDEBUG") ? atoi(getenv("MEMDEBUG")) : 0;
    if( memdebug ) {
        talloc_enable_leak_report_full();
    }

    // Set up test suite
    Suite * s = parser_suite();
    SRunner * sr = srunner_create(s);
    if( IS_WIN || memdebug ) {
        srunner_set_fork_status(sr, CK_NOFORK);
    }
    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    error = (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;

    // Generate report for memdebug
    if( memdebug ) {
        talloc_report_full(NULL, stderr);
    }

    // Return
    return error;
}
