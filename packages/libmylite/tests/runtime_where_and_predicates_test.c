#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    inserted_numbers_result_row_count = 5,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_data_out_of_range = 1264,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_result {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    size_t warning_count;
    int64_t affected_rows;
    const char *context;
};

static int test_where_and_predicates(void);
static int test_where_xor_predicates(void);
static int test_where_scalar_literal_predicates(void);
static int test_independent_where_and_handles(void);
static int seed_database(mylite_db *database);
static int reset_numbers(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_result(mylite_db *database, struct expected_result expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_where_and_predicates();
    failures += test_where_xor_predicates();
    failures += test_where_scalar_literal_predicates();
    failures += test_independent_where_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_where_and_predicates(void) {
    static const char *const and_row[] = {"2"};
    static const char *const nested_row[] = {"2"};
    static const char *const null_row[] = {"3"};
    static const char *const distinct_row[] = {"9"};
    static const char *const count_row[] = {"1"};
    static const char *const scalar_all_count_row[] = {"3"};
    static const char *const scalar_true_and_row[] = {"1"};
    static const char *const max_row[] = {"2147483647"};
    static const char *const grouped_rows[] = {"1", "1", "2", "2"};
    static const char *const copy_row[] = {"2", "1", "9"};
    static const char *const inserted_rows[] = {
        "1",
        "-2",
        "2",
        "1",
        "2",
        "1",
        "3",
        "2147483647",
        "4",
        "0",
    };
    static const char *const replaced_row[] = {"2", "1"};
    static const char *const update_rows[] = {"1", NULL, "2", "11", "3", NULL, "4", "9"};
    static const char *const update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        "99",
        "4",
        "99",
    };
    static const char *const delete_rows[] = {"1", "2", "4"};
    static const char *const delete_limited_rows[] = {"1", "2", "3"};
    static const char *const persisted_row[] = {"11"};
    static const char *const warning_row[] = {
        "Warning",
        "1287",
        "'&&' is deprecated and will be removed in a future release. Please use AND instead",
    };
    static const char *const or_rows[] = {"2", "4"};
    static const char *const or_precedence_rows[] = {"2"};
    static const char *const or_null_rows[] = {"1", "2", "3", "4"};
    static const char *const or_distinct_rows[] = {NULL, "9"};
    static const char *const or_count_row[] = {"3"};
    static const char *const or_max_row[] = {"2147483647"};
    static const char *const or_grouped_rows[] = {"1", "2", "2", "2"};
    static const char *const or_copy_rows[] = {"2", "1", "9", "4", "0", "9"};
    static const char *const or_inserted_rows[] = {"2", "1", "4", "0"};
    static const char *const or_replaced_rows[] = {"2", "1", "4", "0"};
    static const char *const or_update_rows[] = {"1", NULL, "2", "11", "3", NULL, "4", "11"};
    static const char *const or_update_null_rows[] = {"1", NULL, "2", NULL, "3", NULL, "4", "9"};
    static const char *const or_update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        "99",
        "4",
        "99",
    };
    static const char *const or_delete_rows[] = {"1", "3"};
    static const char *const or_delete_limited_rows[] = {"1", "2"};
    static const char *const or_persisted_rows[] = {"22", "22"};
    static const char *const or_warning_rows[] = {"1", "2", "4"};
    static const char *const or_warning_table[] = {
        "Warning",
        "1287",
        "'|| as a synonym for OR' is deprecated and will be removed in a future release. Please "
        "use OR instead",
        "Warning",
        "1287",
        "'|| as a synonym for OR' is deprecated and will be removed in a future release. Please "
        "use OR instead",
    };
    static const char *const not_rows[] = {"1", "3", "4"};
    static const char *const not_null_safe_rows[] = {"1", "3"};
    static const char *const not_null_rows[] = {"2", "4"};
    static const char *const not_parenthesized_rows[] = {"1", "3"};
    static const char *const not_precedence_rows[] = {"1", "4"};
    static const char *const not_repeated_rows[] = {"2"};
    static const char *const not_distinct_rows[] = {"9"};
    static const char *const not_count_row[] = {"2"};
    static const char *const not_max_row[] = {"2147483647"};
    static const char *const not_grouped_rows[] = {"1", "1", "2", "2"};
    static const char *const not_copy_rows[] = {
        "1",
        "-2",
        NULL,
        "3",
        "2147483647",
        NULL,
        "4",
        "0",
        "9",
    };
    static const char *const not_inserted_rows[] = {"1", "-2", "3", "2147483647", "4", "0"};
    static const char *const not_replaced_rows[] = {"1", "-2", "3", "2147483647", "4", "0"};
    static const char *const not_update_rows[] = {"1", "11", "2", "9", "3", "11", "4", "9"};
    static const char *const not_update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        NULL,
        "4",
        NULL,
    };
    static const char *const not_delete_rows[] = {"1", "2", "3"};
    static const char *const not_delete_limited_rows[] = {"1", "3", "4"};
    static const char *const not_persisted_rows[] = {"44"};
    static const char *const between_rows[] = {"1", "2", "4"};
    static const char *const between_nullable_rows[] = {"2", "4"};
    static const char *const between_not_rows[] = {"3"};
    static const char *const between_precedence_rows[] = {"1", "3"};
    static const char *const between_bool_rows[] = {"2", "4"};
    static const char *const between_unsigned_rows[] = {"1", "2"};
    static const char *const between_all_rows[] = {"1", "2", "3", "4"};
    static const char *const between_distinct_rows[] = {NULL, "9"};
    static const char *const between_count_row[] = {"3"};
    static const char *const between_max_row[] = {"1"};
    static const char *const between_grouped_rows[] = {"1", "2", "2", "1"};
    static const char *const between_copy_rows[] = {
        "1",
        "-2",
        NULL,
        "2",
        "1",
        "9",
        "4",
        "0",
        "9",
    };
    static const char *const between_inserted_rows[] = {"1", "-2", "2", "1", "4", "0"};
    static const char *const between_replaced_rows[] = {"1", "-2", "2", "1", "4", "0"};
    static const char *const between_update_rows[] = {"1", "11", "2", "11", "3", NULL, "4", "11"};
    static const char *const between_update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        NULL,
        "4",
        NULL,
    };
    static const char *const between_delete_rows[] = {"3"};
    static const char *const between_delete_limited_rows[] = {"1", "2", "3"};
    static const char *const between_warning_rows[] = {"1"};
    static const char *const between_persisted_rows[] = {"7", "7", "7"};
    static const char *const in_duplicate_rows[] = {"2", "4"};
    static const char *const in_not_rows[] = {"3"};
    static const char *const in_nullable_rows[] = {"2", "4"};
    static const char *const in_precedence_rows[] = {"1", "3"};
    static const char *const in_bool_rows[] = {"2", "4"};
    static const char *const in_unsigned_boundary_rows[] = {"1", "3"};
    static const char *const in_bigint_boundary_rows[] = {"1", "3"};
    static const char *const in_warning_rows[] = {"1"};
    static const char *const in_persisted_rows[] = {"8", "8", "8"};
    static const char *const is_true_rows[] = {"1", "2", "3"};
    static const char *const is_false_rows[] = {"4"};
    static const char *const is_unknown_rows[] = {"1", "3"};
    static const char *const is_not_unknown_rows[] = {"2", "4"};
    static const char *const is_all_rows[] = {"1", "2", "3", "4"};
    static const char *const is_precedence_rows[] = {"1", "4"};
    static const char *const is_or_precedence_rows[] = {"2", "4"};
    static const char *const is_unsigned_false_rows[] = {"1"};
    static const char *const is_unsigned_true_rows[] = {"2", "3", "4"};
    static const char *const is_distinct_rows[] = {NULL, "9"};
    static const char *const is_count_row[] = {"2"};
    static const char *const is_grouped_rows[] = {"1", "2", "2", "1"};
    static const char *const is_copy_rows[] = {"1", "-2", NULL, "3", "2147483647", NULL};
    static const char *const is_inserted_rows[] = {"4", "0"};
    static const char *const is_replaced_rows[] = {"2", "1", "4", "0"};
    static const char *const is_update_rows[] = {"1", "11", "2", "9", "3", "11", "4", "9"};
    static const char *const is_update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        "22",
        "4",
        "9",
    };
    static const char *const is_delete_limited_rows[] = {"1", "2", "3"};
    static const char *const is_persisted_rows[] = {"6", "6", "6"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open lifecycle database");
    failures += seed_database(database);
    failures += reset_numbers(database);

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 AND nn = 6",
            .values = and_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE (i = 1 AND (nn = 6 AND n IS NOT NULL))",
            .values = nested_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "nested and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NULL AND nn = 7",
            .values = null_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is null and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 && nn = 6",
            .values = and_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "deprecated symbolic and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "deprecated symbolic and warning",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 OR nn = 8 ORDER BY id",
            .values = or_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 OR nn = 8 AND n IS NULL ORDER BY id",
            .values = or_precedence_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "or and precedence predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE (i = 1 OR nn = 8) AND n IS NULL ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "parenthesized or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NULL OR n = 9 ORDER BY id",
            .values = or_null_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "or null predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE id = 1 || nn = 8 || i = 1 ORDER BY id",
            .values = or_warning_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 2U,
            .affected_rows = 0,
            .context = "deprecated symbolic or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = or_warning_table,
            .column_count = 3U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "deprecated symbolic or warnings",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT i = 1 ORDER BY id",
            .values = not_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison not predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT n <=> 9 ORDER BY id",
            .values = not_null_safe_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "null-safe comparison not predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT n IS NULL ORDER BY id",
            .values = not_null_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not is null predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT (i = 1 OR nn = 8) ORDER BY id",
            .values = not_parenthesized_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not parenthesized predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT i = 1 AND nn = 5 OR id = 4 ORDER BY id",
            .values = not_precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not and or precedence predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT NOT i = 1 ORDER BY id",
            .values = not_repeated_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "repeated not predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT n = 9",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not unknown predicate",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i BETWEEN -2 AND 1 ORDER BY id",
            .values = between_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "inclusive between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i BETWEEN 1 AND -2 ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reversed between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n BETWEEN 1 AND 9 ORDER BY id",
            .values = between_nullable_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "nullable between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n NOT BETWEEN 1 AND 9 ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not between unknown predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i NOT BETWEEN -2 AND 1 ORDER BY id",
            .values = between_not_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT i BETWEEN -2 AND 1 ORDER BY id",
            .values = between_not_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "prefix not between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i BETWEEN -2 AND 1 AND nn = 5 OR id = 3 "
                   "ORDER BY id",
            .values = between_precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "between and or precedence predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i BETWEEN FALSE AND TRUE ORDER BY id",
            .values = between_bool_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "boolean bound between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE iu BETWEEN 0 AND 2 ORDER BY id",
            .values = between_unsigned_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "unsigned int between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE iu BETWEEN 0 AND 4294967295 ORDER BY id",
            .values = between_all_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "unsigned int boundary between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE b BETWEEN -9223372036854775808 AND 8 "
                   "ORDER BY id",
            .values = between_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "bigint boundary between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE bu BETWEEN 0 AND 9223372036854775807 "
                   "ORDER BY id",
            .values = between_all_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "unsigned bigint boundary between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i BETWEEN -2 AND 1 && nn = 5",
            .values = between_warning_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "deprecated symbolic and between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "deprecated symbolic and between warning",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IN (-2, 1, 0) ORDER BY id",
            .values = between_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IN (1, 1, 0) ORDER BY id",
            .values = in_duplicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "duplicate in predicate values",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i NOT IN (-2, 1, 0) ORDER BY id",
            .values = in_not_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT i IN (-2, 1, 0) ORDER BY id",
            .values = in_not_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "prefix not in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IN (NULL, 9) ORDER BY id",
            .values = in_nullable_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "nullable in predicate with null list value",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IN (NULL, 8)",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "nullable in predicate without match",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n NOT IN (NULL, 9)",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not in predicate with null list value",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n NOT IN (9)",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not in predicate over nullable column",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IN (FALSE, TRUE) ORDER BY id",
            .values = in_bool_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "boolean in predicate values",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE iu IN (0, 4294967295) ORDER BY id",
            .values = in_unsigned_boundary_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "unsigned int boundary in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers "
                   "WHERE b IN (-9223372036854775808, 9223372036854775807) ORDER BY id",
            .values = in_bigint_boundary_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "bigint boundary in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE ia IN (-2, 2147483647) ORDER BY id",
            .values = in_bigint_boundary_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "integer alias boundary in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE bu IN (0, 9223372036854775807) ORDER BY id",
            .values = in_unsigned_boundary_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "unsigned bigint boundary in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IN (-2, 1) AND nn = 5 OR id = 3 "
                   "ORDER BY id",
            .values = in_precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "in and or precedence predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IN (-2, 1) && nn = 5",
            .values = in_warning_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "deprecated symbolic and in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "deprecated symbolic and in warning",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IS TRUE ORDER BY id",
            .values = is_true_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IS FALSE ORDER BY id",
            .values = is_false_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is false predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS UNKNOWN ORDER BY id",
            .values = is_unknown_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is unknown predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IS NOT TRUE ORDER BY id",
            .values = is_false_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is not true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NOT TRUE ORDER BY id",
            .values = is_unknown_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "nullable is not true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IS NOT FALSE ORDER BY id",
            .values = is_true_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is not false predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NOT FALSE ORDER BY id",
            .values = is_all_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "nullable is not false predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NOT UNKNOWN ORDER BY id",
            .values = is_not_unknown_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is not unknown predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT n IS UNKNOWN ORDER BY id",
            .values = is_not_unknown_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "prefix not is unknown predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IS TRUE AND nn = 5 OR id = 4 ORDER BY id",
            .values = is_precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is and or precedence predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IS FALSE OR nn = 6 AND n IS TRUE ORDER BY id",
            .values = is_or_precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is or and precedence predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE iu IS FALSE ORDER BY id",
            .values = is_unsigned_false_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "unsigned is false predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE ia IS TRUE ORDER BY id",
            .values = is_true_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "integer alias is true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE b IS TRUE ORDER BY id",
            .values = is_all_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "bigint is true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE bu IS TRUE ORDER BY id",
            .values = is_unsigned_true_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "unsigned bigint is true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IS TRUE && nn = 5",
            .values = in_warning_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "deprecated symbolic and is predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "deprecated symbolic and is warning",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE n IS NOT NULL AND tie = 1",
            .values = distinct_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE i >= 0 AND n IS NULL",
            .values = count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE nn >= 6 AND n IS NULL",
            .values = max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE id > 1 AND nn >= 6 GROUP BY tie "
                   "ORDER BY tie",
            .values = grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source and predicate",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE n IS NULL OR tie = 1 ORDER BY n",
            .values = or_distinct_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n = 9 OR nn = 7",
            .values = or_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE nn = 6 OR n IS NULL",
            .values = or_max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE id = 1 OR nn >= 6 GROUP BY tie "
                   "ORDER BY tie",
            .values = or_grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source or predicate",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE NOT n IS NULL ORDER BY n",
            .values = not_distinct_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source not predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE NOT n IS NULL",
            .values = not_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source not predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE NOT nn = 6",
            .values = not_max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source not predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE NOT id = 2 GROUP BY tie ORDER BY tie",
            .values = not_grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source not predicate",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE i BETWEEN -2 AND 1 ORDER BY n",
            .values = between_distinct_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE i BETWEEN -2 AND 1",
            .values = between_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE i BETWEEN -2 AND 1",
            .values = between_max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source between predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE i BETWEEN -2 AND 1 GROUP BY tie "
                   "ORDER BY tie",
            .values = between_grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source between predicate",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE i IN (-2, 1, 0) ORDER BY n",
            .values = between_distinct_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE i IN (-2, 1, 0)",
            .values = between_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE i IN (-2, 1, 0)",
            .values = between_max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source in predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE i IN (-2, 1, 0) GROUP BY tie "
                   "ORDER BY tie",
            .values = between_grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source in predicate",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE i IS TRUE ORDER BY n",
            .values = is_distinct_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source is predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n IS UNKNOWN",
            .values = is_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source is predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE i IS TRUE",
            .values = max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source is predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE i IS TRUE GROUP BY tie ORDER BY tie",
            .values = is_grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source is predicate",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_numbers SELECT id, i, n FROM numbers WHERE i = 1 AND n IS NOT NULL",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "create table select affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_numbers ORDER BY id",
            .values = copy_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select copied rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_or_numbers SELECT id, i, n FROM numbers WHERE i = 1 OR nn = 8",
        &result
    );
    failures += expect_int64(
        mylite_result_affected_rows(result),
        2,
        "create table select or affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_or_numbers ORDER BY id",
            .values = or_copy_rows,
            .column_count = 3U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select or copied rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_not_numbers SELECT id, i, n FROM numbers WHERE NOT i = 1",
        &result
    );
    failures += expect_int64(
        mylite_result_affected_rows(result),
        3,
        "create table select not affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_not_numbers ORDER BY id",
            .values = not_copy_rows,
            .column_count = 3U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select not copied rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_between_numbers SELECT id, i, n FROM numbers WHERE i BETWEEN -2 AND 1",
        &result
    );
    failures += expect_int64(
        mylite_result_affected_rows(result),
        3,
        "create table select between affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_between_numbers ORDER BY id",
            .values = between_copy_rows,
            .column_count = 3U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select between copied rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_in_numbers SELECT id, i, n FROM numbers WHERE i IN (-2, 1, 0)",
        &result
    );
    failures += expect_int64(
        mylite_result_affected_rows(result),
        3,
        "create table select in affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_in_numbers ORDER BY id",
            .values = between_copy_rows,
            .column_count = 3U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select in copied rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_is_numbers SELECT id, i, n FROM numbers WHERE n IS UNKNOWN",
        &result
    );
    failures += expect_int64(
        mylite_result_affected_rows(result),
        2,
        "create table select is affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_is_numbers ORDER BY id",
            .values = is_copy_rows,
            .column_count = 3U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select is copied rows",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE inserted_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO inserted_numbers SELECT id, i FROM numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO inserted_numbers SELECT id, i FROM numbers WHERE i = 1 AND n IS NOT NULL",
        &result
    );
    failures += expect_int64(mylite_result_affected_rows(result), 1, "insert select affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM inserted_numbers ORDER BY id",
            .values = inserted_rows,
            .column_count = 2U,
            .row_count = inserted_numbers_result_row_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source and predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE or_inserted_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO or_inserted_numbers SELECT id, i FROM numbers WHERE i = 1 OR nn = 8",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "insert select or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM or_inserted_numbers ORDER BY id",
            .values = or_inserted_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source or predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE not_inserted_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO not_inserted_numbers SELECT id, i FROM numbers WHERE NOT i = 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 3, "insert select not affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM not_inserted_numbers ORDER BY id",
            .values = not_inserted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source not predicate",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE between_inserted_numbers (id INT NOT NULL, i INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO between_inserted_numbers SELECT id, i FROM numbers "
        "WHERE i BETWEEN -2 AND 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 3, "insert select between affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM between_inserted_numbers ORDER BY id",
            .values = between_inserted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source between predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE in_inserted_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO in_inserted_numbers SELECT id, i FROM numbers WHERE i IN (-2, 1, 0)",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 3, "insert select in affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM in_inserted_numbers ORDER BY id",
            .values = between_inserted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source in predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE is_inserted_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO is_inserted_numbers SELECT id, i FROM numbers WHERE i IS FALSE",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "insert select is affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM is_inserted_numbers ORDER BY id",
            .values = is_inserted_rows,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source is predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE replaced_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO replaced_numbers SELECT id, i FROM numbers WHERE i = 1 AND n IS NOT NULL",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "replace select affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM replaced_numbers ORDER BY id",
            .values = replaced_row,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source and predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE or_replaced_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO or_replaced_numbers SELECT id, i FROM numbers WHERE i = 1 OR nn = 8",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "replace select or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM or_replaced_numbers ORDER BY id",
            .values = or_replaced_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source or predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE not_replaced_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO not_replaced_numbers SELECT id, i FROM numbers WHERE NOT i = 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 3, "replace select not affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM not_replaced_numbers ORDER BY id",
            .values = not_replaced_rows,
            .column_count = 2U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source not predicate",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE between_replaced_numbers (id INT NOT NULL, i INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO between_replaced_numbers SELECT id, i FROM numbers "
        "WHERE i BETWEEN -2 AND 1",
        &result
    );
    failures += expect_int64(
        mylite_result_affected_rows(result),
        3,
        "replace select between affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM between_replaced_numbers ORDER BY id",
            .values = between_replaced_rows,
            .column_count = 2U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source between predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE in_replaced_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO in_replaced_numbers SELECT id, i FROM numbers WHERE i IN (-2, 1, 0)",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 3, "replace select in affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM in_replaced_numbers ORDER BY id",
            .values = between_replaced_rows,
            .column_count = 2U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source in predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE is_replaced_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO is_replaced_numbers SELECT id, i FROM numbers WHERE n IS NOT UNKNOWN",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "replace select is affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM is_replaced_numbers ORDER BY id",
            .values = is_replaced_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source is predicate",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE i = 1 AND nn = 6", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "update and affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update and row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE i = 1 OR nn = 8", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 2, "update or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = or_update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update or row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = NULL WHERE id = 1 OR nn = 6", &result);
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "update or changed affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = or_update_null_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update or changed row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = 99 WHERE id > 1 AND nn >= 6 ORDER BY id DESC LIMIT 2",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "update and limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update and limit row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = 99 WHERE id = 1 OR nn >= 6 ORDER BY id DESC LIMIT 2",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "update or limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = or_update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update or limit row state",
        }
    );

    failures += reset_numbers(database);
    failures +=
        execute_ok(database, "UPDATE numbers SET n = 11 WHERE NOT (i = 1 OR nn = 8)", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 2, "update not affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = not_update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update not row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = NULL WHERE NOT (n IS NULL) ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "update not limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = not_update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update not limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE i BETWEEN -2 AND 1", &result);
    failures +=
        expect_int64(mylite_result_affected_rows(result), 3, "update between affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = between_update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update between row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = NULL WHERE i BETWEEN -2 AND 1 ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "update between limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = between_update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update between limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE i IN (-2, 1, 0)", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 3, "update in affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = between_update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update in row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = NULL WHERE i IN (-2, 1, 0) ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "update in limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = between_update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update in limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE n IS UNKNOWN", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 2, "update is affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = is_update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update is row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = 22 WHERE i IS TRUE ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "update is limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = is_update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update is limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "DELETE FROM numbers WHERE i > 1 AND n IS NULL", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "delete and affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = delete_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete and row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "DELETE FROM numbers WHERE i = 1 OR nn = 8", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 2, "delete or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = or_delete_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete or row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE id > 1 AND nn >= 6 ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "delete and limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = delete_limited_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete and limit row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE id = 1 OR nn >= 6 ORDER BY id DESC LIMIT 2",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "delete or limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = or_delete_limited_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete or limit row state",
        }
    );

    failures += reset_numbers(database);
    failures +=
        execute_ok(database, "DELETE FROM numbers WHERE NOT (n IS NULL OR nn = 6)", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "delete not affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = not_delete_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete not row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE NOT (id = 1 OR nn >= 7) ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "delete not limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = not_delete_limited_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete not limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "DELETE FROM numbers WHERE i BETWEEN -2 AND 1", &result);
    failures +=
        expect_int64(mylite_result_affected_rows(result), 3, "delete between affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = between_delete_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete between row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE i BETWEEN -2 AND 1 ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "delete between limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = between_delete_limited_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete between limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "DELETE FROM numbers WHERE i IN (-2, 1, 0)", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 3, "delete in affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = between_delete_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete in row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE i IN (-2, 1, 0) ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "delete in limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = between_delete_limited_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete in limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE n IS NOT UNKNOWN ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "delete is limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = is_delete_limited_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete is limit row state",
        }
    );

    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 AND missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 OR missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 AND i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 OR i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id XOR nn = 8",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE NOT missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE NOT i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE missing BETWEEN 1 AND 2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i BETWEEN -2 AND 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE iu BETWEEN -1 AND 2",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i BETWEEN NULL AND 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i BETWEEN nn AND 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i BETWEEN '1' AND 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only integer or boolean predicate literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE 1 BETWEEN 1 AND 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers WHERE numbers.i BETWEEN -2 AND 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET n = 1 WHERE numbers.i BETWEEN -2 AND 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE missing IN (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN (1, 2147483648)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE iu IN (-1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id IN ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN (nn)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN ('1')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only integer or boolean predicate literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN (1.0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN (0x1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN (?)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN (SELECT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IN subqueries support one descriptor table source",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i IN ((1, 2))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers WHERE numbers.i IN (-2, 1, 0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET n = 1 WHERE numbers.i IN (-2, 1, 0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE missing IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id IS 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id IS TRUE IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE 1 IS TRUE",
            .values = scalar_all_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar literal is true predicate",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id + 1 IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers WHERE numbers.i IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET n = 1 WHERE numbers.i IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE ! (id = 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE TRUE AND id = 1",
            .values = scalar_true_and_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar truth and predicate",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = nn AND id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "column-to-column predicates are supported only inside EXISTS",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE id = 2 AND nn = 6", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET n = 22 WHERE id = 1 OR id = 4", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = 44 WHERE NOT (id = 1 OR id = 2 OR id = 4)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "UPDATE numbers SET tie = 7 WHERE i BETWEEN -2 AND 1", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET b = 8 WHERE i IN (-2, 1, 0)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET nn = 6 WHERE i IS TRUE", &result);
    mylite_result_free(result);
    result = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "preamble after where and lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen lifecycle database");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 AND nn = 6",
            .values = persisted_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened updated row",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 1 OR id = 4 ORDER BY id",
            .values = or_persisted_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened or updated rows",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE NOT (id = 1 OR id = 2 OR id = 4)",
            .values = not_persisted_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened not updated rows",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie FROM numbers WHERE i BETWEEN -2 AND 1 ORDER BY id",
            .values = between_persisted_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened between updated rows",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT b FROM numbers WHERE i IN (-2, 1, 0) ORDER BY id",
            .values = in_persisted_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened in updated rows",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT nn FROM numbers WHERE i IS TRUE ORDER BY id",
            .values = is_persisted_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened is updated rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_where_xor_predicates(void) {
    static const char *const simple_rows[] = {"2", "1", "4", "0"};
    static const char *const null_right_rows[] = {"4"};
    static const char *const null_truth_rows[] = {"3"};
    static const char *const precedence_rows[] = {"2", "4"};
    static const char *const or_precedence_rows[] = {"1", "2", "4"};
    static const char *const parenthesized_rows[] = {"1", "2", "3", "4"};
    static const char *const in_unknown_rows[] = {"2", "3"};
    static const char *const not_rows[] = {"1", "3"};
    static const char *const repeated_rows[] = {"2", "3"};
    static const char *const distinct_rows[] = {"9"};
    static const char *const count_row[] = {"3"};
    static const char *const grouped_rows[] = {"1", "1", "2", "1"};
    static const char *const copy_rows[] = {"1", NULL, "2", "9", "3", NULL};
    static const char *const inserted_rows[] = {"2", "9", "4", "9"};
    static const char *const update_rows[] = {"1", "11", "2", "11", "3", "11", "4", "9"};
    static const char *const update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        NULL,
        "4",
        "22",
    };
    static const char *const delete_limited_rows[] = {"1", "2", "3"};
    static const char *const persisted_rows[] = {"33", "33", "33"};
    static const char *const scalar_xor_row[] = {"1"};
    static const char *const warning_rows[] = {"1", "4"};
    static const char *const warning_table[] = {
        "Warning",
        "1287",
        "'&&' is deprecated and will be removed in a future release. Please use AND instead",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "xor_lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open xor database");
    failures += seed_database(database);

    failures += execute_ok(
        database,
        "SELECT id, i FROM numbers WHERE i = 1 XOR nn = 8 ORDER BY id",
        &result
    );
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 2U, "xor labels columns");
        failures += expect_size(mylite_result_row_count(result), 2U, "xor labels rows");
        failures += expect_text(mylite_result_column_name(result, 0U), "id", "xor label id");
        failures += expect_text(mylite_result_column_name(result, 1U), "i", "xor label i");
        failures += expect_result_value(result, 0U, 0U, simple_rows[0], "xor row 1 id");
        failures += expect_result_value(result, 0U, 1U, simple_rows[1], "xor row 1 i");
        failures += expect_result_value(result, 1U, 0U, simple_rows[2], "xor row 2 id");
        failures += expect_result_value(result, 1U, 1U, simple_rows[3], "xor row 2 i");
        failures += expect_size(mylite_result_warning_count(result), 0U, "xor warning count");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "xor affected rows");
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 XOR n = 9 ORDER BY id",
            .values = null_right_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor null right operand",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NULL XOR nn = 5 ORDER BY id",
            .values = null_truth_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor null truth operand",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 XOR nn = 8 AND n = 9 ORDER BY id",
            .values = precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "and binds tighter than xor",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 XOR nn = 8 OR id = 1 ORDER BY id",
            .values = or_precedence_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor binds tighter than or",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 AND nn = 6 XOR id = 4 ORDER BY id",
            .values = precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "left conjunction before xor",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE (i = 1 OR nn = 8) XOR n IS NULL ORDER BY id",
            .values = parenthesized_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "parenthesized xor operands",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i IN (-2, 1) XOR n IS UNKNOWN ORDER BY id",
            .values = in_unknown_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor with in and unknown predicates",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NOT i = 1 XOR nn = 8 ORDER BY id",
            .values = not_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "not binds tighter than xor",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = -2 XOR i = 1 XOR n IS NULL ORDER BY id",
            .values = repeated_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "left associative repeated xor",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 XOR id <=> 4 ORDER BY id",
            .values = precedence_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor with null safe comparison",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i BETWEEN 0 AND 1 XOR n IS NULL ORDER BY id",
            .values = parenthesized_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor with between",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE id = 1 XOR nn = 8 && n = 9 ORDER BY id",
            .values = warning_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "xor with deprecated symbolic and",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = warning_table,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor deprecated symbolic and warning",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE i = 1 XOR nn = 8 ORDER BY n",
            .values = distinct_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor distinct source reuse",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE i = 1 XOR n IS NULL",
            .values = count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor count source reuse",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE i = 1 XOR nn = 8 GROUP BY tie "
                   "ORDER BY tie",
            .values = grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor grouped source reuse",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_xor_numbers AS SELECT id, n FROM numbers WHERE n IS NULL XOR i = 1",
        &result
    );
    if (result != NULL) {
        failures += expect_int64(mylite_result_affected_rows(result), 3, "xor ctas affected rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM copy_xor_numbers ORDER BY id",
            .values = copy_rows,
            .column_count = 2U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor create table select source reuse",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE inserted_xor_numbers (id INT NOT NULL, n INT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO inserted_xor_numbers SELECT id, n FROM numbers WHERE i = 1 XOR nn = 8",
        &result
    );
    if (result != NULL) {
        failures +=
            expect_int64(mylite_result_affected_rows(result), 2, "xor insert select affected rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM inserted_xor_numbers ORDER BY id",
            .values = inserted_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor insert select source reuse",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE replaced_xor_numbers (id INT NOT NULL, n INT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO replaced_xor_numbers SELECT id, n FROM numbers WHERE i = 1 XOR nn = 8",
        &result
    );
    if (result != NULL) {
        failures += expect_int64(
            mylite_result_affected_rows(result),
            2,
            "xor replace select affected rows"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM replaced_xor_numbers ORDER BY id",
            .values = inserted_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor replace select source reuse",
        }
    );

    failures += reset_numbers(database);
    failures +=
        execute_ok(database, "UPDATE numbers SET n = 11 WHERE n IS NULL XOR i = 1", &result);
    if (result != NULL) {
        failures +=
            expect_int64(mylite_result_affected_rows(result), 3, "xor update affected rows");
        failures += expect_size(mylite_result_warning_count(result), 0U, "xor update warnings");
        failures += expect_size(mylite_result_row_count(result), 0U, "xor update no rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor update rows",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = 22 WHERE i = 1 XOR nn = 8 ORDER BY id DESC LIMIT 1",
        &result
    );
    if (result != NULL) {
        failures +=
            expect_int64(mylite_result_affected_rows(result), 1, "xor update limit affected rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor ordered limited update rows",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE n IS NOT UNKNOWN XOR id = 1 ORDER BY id DESC LIMIT 1",
        &result
    );
    if (result != NULL) {
        failures +=
            expect_int64(mylite_result_affected_rows(result), 1, "xor delete limit affected rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = delete_limited_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "xor ordered limited delete rows",
        }
    );

    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE missing_left = 1 XOR id = 2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_left' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 XOR missing_right = 2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_right' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 XOR i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers WHERE numbers.id = 1 XOR nn = 8",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET n = 1 WHERE numbers.id = 1 XOR nn = 8",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE i XOR nn = 8",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT 1 XOR 0",
            .values = scalar_xor_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "no-source scalar xor projection",
        }
    );

    failures += reset_numbers(database);
    failures +=
        execute_ok(database, "UPDATE numbers SET nn = 33 WHERE n IS NULL XOR i = 1", &result);
    if (result != NULL) {
        failures += expect_int64(
            mylite_result_affected_rows(result),
            3,
            "xor persisted update affected rows"
        );
    }
    mylite_result_free(result);
    result = NULL;
    mylite_close(database);
    database = NULL;

    if (read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) != 0) {
        fprintf(stderr, "failed to read xor database preamble\n");
        ++failures;
    } else {
        failures += expect_bytes(
            actual_preamble,
            expected_preamble,
            sizeof(expected_preamble),
            "xor preamble"
        );
    }

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen xor database");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT nn FROM numbers WHERE n IS NULL XOR i = 1 ORDER BY id",
            .values = persisted_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened xor updated rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_where_scalar_literal_predicates(void) {
    static const char *const all_ids[] = {"1", "2", "3", "4"};
    static const char *const count_all_row[] = {"4"};
    static const char *const count_empty_row[] = {"0"};
    static const char *const id_two_row[] = {"2"};
    static const char *const positive_i_ids[] = {"2", "3"};
    static const char *const null_n_ids[] = {"1", "3"};
    static const char *const update_rows[] = {"1", NULL, "2", "11", "3", NULL, "4", "9"};
    static const char *const delete_rows[] = {"2", "4"};
    static const char *const copy_rows[] = {"2", "1"};
    static const char *const inserted_rows[] = {"4", "0"};
    static const char *const persisted_count_row[] = {"4"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "scalar_literals") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open scalar literal database");
    failures += seed_database(database);

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE TRUE ORDER BY id",
            .values = all_ids,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE FALSE",
            .values = count_empty_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar false predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE NULL",
            .values = count_empty_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar null truth predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE -1",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "negative scalar truth predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE 9223372036854775807",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "signed max scalar truth predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE -9223372036854775808",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "signed min scalar truth predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE 1 = i",
            .values = id_two_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "literal-left equality predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE 0 < i ORDER BY id",
            .values = positive_i_ids,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "literal-left ordered predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE NULL <=> n ORDER BY id",
            .values = null_n_ids,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "literal-left null-safe predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n = NULL",
            .values = count_empty_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "column equal null predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE 1 = 1",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar literal equality predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE NULL = NULL",
            .values = count_empty_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar null equality predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE NULL <=> NULL",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar null-safe equality predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE 1 IS TRUE",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar is true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE 0 IS FALSE",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar is false predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE NULL IS UNKNOWN",
            .values = count_all_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar is unknown predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE 1 IS NOT TRUE",
            .values = count_empty_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar is not true predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE TRUE AND 1 = i OR FALSE",
            .values = id_two_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar logical predicate",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE 1 = i", &result);
    if (result != NULL) {
        failures += expect_int64(
            mylite_result_affected_rows(result),
            1,
            "scalar literal update affected rows"
        );
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "scalar literal update warnings");
        failures += expect_size(mylite_result_row_count(result), 0U, "scalar literal update rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar literal update result",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "DELETE FROM numbers WHERE NULL <=> n", &result);
    if (result != NULL) {
        failures += expect_int64(
            mylite_result_affected_rows(result),
            2,
            "scalar literal delete affected rows"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = delete_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar literal delete result",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "CREATE TABLE copy_scalar SELECT id, i FROM numbers WHERE 1 = i",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM copy_scalar",
            .values = copy_rows,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar literal create select source",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE inserted_scalar (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO inserted_scalar SELECT id, i FROM numbers WHERE TRUE AND i = 0",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM inserted_scalar",
            .values = inserted_rows,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar literal insert select source",
        }
    );

    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE '1' = i",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE 1.0 = i",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE 2147483648 = id",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'id' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE -1 = iu",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "WHERE scalar literal integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE -9223372036854775809",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "WHERE scalar literal integer literals must fit the signed 64-bit range",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET nn = 33 WHERE NULL IS UNKNOWN", &result);
    if (result != NULL) {
        failures += expect_int64(
            mylite_result_affected_rows(result),
            4,
            "scalar literal persisted update affected rows"
        );
    }
    mylite_result_free(result);
    result = NULL;
    mylite_close(database);
    database = NULL;

    if (read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) != 0) {
        fprintf(stderr, "failed to read scalar literal database preamble\n");
        ++failures;
    } else {
        failures += expect_bytes(
            actual_preamble,
            expected_preamble,
            sizeof(expected_preamble),
            "scalar literal preamble"
        );
    }

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen scalar literal database");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE nn = 33",
            .values = persisted_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened scalar literal update result",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_where_and_handles(void) {
    static const char *const first_values[] = {"31"};
    static const char *const second_values[] = {"41"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first database");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second database");
    failures += seed_database(first);
    failures += seed_database(second);
    failures += execute_ok(first, "UPDATE numbers SET n = 31 WHERE id = 2 OR nn = 100", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "UPDATE numbers SET n = 41 WHERE id = 2 OR nn = 100", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 OR nn = 100",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 OR nn = 100",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle",
        }
    );
    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE NOT id <> 2",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle not predicate",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE NOT id <> 2",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle not predicate",
        }
    );
    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE i BETWEEN 1 AND 1",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle between predicate",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE i BETWEEN 1 AND 1",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle between predicate",
        }
    );
    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id IN (2, 100)",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle in predicate",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id IN (2, 100)",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle in predicate",
        }
    );
    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE TRUE AND 2 = id",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle scalar literal predicate",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE TRUE AND 2 = id",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle scalar literal predicate",
        }
    );
    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id IS TRUE AND id = 2",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle is predicate",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id IS TRUE AND id = 2",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle is predicate",
        }
    );
    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 XOR nn = 100",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle xor predicate",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 XOR nn = 100",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle xor predicate",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int seed_database(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += reset_numbers(database);

    return failures;
}

static int reset_numbers(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = 0;

    (void)execute_ok(database, "DROP TABLE IF EXISTS numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, i INT, iu INT UNSIGNED, b BIGINT, "
        "bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL, tie INT NULL, ia INTEGER)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO numbers VALUES "
        "(1, -2, 0, -9223372036854775808, 0, NULL, 5, 1, -2), "
        "(2, 1, 2, 3, 4, 9, 6, 1, 1), "
        "(3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7, 2, "
        "2147483647), "
        "(4, 0, 8, 8, 8, 9, 8, 2, 0)",
        &result
    );
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got status %d\n", sql, rc);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_result(mylite_db *database, struct expected_result expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_value(result, row, column, expected.values[index], expected.context);
        }
    }
    mylite_result_free(result);

    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL && actual == NULL) {
        return 0;
    }
    if (expected == NULL || actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_runtime_where_and_predicates_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "%s: failed to seek file\n", path);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_size != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, read_size);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing [%s], got [%s]\n",
            context,
            needle,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
