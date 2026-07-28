#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *version = mylite_version();
    const char *server_version = mylite_server_version();
    mylite_db *database = NULL;
    mylite_result *result = NULL;

    if (strcmp(version, MYLITE_VERSION_STRING) != 0) {
        fprintf(stderr, "expected %s, got %s\n", MYLITE_VERSION_STRING, version);
        return 1;
    }
    if (strcmp(server_version, "8.4.9") != 0) {
        fprintf(stderr, "expected server version 8.4.9, got %s\n", server_version);
        return 1;
    }
    if (mylite_open_memory(&database) != MYLITE_OK ||
        mylite_execute(
            database,
            "SELECT VERSION()",
            strlen("SELECT VERSION()"),
            &result
        ) != MYLITE_OK ||
        mylite_result_row_count(result) != 1U ||
        strcmp(mylite_result_value_text(result, 0U, 0U), server_version) != 0) {
        fprintf(stderr, "SQL-visible version does not match %s\n", server_version);
        mylite_result_free(result);
        if (database != NULL) {
            mylite_close(database);
        }
        return 1;
    }

    mylite_result_free(result);
    mylite_close(database);
    return 0;
}
