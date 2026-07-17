#ifndef MYLITE_STORAGE_MYLITE_FILE_OPEN_H
#define MYLITE_STORAGE_MYLITE_FILE_OPEN_H

#include <mylite/mylite.h>

#include <stdbool.h>

struct sqlite3;
struct sqlite3_file;

int mylite_storage_vfs_ensure_registered(void);
const char *mylite_storage_vfs_name(void);
void mylite_storage_vfs_set_exclusive_create(bool enabled);
int mylite_storage_vfs_transition_initialization(struct sqlite3_file *file, bool commit);
int mylite_storage_open_sqlite_payload(const char *path, struct sqlite3 **out_sqlite);
int mylite_storage_commit_sqlite_initialization(struct sqlite3 *sqlite);
void mylite_storage_abort_sqlite_initialization(struct sqlite3 *sqlite);
int mylite_storage_configure_sqlite_payload(struct sqlite3 *sqlite);

#endif
