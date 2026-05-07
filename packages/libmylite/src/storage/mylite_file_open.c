#include "mylite_file_open.h"

#include "mylite_file_format.h"
#include "sqlite3.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int validate_or_create_preamble(const char *path, struct mylite_storage_open_state *state);
static int validate_existing_preamble(FILE *file);
static int create_file_with_preamble(const char *path, struct mylite_storage_open_state *state);
static int write_preamble(FILE *file);
static int sqlite_status_to_mylite(int sqlite_status);

int mylite_storage_prepare_mylite_file(const char *path, struct mylite_storage_open_state *state) {
    if (path == NULL || path[0] == '\0' || state == NULL) {
        return MYLITE_MISUSE;
    }

    memset(state, 0, sizeof(*state));

    return validate_or_create_preamble(path, state);
}

void mylite_storage_open_state_mark_published(struct mylite_storage_open_state *state) {
    if (state == NULL) {
        return;
    }

    state->published = true;
}

void mylite_storage_open_state_deinit(struct mylite_storage_open_state *state, const char *path) {
    if (state == NULL) {
        return;
    }

    if (state->created_file && !state->published && path != NULL && path[0] != '\0') {
        (void)remove(path);
    }

    memset(state, 0, sizeof(*state));
}

int mylite_storage_configure_sqlite_payload(sqlite3 *sqlite) {
    int rc = SQLITE_OK;

    if (sqlite == NULL) {
        return MYLITE_MISUSE;
    }

    rc = sqlite3_exec(sqlite, "PRAGMA journal_mode=DELETE", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return sqlite_status_to_mylite(rc);
    }

    rc = sqlite3_exec(sqlite, "PRAGMA mmap_size=0", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return sqlite_status_to_mylite(rc);
    }

    return MYLITE_OK;
}

static int validate_or_create_preamble(const char *path, struct mylite_storage_open_state *state) {
    FILE *file = fopen(path, "rb");
    int rc = MYLITE_OK;

    if (file == NULL) {
        if (errno != ENOENT) {
            return MYLITE_ERROR;
        }

        return create_file_with_preamble(path, state);
    }

    rc = validate_existing_preamble(file);
    if (fclose(file) != 0 && rc == MYLITE_OK) {
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_existing_preamble(FILE *file) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    size_t read_count = 0U;

    read_count = fread(preamble, 1U, sizeof(preamble), file);
    if (read_count != sizeof(preamble)) {
        return MYLITE_ERROR;
    }
    if (!mylite_file_preamble_validate(preamble)) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int create_file_with_preamble(const char *path, struct mylite_storage_open_state *state) {
    FILE *file = fopen(path, "wbx");
    int rc = MYLITE_OK;

    if (file == NULL) {
        return MYLITE_ERROR;
    }

    rc = write_preamble(file);
    if (fclose(file) != 0 && rc == MYLITE_OK) {
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        (void)remove(path);
        return rc;
    }

    state->created_file = true;

    return MYLITE_OK;
}

static int write_preamble(FILE *file) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];

    mylite_file_preamble_init(preamble);

    if (fwrite(preamble, 1U, sizeof(preamble), file) != sizeof(preamble)) {
        return MYLITE_ERROR;
    }
    if (fflush(file) != 0) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int sqlite_status_to_mylite(int sqlite_status) {
    if (sqlite_status == SQLITE_OK) {
        return MYLITE_OK;
    }
    if (sqlite_status == SQLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    if (sqlite_status == SQLITE_MISUSE) {
        return MYLITE_MISUSE;
    }

    return MYLITE_ERROR;
}
