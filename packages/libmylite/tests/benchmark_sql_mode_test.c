#include "mylite_benchmark_csv.h"
#include "mylite_benchmark_sql_mode.h"
#include "sql/mylite_lexer.h"
#include "sql/mylite_parser.h"

#include <stdio.h>
#include <string.h>

static int test_sql_mode_replay(void);
static int load_and_assign(const char *csv, struct mylite_benchmark_owned_query_list *out_queries);

struct sql_mode_expectation {
    size_t index;
    unsigned int modes;
    const char *context;
};

static int expect_modes(
    const struct mylite_benchmark_owned_query_list *queries,
    const struct sql_mode_expectation *expectation
);
static int expect_parse_ok(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t index,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_sql_mode_replay();

    return failures == 0 ? 0 : 1;
}

static int test_sql_mode_replay(void) {
    enum {
        ansi = MYLITE_SQL_MODE_ANSI_QUOTES,
        ignore_space = MYLITE_SQL_MODE_IGNORE_SPACE,
        no_backslash = MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES,
        pipes = MYLITE_SQL_MODE_PIPES_AS_CONCAT,
    };

    enum sql_mode_query_index {
        query_set_ansi_quotes,
        query_create_quoted,
        query_save_and_clear,
        query_select_literal,
        query_restore,
        query_select_full,
        query_set_global,
        query_select_after_global,
        query_list_drop,
        query_select_string,
        query_list_add,
        query_mod_call,
        query_concat,
        query_pipe_concat,
        query_set_no_backslash,
        query_backslash_literal,
        query_set_default,
        query_set_zero,
        query_set_ansi,
        query_select_ansi,
    };

    static const char csv[] = "SET SQL_MODE='ANSI_QUOTES'\n"
                              "CREATE TABLE \"quoted\" (i INT)\n"
                              "SET @old_sql_mode=@@SQL_MODE, SQL_MODE=''\n"
                              "SELECT \"literal\"\n"
                              "SET SQL_MODE=@old_sql_mode\n"
                              "SELECT * FROM \"full\"\n"
                              "SET @@GLOBAL.sql_mode='IGNORE_SPACE'\n"
                              "SELECT * FROM \"after_global\"\n"
                              "SET sql_mode = sys.LIST_DROP(@@sql_mode, 'ANSI_QUOTES')\n"
                              "SELECT \"string\"\n"
                              "SET sql_mode = sys.LIST_ADD(@@sql_mode, 'IGNORE_SPACE')\n"
                              "SELECT MOD (5,2)\n"
                              "SET sql_mode = CONCAT(@@sql_mode, ',PIPES_AS_CONCAT')\n"
                              "SELECT 1 || 2\n"
                              "SET @@session.sql_mode = NO_BACKSLASH_ESCAPES\n"
                              "SELECT 'a\\\\b'\n"
                              "SET SQL_MODE=DEFAULT\n"
                              "SET SQL_MODE=0\n"
                              "SET SQL_MODE=ANSI\n"
                              "SELECT * FROM \"ansi\"\n";
    struct mylite_benchmark_owned_query_list queries = {0};
    int failures = load_and_assign(csv, &queries);

    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_set_ansi_quotes,
            .modes = 0U,
            .context = "initial direct SET",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_create_quoted,
            .modes = ansi,
            .context = "ANSI_QUOTES create table",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_save_and_clear,
            .modes = ansi,
            .context = "save and clear starts in ANSI_QUOTES",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_select_literal,
            .modes = 0U,
            .context = "cleared mode string literal",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_restore,
            .modes = 0U,
            .context = "restore statement starts cleared",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_select_full,
            .modes = ansi,
            .context = "restored ANSI_QUOTES",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_set_global,
            .modes = ansi,
            .context = "global assignment starts ANSI_QUOTES",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_select_after_global,
            .modes = ansi,
            .context = "global assignment did not affect session",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_list_drop,
            .modes = ansi,
            .context = "LIST_DROP starts ANSI_QUOTES",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_select_string,
            .modes = 0U,
            .context = "LIST_DROP removed ANSI_QUOTES",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_list_add,
            .modes = 0U,
            .context = "LIST_ADD starts empty",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_mod_call,
            .modes = ignore_space,
            .context = "LIST_ADD added IGNORE_SPACE",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_concat,
            .modes = ignore_space,
            .context = "CONCAT starts IGNORE_SPACE",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_pipe_concat,
            .modes = ignore_space | pipes,
            .context = "CONCAT added PIPES_AS_CONCAT",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_set_no_backslash,
            .modes = ignore_space | pipes,
            .context = "session bare mode assignment starts combined",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_backslash_literal,
            .modes = no_backslash,
            .context = "bare NO_BACKSLASH_ESCAPES",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_set_default,
            .modes = no_backslash,
            .context = "DEFAULT starts no-backslash",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_set_zero,
            .modes = 0U,
            .context = "DEFAULT cleared modes",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_set_ansi,
            .modes = 0U,
            .context = "zero mode assignment starts empty",
        }
    );
    failures += expect_modes(
        &queries,
        &(struct sql_mode_expectation){
            .index = query_select_ansi,
            .modes = ansi | ignore_space | pipes,
            .context = "ANSI combination expansion",
        }
    );
    failures += expect_parse_ok(&queries, query_create_quoted, "ANSI quoted CREATE TABLE");
    failures += expect_parse_ok(&queries, query_select_full, "ANSI quoted SELECT");
    failures += expect_parse_ok(&queries, query_mod_call, "IGNORE_SPACE function call");
    failures += expect_parse_ok(&queries, query_pipe_concat, "PIPES_AS_CONCAT expression");
    failures += expect_parse_ok(&queries, query_select_ansi, "ANSI combination quoted SELECT");

    mylite_benchmark_owned_query_list_deinit(&queries);
    return failures;
}

static int load_and_assign(const char *csv, struct mylite_benchmark_owned_query_list *out_queries) {
    int rc = mylite_benchmark_parse_single_column_csv(csv, strlen(csv), out_queries);

    if (rc != 0) {
        fprintf(stderr, "expected CSV parse success, got %d\n", rc);
        return 1;
    }
    rc = mylite_benchmark_assign_sql_modes(out_queries);
    if (rc != 0) {
        fprintf(stderr, "expected SQL mode assignment success, got %d\n", rc);
        return 1;
    }
    return 0;
}

static int expect_modes(
    const struct mylite_benchmark_owned_query_list *queries,
    const struct sql_mode_expectation *expectation
) {
    unsigned int actual = 0U;

    if (expectation == NULL || expectation->index >= queries->count) {
        const char *context = expectation == NULL ? "mode expectation" : expectation->context;
        size_t index = expectation == NULL ? 0U : expectation->index;

        fprintf(stderr, "%s: missing query %zu\n", context, index);
        return 1;
    }
    actual = queries->items[expectation->index].modes;
    if (actual != expectation->modes) {
        fprintf(
            stderr,
            "%s: expected modes 0x%x, got 0x%x\n",
            expectation->context,
            expectation->modes,
            actual
        );
        return 1;
    }
    return 0;
}

static int expect_parse_ok(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t index,
    const char *context
) {
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (index >= queries->count) {
        fprintf(stderr, "%s: missing query %zu\n", context, index);
        return 1;
    }
    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = queries->items[index].sql,
            .length = queries->items[index].length,
            .modes = queries->items[index].modes,
        },
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    if (status != MYLITE_SQL_PARSE_OK) {
        fprintf(
            stderr,
            "%s: expected parse OK, got %s\n",
            context,
            mylite_sql_parse_status_name(status)
        );
        return 1;
    }
    return 0;
}
