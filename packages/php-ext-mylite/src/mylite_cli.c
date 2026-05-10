#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_query(const char *path, bool memory, const char *sql);
static int print_result(const mylite_result *result);
static void print_usage(const char *program);

int main(int argc, char **argv)
{
    const char *path = NULL;
    const char *sql = NULL;
    bool memory = false;

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", mylite_version());
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--memory") == 0) {
        memory = true;
        sql = argv[2];
    } else if (argc == 3) {
        path = argv[1];
        sql = argv[2];
    } else {
        print_usage(argv[0]);
        return 2;
    }

    return run_query(path, memory, sql);
}

static int run_query(const char *path, bool memory, const char *sql)
{
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int status = MYLITE_OK;
    int exit_code = 0;

    if (memory) {
        status = mylite_open_memory(&database);
    } else {
        status = mylite_open(path, &database);
    }
    if (status != MYLITE_OK) {
        fprintf(stderr, "open failed: status %d\n", status);
        return 1;
    }

    status = mylite_execute(database, sql, strlen(sql), &result);
    if (status != MYLITE_OK) {
        fprintf(stderr, "execute failed: %s\n", mylite_errmsg(database));
        mylite_close(database);
        return 1;
    }

    exit_code = print_result(result);
    mylite_result_free(result);
    mylite_close(database);
    return exit_code;
}

static int print_result(const mylite_result *result)
{
    size_t column_count = mylite_result_column_count(result);
    size_t row_count = mylite_result_row_count(result);

    for (size_t column = 0; column < column_count; column++) {
        if (column > 0) {
            putchar('\t');
        }
        printf("%s", mylite_result_column_name(result, column));
    }
    if (column_count > 0) {
        putchar('\n');
    }

    for (size_t row = 0; row < row_count; row++) {
        for (size_t column = 0; column < column_count; column++) {
            const char *value = mylite_result_value_text(result, row, column);

            if (column > 0) {
                putchar('\t');
            }
            printf("%s", value == NULL ? "NULL" : value);
        }
        putchar('\n');
    }

    return 0;
}

static void print_usage(const char *program)
{
    fprintf(stderr, "usage: %s --version\n", program);
    fprintf(stderr, "usage: %s --memory SQL\n", program);
    fprintf(stderr, "usage: %s FILE.mylite SQL\n", program);
}
