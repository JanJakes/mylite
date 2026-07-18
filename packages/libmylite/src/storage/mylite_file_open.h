#ifndef MYLITE_STORAGE_MYLITE_FILE_OPEN_H
#define MYLITE_STORAGE_MYLITE_FILE_OPEN_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>

struct sqlite3;
struct sqlite3_file;

enum mylite_storage_vfs_fault_operation {
    MYLITE_STORAGE_VFS_FAULT_NONE = 0,
    MYLITE_STORAGE_VFS_FAULT_CREATE,
    MYLITE_STORAGE_VFS_FAULT_OPEN,
    MYLITE_STORAGE_VFS_FAULT_WRITE,
    MYLITE_STORAGE_VFS_FAULT_SYNC,
    MYLITE_STORAGE_VFS_FAULT_TRUNCATE,
    MYLITE_STORAGE_VFS_FAULT_DELETE,
    MYLITE_STORAGE_VFS_FAULT_CLOSE,
};

int mylite_storage_vfs_ensure_registered(void);
const char *mylite_storage_vfs_name(void);
void mylite_storage_vfs_set_exclusive_create(bool enabled);
void mylite_storage_vfs_test_set_fault(
    enum mylite_storage_vfs_fault_operation operation,
    size_t fail_on_call
);
void mylite_storage_vfs_test_clear_fault(void);
bool mylite_storage_vfs_test_fault_was_triggered(void);
size_t mylite_storage_vfs_test_matching_call_count(void);
int mylite_storage_vfs_transition_initialization(struct sqlite3_file *file, bool commit);
int mylite_storage_open_sqlite_payload(const char *path, struct sqlite3 **out_sqlite);
int mylite_storage_commit_sqlite_initialization(struct sqlite3 *sqlite);
void mylite_storage_abort_sqlite_initialization(struct sqlite3 *sqlite);
int mylite_storage_configure_sqlite_payload(struct sqlite3 *sqlite);

#endif
