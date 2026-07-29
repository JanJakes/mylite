#include "mylite_file_open.h"

#include "sqlite3.h"

#ifdef _WIN32
#  define MYLITE_STORAGE_THREAD_LOCAL __declspec(thread)
#else
#  define MYLITE_STORAGE_THREAD_LOCAL _Thread_local
#endif

static MYLITE_STORAGE_THREAD_LOCAL bool test_truncate_journal;

static int open_sqlite(const char *path, int flags, bool exclusive_create, sqlite3 **out_sqlite);
static int control_sqlite_initialization(sqlite3 *sqlite, bool commit);
static int sqlite_status_to_mylite(int sqlite_status);

int mylite_storage_open_sqlite_payload(const char *path, sqlite3 **out_sqlite) {
    sqlite3 *sqlite = NULL;
    int rc = MYLITE_OK;

    if (path == NULL || path[0] == '\0' || out_sqlite == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sqlite = NULL;

    rc = mylite_storage_vfs_ensure_registered();
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = open_sqlite(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, true, &sqlite);
    if (rc == MYLITE_ERROR) {
        if (sqlite != NULL) {
            (void)sqlite3_close(sqlite);
            sqlite = NULL;
        }
        rc = open_sqlite(path, SQLITE_OPEN_READWRITE, false, &sqlite);
    }
    if (rc != MYLITE_OK) {
        if (sqlite != NULL) {
            (void)sqlite3_close(sqlite);
        }
        return rc;
    }

    *out_sqlite = sqlite;
    return MYLITE_OK;
}

int mylite_storage_commit_sqlite_initialization(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return MYLITE_MISUSE;
    }

    return sqlite_status_to_mylite(control_sqlite_initialization(sqlite, true));
}

void mylite_storage_abort_sqlite_initialization(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return;
    }

    (void)control_sqlite_initialization(sqlite, false);
}

int mylite_storage_configure_sqlite_payload(sqlite3 *sqlite) {
    int rc = SQLITE_OK;

    if (sqlite == NULL) {
        return MYLITE_MISUSE;
    }

    rc = sqlite3_exec(
        sqlite,
        test_truncate_journal ? "PRAGMA journal_mode=TRUNCATE" : "PRAGMA journal_mode=DELETE",
        NULL,
        NULL,
        NULL
    );
    if (rc != SQLITE_OK) {
        return sqlite_status_to_mylite(rc);
    }

    rc = sqlite3_exec(sqlite, "PRAGMA synchronous=EXTRA", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return sqlite_status_to_mylite(rc);
    }

    rc = sqlite3_exec(sqlite, "PRAGMA mmap_size=0", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return sqlite_status_to_mylite(rc);
    }

    return MYLITE_OK;
}

void mylite_storage_test_set_truncate_journal(bool enabled) {
    test_truncate_journal = enabled;
}

static int open_sqlite(const char *path, int flags, bool exclusive_create, sqlite3 **out_sqlite) {
    int sqlite_rc = SQLITE_OK;

    mylite_storage_vfs_set_exclusive_create(exclusive_create);
    sqlite_rc = sqlite3_open_v2(path, out_sqlite, flags, mylite_storage_vfs_name());
    mylite_storage_vfs_set_exclusive_create(false);

    return sqlite_status_to_mylite(sqlite_rc);
}

static int control_sqlite_initialization(sqlite3 *sqlite, bool commit) {
    sqlite3_file *file = NULL;
    int rc = sqlite3_file_control(sqlite, "main", SQLITE_FCNTL_FILE_POINTER, (void *)&file);

    if (rc != SQLITE_OK) {
        return rc;
    }
    return mylite_storage_vfs_transition_initialization(file, commit);
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
