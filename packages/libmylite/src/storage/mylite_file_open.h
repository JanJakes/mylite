#ifndef MYLITE_STORAGE_MYLITE_FILE_OPEN_H
#define MYLITE_STORAGE_MYLITE_FILE_OPEN_H

#include <mylite/mylite.h>

#include <stdbool.h>

struct sqlite3;

struct mylite_storage_open_state {
    bool created_file;
    bool published;
};

int mylite_storage_prepare_mylite_file(const char *path, struct mylite_storage_open_state *state);
void mylite_storage_open_state_mark_published(struct mylite_storage_open_state *state);
void mylite_storage_open_state_deinit(struct mylite_storage_open_state *state, const char *path);

int mylite_storage_vfs_ensure_registered(void);
const char *mylite_storage_vfs_name(void);
int mylite_storage_configure_sqlite_payload(struct sqlite3 *sqlite);

#endif
