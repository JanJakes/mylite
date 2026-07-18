#include <mylite/mylite.h>

#include <string.h>

int main(void) {
    static const char query[] = "SELECT 1";
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int rc = mylite_open_memory(&database);

    if (rc == MYLITE_OK) {
        rc = mylite_execute(database, query, strlen(query), &result);
    }
    if (rc == MYLITE_OK &&
        (mylite_result_row_count(result) != 1U || mylite_result_column_count(result) != 1U)) {
        rc = MYLITE_ERROR;
    }

    mylite_result_free(result);
    mylite_close(database);
    return rc == MYLITE_OK ? 0 : 1;
}
