#include "mylite_vfs.h"

#include "mylite_file_format.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct mylite_vfs_file {
    sqlite3_file base;
    sqlite3_file *real_file;
    const sqlite3_io_methods *real_methods;
    sqlite3_int64 payload_offset;
};

static int mylite_vfs_open(
    sqlite3_vfs *vfs,
    sqlite3_filename name,
    sqlite3_file *file,
    int flags,
    int *out_flags
);

static int mylite_file_close(sqlite3_file *file);
// SQLite's VFS callback signature fixes the order and types of these parameters.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int mylite_file_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset);
// SQLite's VFS callback signature fixes the order and types of these parameters.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int mylite_file_write(
    sqlite3_file *file,
    const void *buffer,
    int amount,
    sqlite3_int64 offset
);

static int mylite_file_truncate(sqlite3_file *file, sqlite3_int64 size);

static int mylite_file_sync(sqlite3_file *file, int flags);

static int mylite_file_size(sqlite3_file *file, sqlite3_int64 *out_size);

static int mylite_file_lock(sqlite3_file *file, int lock_type);

static int mylite_file_unlock(sqlite3_file *file, int lock_type);

static int mylite_file_check_reserved_lock(sqlite3_file *file, int *out_reserved);

static int mylite_file_control(sqlite3_file *file, int opcode, void *arg);

static int mylite_file_sector_size(sqlite3_file *file);

static int mylite_file_device_characteristics(sqlite3_file *file);

static int mylite_file_shm_map(
    sqlite3_file *file,
    int page,
    int page_size,
    int extend,
    void volatile **out_map
);

static int mylite_file_shm_lock(sqlite3_file *file, int offset, int count, int flags);

static void mylite_file_shm_barrier(sqlite3_file *file);

static int mylite_file_shm_unmap(sqlite3_file *file, int delete_flag);
// SQLite's VFS callback signature fixes the order and types of these parameters.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int mylite_file_fetch(
    sqlite3_file *file,
    sqlite3_int64 offset,
    int amount,
    void **out_mapping
);

static int mylite_file_unfetch(sqlite3_file *file, sqlite3_int64 offset, void *mapping);

static int initialize_main_database_file(struct mylite_vfs_file *file, int flags);

static int translate_offset(
    struct mylite_vfs_file *file,
    sqlite3_int64 offset,
    sqlite3_int64 *out_offset
);

static int translate_file_size(
    struct mylite_vfs_file *file,
    sqlite3_int64 size,
    sqlite3_int64 *out_size
);

static struct mylite_vfs_file *from_sqlite_file(sqlite3_file *file);

static sqlite3_vfs *base_vfs(sqlite3_vfs *vfs);

static int mylite_vfs_delete(sqlite3_vfs *vfs, const char *name, int sync_dir);

static int mylite_vfs_access(sqlite3_vfs *vfs, const char *name, int flags, int *out_result);

static int mylite_vfs_full_pathname(
    sqlite3_vfs *vfs,
    const char *name,
    int output_size,
    char *output
);

static void *mylite_vfs_dl_open(sqlite3_vfs *vfs, const char *filename);

static void mylite_vfs_dl_error(sqlite3_vfs *vfs, int byte_count, char *error_message);
static void (*mylite_vfs_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void);
static void mylite_vfs_dl_close(sqlite3_vfs *vfs, void *handle);

static int mylite_vfs_randomness(sqlite3_vfs *vfs, int byte_count, char *output);

static int mylite_vfs_sleep(sqlite3_vfs *vfs, int microseconds);

static int mylite_vfs_current_time(sqlite3_vfs *vfs, double *out_time);

static int mylite_vfs_get_last_error(sqlite3_vfs *vfs, int byte_count, char *output);

static int mylite_vfs_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *out_time);

static int mylite_vfs_set_system_call(sqlite3_vfs *vfs, const char *name, sqlite3_syscall_ptr call);

static sqlite3_syscall_ptr mylite_vfs_get_system_call(sqlite3_vfs *vfs, const char *name);

static const char *mylite_vfs_next_system_call(sqlite3_vfs *vfs, const char *name);

static const sqlite3_io_methods mylite_io_methods = {
    3,
    mylite_file_close,
    mylite_file_read,
    mylite_file_write,
    mylite_file_truncate,
    mylite_file_sync,
    mylite_file_size,
    mylite_file_lock,
    mylite_file_unlock,
    mylite_file_check_reserved_lock,
    mylite_file_control,
    mylite_file_sector_size,
    mylite_file_device_characteristics,
    mylite_file_shm_map,
    mylite_file_shm_lock,
    mylite_file_shm_barrier,
    mylite_file_shm_unmap,
    mylite_file_fetch,
    mylite_file_unfetch,
};

const char *mylite_vfs_name(void) {
    return "mylite";
}

int mylite_vfs_register(void) {
    static sqlite3_vfs vfs;
    sqlite3_vfs *default_vfs = NULL;

    if (sqlite3_vfs_find(mylite_vfs_name()) != NULL) {
        return SQLITE_OK;
    }

    default_vfs = sqlite3_vfs_find(NULL);
    if (default_vfs == NULL) {
        return SQLITE_ERROR;
    }

    vfs = (sqlite3_vfs){
        .iVersion = default_vfs->iVersion,
        .szOsFile = (int)sizeof(struct mylite_vfs_file),
        .mxPathname = default_vfs->mxPathname,
        .zName = mylite_vfs_name(),
        .pAppData = default_vfs,
        .xOpen = mylite_vfs_open,
        .xDelete = mylite_vfs_delete,
        .xAccess = mylite_vfs_access,
        .xFullPathname = mylite_vfs_full_pathname,
        .xDlOpen = mylite_vfs_dl_open,
        .xDlError = mylite_vfs_dl_error,
        .xDlSym = mylite_vfs_dl_sym,
        .xDlClose = mylite_vfs_dl_close,
        .xRandomness = mylite_vfs_randomness,
        .xSleep = mylite_vfs_sleep,
        .xCurrentTime = mylite_vfs_current_time,
        .xGetLastError = mylite_vfs_get_last_error,
        .xCurrentTimeInt64 = mylite_vfs_current_time_int64,
        .xSetSystemCall = mylite_vfs_set_system_call,
        .xGetSystemCall = mylite_vfs_get_system_call,
        .xNextSystemCall = mylite_vfs_next_system_call,
    };

    if (vfs.iVersion > 3) {
        vfs.iVersion = 3;
    }

    return sqlite3_vfs_register(&vfs, 0);
}

static int mylite_vfs_open(
    sqlite3_vfs *vfs,
    sqlite3_filename name,
    sqlite3_file *file,
    int flags,
    int *out_flags
) {
    sqlite3_vfs *underlying_vfs = base_vfs(vfs);
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    sqlite3_file *real_file = sqlite3_malloc64((sqlite3_uint64)underlying_vfs->szOsFile);
    int rc = SQLITE_OK;

    memset(mylite_file, 0, sizeof(*mylite_file));
    if (real_file == NULL) {
        return SQLITE_NOMEM;
    }
    memset(real_file, 0, (size_t)underlying_vfs->szOsFile);

    rc = underlying_vfs->xOpen(underlying_vfs, name, real_file, flags, out_flags);
    if (rc != SQLITE_OK) {
        sqlite3_free(real_file);
        return rc;
    }

    mylite_file->real_file = real_file;
    mylite_file->real_methods = real_file->pMethods;
    if ((flags & SQLITE_OPEN_MAIN_DB) != 0) {
        rc = initialize_main_database_file(mylite_file, flags);
        if (rc != SQLITE_OK) {
            (void)mylite_file_close(file);
            return rc;
        }
    }

    mylite_file->base.pMethods = &mylite_io_methods;
    return SQLITE_OK;
}

static int mylite_file_close(sqlite3_file *file) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    int rc = SQLITE_OK;

    if (mylite_file->real_file != NULL && mylite_file->real_methods != NULL) {
        rc = mylite_file->real_methods->xClose(mylite_file->real_file);
    }

    sqlite3_free(mylite_file->real_file);
    memset(mylite_file, 0, sizeof(*mylite_file));
    return rc;
}

// SQLite's VFS callback signature fixes the order and types of these parameters.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int mylite_file_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    sqlite3_int64 physical_offset = 0;
    int rc = translate_offset(mylite_file, offset, &physical_offset);

    if (rc != SQLITE_OK) {
        return rc;
    }

    return mylite_file->real_methods
        ->xRead(mylite_file->real_file, buffer, amount, physical_offset);
}

// SQLite's VFS callback signature fixes the order and types of these parameters.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int mylite_file_write(
    sqlite3_file *file,
    const void *buffer,
    int amount,
    sqlite3_int64 offset
) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    sqlite3_int64 physical_offset = 0;
    int rc = translate_offset(mylite_file, offset, &physical_offset);

    if (rc != SQLITE_OK) {
        return rc;
    }

    return mylite_file->real_methods
        ->xWrite(mylite_file->real_file, buffer, amount, physical_offset);
}

static int mylite_file_truncate(sqlite3_file *file, sqlite3_int64 size) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    sqlite3_int64 physical_size = 0;
    int rc = translate_file_size(mylite_file, size, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }

    return mylite_file->real_methods->xTruncate(mylite_file->real_file, physical_size);
}

static int mylite_file_sync(sqlite3_file *file, int flags) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    return mylite_file->real_methods->xSync(mylite_file->real_file, flags);
}

static int mylite_file_size(sqlite3_file *file, sqlite3_int64 *out_size) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    sqlite3_int64 physical_size = 0;
    int rc = mylite_file->real_methods->xFileSize(mylite_file->real_file, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }

    if (physical_size <= mylite_file->payload_offset) {
        *out_size = 0;
    } else {
        *out_size = physical_size - mylite_file->payload_offset;
    }
    return SQLITE_OK;
}

static int mylite_file_lock(sqlite3_file *file, int lock_type) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    return mylite_file->real_methods->xLock(mylite_file->real_file, lock_type);
}

static int mylite_file_unlock(sqlite3_file *file, int lock_type) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    return mylite_file->real_methods->xUnlock(mylite_file->real_file, lock_type);
}

static int mylite_file_check_reserved_lock(sqlite3_file *file, int *out_reserved) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    return mylite_file->real_methods->xCheckReservedLock(mylite_file->real_file, out_reserved);
}

static int mylite_file_control(sqlite3_file *file, int opcode, void *arg) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);

    if (opcode == SQLITE_FCNTL_FILE_POINTER) {
        *((sqlite3_file **)arg) = file;
        return SQLITE_OK;
    }
    if (opcode == SQLITE_FCNTL_SIZE_HINT && mylite_file->payload_offset > 0) {
        sqlite3_int64 physical_size = 0;
        int rc = translate_file_size(mylite_file, *((sqlite3_int64 *)arg), &physical_size);

        if (rc != SQLITE_OK) {
            return rc;
        }
        return mylite_file->real_methods
            ->xFileControl(mylite_file->real_file, opcode, &physical_size);
    }

    return mylite_file->real_methods->xFileControl(mylite_file->real_file, opcode, arg);
}

static int mylite_file_sector_size(sqlite3_file *file) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    return mylite_file->real_methods->xSectorSize(mylite_file->real_file);
}

static int mylite_file_device_characteristics(sqlite3_file *file) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    return mylite_file->real_methods->xDeviceCharacteristics(mylite_file->real_file);
}

static int mylite_file_shm_map(
    sqlite3_file *file,
    int page,
    int page_size,
    int extend,
    void volatile **out_map
) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);

    if (mylite_file->real_methods->iVersion < 2 || mylite_file->real_methods->xShmMap == NULL) {
        return SQLITE_IOERR_SHMMAP;
    }

    return mylite_file->real_methods
        ->xShmMap(mylite_file->real_file, page, page_size, extend, out_map);
}

static int mylite_file_shm_lock(sqlite3_file *file, int offset, int count, int flags) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);

    if (mylite_file->real_methods->iVersion < 2 || mylite_file->real_methods->xShmLock == NULL) {
        return SQLITE_IOERR_SHMLOCK;
    }

    return mylite_file->real_methods->xShmLock(mylite_file->real_file, offset, count, flags);
}

static void mylite_file_shm_barrier(sqlite3_file *file) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);

    if (mylite_file->real_methods->iVersion >= 2 &&
        mylite_file->real_methods->xShmBarrier != NULL) {
        mylite_file->real_methods->xShmBarrier(mylite_file->real_file);
    }
}

static int mylite_file_shm_unmap(sqlite3_file *file, int delete_flag) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);

    if (mylite_file->real_methods->iVersion < 2 || mylite_file->real_methods->xShmUnmap == NULL) {
        return SQLITE_OK;
    }

    return mylite_file->real_methods->xShmUnmap(mylite_file->real_file, delete_flag);
}

// SQLite's VFS callback signature fixes the order and types of these parameters.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int mylite_file_fetch(
    sqlite3_file *file,
    sqlite3_int64 offset,
    int amount,
    void **out_mapping
) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    sqlite3_int64 physical_offset = 0;
    int rc = SQLITE_OK;

    if (mylite_file->real_methods->iVersion < 3 || mylite_file->real_methods->xFetch == NULL) {
        *out_mapping = NULL;
        return SQLITE_OK;
    }

    rc = translate_offset(mylite_file, offset, &physical_offset);
    if (rc != SQLITE_OK) {
        return rc;
    }

    return mylite_file->real_methods
        ->xFetch(mylite_file->real_file, physical_offset, amount, out_mapping);
}

static int mylite_file_unfetch(sqlite3_file *file, sqlite3_int64 offset, void *mapping) {
    struct mylite_vfs_file *mylite_file = from_sqlite_file(file);
    sqlite3_int64 physical_offset = 0;
    int rc = SQLITE_OK;

    if (mylite_file->real_methods->iVersion < 3 || mylite_file->real_methods->xUnfetch == NULL) {
        return SQLITE_OK;
    }

    rc = translate_offset(mylite_file, offset, &physical_offset);
    if (rc != SQLITE_OK) {
        return rc;
    }

    return mylite_file->real_methods->xUnfetch(mylite_file->real_file, physical_offset, mapping);
}

static int initialize_main_database_file(struct mylite_vfs_file *file, int flags) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    sqlite3_int64 physical_size = 0;
    int rc = file->real_methods->xFileSize(file->real_file, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }
    if (physical_size == 0) {
        if ((flags & SQLITE_OPEN_READONLY) != 0) {
            return SQLITE_NOTADB;
        }
        mylite_file_preamble_init(preamble);
        rc = file->real_methods->xWrite(file->real_file, preamble, MYLITE_FILE_PREAMBLE_SIZE, 0);
        if (rc != SQLITE_OK) {
            return rc;
        }
        file->payload_offset = MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;
        return SQLITE_OK;
    }
    if (physical_size < MYLITE_FILE_PREAMBLE_SIZE) {
        return SQLITE_NOTADB;
    }

    rc = file->real_methods->xRead(file->real_file, preamble, MYLITE_FILE_PREAMBLE_SIZE, 0);
    if (rc != SQLITE_OK) {
        return rc;
    }
    if (!mylite_file_preamble_validate(preamble)) {
        return SQLITE_NOTADB;
    }

    file->payload_offset = MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;
    return SQLITE_OK;
}

static int translate_offset(
    struct mylite_vfs_file *file,
    sqlite3_int64 offset,
    sqlite3_int64 *out_offset
) {
    if (offset < 0 || offset > INT64_MAX - file->payload_offset) {
        return SQLITE_IOERR;
    }

    *out_offset = offset + file->payload_offset;
    return SQLITE_OK;
}

static int translate_file_size(
    struct mylite_vfs_file *file,
    sqlite3_int64 size,
    sqlite3_int64 *out_size
) {
    if (file->payload_offset == 0) {
        *out_size = size;
        return SQLITE_OK;
    }
    if (size < 0 || size > INT64_MAX - file->payload_offset) {
        return SQLITE_IOERR;
    }
    if (size == 0) {
        *out_size = MYLITE_FILE_PREAMBLE_SIZE;
    } else {
        *out_size = size + file->payload_offset;
    }
    return SQLITE_OK;
}

static struct mylite_vfs_file *from_sqlite_file(sqlite3_file *file) {
    return (struct mylite_vfs_file *)file;
}

static sqlite3_vfs *base_vfs(sqlite3_vfs *vfs) {
    return (sqlite3_vfs *)vfs->pAppData;
}

static int mylite_vfs_delete(sqlite3_vfs *vfs, const char *name, int sync_dir) {
    return base_vfs(vfs)->xDelete(base_vfs(vfs), name, sync_dir);
}

static int mylite_vfs_access(sqlite3_vfs *vfs, const char *name, int flags, int *out_result) {
    return base_vfs(vfs)->xAccess(base_vfs(vfs), name, flags, out_result);
}

static int mylite_vfs_full_pathname(
    sqlite3_vfs *vfs,
    const char *name,
    int output_size,
    char *output
) {
    return base_vfs(vfs)->xFullPathname(base_vfs(vfs), name, output_size, output);
}

static void *mylite_vfs_dl_open(sqlite3_vfs *vfs, const char *filename) {
    return base_vfs(vfs)->xDlOpen(base_vfs(vfs), filename);
}

static void mylite_vfs_dl_error(sqlite3_vfs *vfs, int byte_count, char *error_message) {
    base_vfs(vfs)->xDlError(base_vfs(vfs), byte_count, error_message);
}

static void (*mylite_vfs_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void) {
    return base_vfs(vfs)->xDlSym(base_vfs(vfs), handle, symbol);
}

static void mylite_vfs_dl_close(sqlite3_vfs *vfs, void *handle) {
    base_vfs(vfs)->xDlClose(base_vfs(vfs), handle);
}

static int mylite_vfs_randomness(sqlite3_vfs *vfs, int byte_count, char *output) {
    return base_vfs(vfs)->xRandomness(base_vfs(vfs), byte_count, output);
}

static int mylite_vfs_sleep(sqlite3_vfs *vfs, int microseconds) {
    return base_vfs(vfs)->xSleep(base_vfs(vfs), microseconds);
}

static int mylite_vfs_current_time(sqlite3_vfs *vfs, double *out_time) {
    return base_vfs(vfs)->xCurrentTime(base_vfs(vfs), out_time);
}

static int mylite_vfs_get_last_error(sqlite3_vfs *vfs, int byte_count, char *output) {
    return base_vfs(vfs)->xGetLastError(base_vfs(vfs), byte_count, output);
}

static int mylite_vfs_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *out_time) {
    if (base_vfs(vfs)->iVersion < 2 || base_vfs(vfs)->xCurrentTimeInt64 == NULL) {
        return SQLITE_ERROR;
    }

    return base_vfs(vfs)->xCurrentTimeInt64(base_vfs(vfs), out_time);
}

static int mylite_vfs_set_system_call(
    sqlite3_vfs *vfs,
    const char *name,
    sqlite3_syscall_ptr call
) {
    if (base_vfs(vfs)->iVersion < 3 || base_vfs(vfs)->xSetSystemCall == NULL) {
        return SQLITE_NOTFOUND;
    }

    return base_vfs(vfs)->xSetSystemCall(base_vfs(vfs), name, call);
}

static sqlite3_syscall_ptr mylite_vfs_get_system_call(sqlite3_vfs *vfs, const char *name) {
    if (base_vfs(vfs)->iVersion < 3 || base_vfs(vfs)->xGetSystemCall == NULL) {
        return NULL;
    }

    return base_vfs(vfs)->xGetSystemCall(base_vfs(vfs), name);
}

static const char *mylite_vfs_next_system_call(sqlite3_vfs *vfs, const char *name) {
    if (base_vfs(vfs)->iVersion < 3 || base_vfs(vfs)->xNextSystemCall == NULL) {
        return NULL;
    }

    return base_vfs(vfs)->xNextSystemCall(base_vfs(vfs), name);
}
