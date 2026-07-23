#include "mylite_test_support.h"

int mylite_test_open_temporary(mylite_db **out_database) {
    char path[mylite_test_temp_path_capacity];
    int rc = MYLITE_OK;

    if (out_database == NULL) {
        return MYLITE_MISUSE;
    }

    *out_database = NULL;
    rc = mylite_test_make_path(path, sizeof(path), "runtime");
    if (rc != 0) {
        return MYLITE_ERROR;
    }

    mylite_test_remove_related_files(path);
    rc = mylite_open(path, out_database);
    if (rc != MYLITE_OK) {
        mylite_test_remove_related_files(path);
        return rc;
    }

    rc = mylite_test_register_temporary_path(path);
    if (rc != MYLITE_OK) {
        mylite_close(*out_database);
        *out_database = NULL;
        mylite_test_remove_related_files(path);
        return rc;
    }

    return MYLITE_OK;
}
