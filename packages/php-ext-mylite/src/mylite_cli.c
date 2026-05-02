#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_query(const char *path, bool memory, const char *sql);
static int print_result(mylite_stmt *stmt);
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
    mylite_stmt *stmt = NULL;
    int status = MYLITE_OK;
    int exit_code = 0;

    if (memory) {
        status = mylite_open_memory(&database);
    } else {
        status = mylite_open(path, &database);
    }
    if (status != MYLITE_OK) {
        fprintf(stderr, "open failed: %s\n", mylite_status_name(status));
        return 1;
    }

    status = mylite_prepare(database, sql, strlen(sql), &stmt);
    if (status != MYLITE_OK) {
        fprintf(stderr, "prepare failed: %s: %s\n", mylite_status_name(status),
                mylite_error_message(database));
        mylite_close(database);
        return 1;
    }

    exit_code = print_result(stmt);
    if (exit_code != 0) {
        fprintf(stderr, "execute failed: %s\n", mylite_error_message(database));
    }

    mylite_finalize(stmt);
    mylite_close(database);
    return exit_code;
}

static int print_result(mylite_stmt *stmt)
{
    int column_count = mylite_column_count(stmt);
    int status = MYLITE_OK;

    for (int column = 0; column < column_count; column++) {
        if (column > 0) {
            putchar('\t');
        }
        printf("%s", mylite_column_name(stmt, column));
    }
    if (column_count > 0) {
        putchar('\n');
    }

    while ((status = mylite_step(stmt)) == MYLITE_ROW) {
        for (int column = 0; column < column_count; column++) {
            const char *value = mylite_column_text(stmt, column);

            if (column > 0) {
                putchar('\t');
            }
            printf("%s", value == NULL ? "NULL" : value);
        }
        putchar('\n');
    }

    return status == MYLITE_DONE ? 0 : 1;
}

static void print_usage(const char *program)
{
    fprintf(stderr, "usage: %s --version\n", program);
    fprintf(stderr, "usage: %s --memory SQL\n", program);
    fprintf(stderr, "usage: %s FILE.mylite SQL\n", program);
}
