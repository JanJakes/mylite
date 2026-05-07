#include "mylite_file_open.h"

#include "mylite_file_format.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct mylite_offset_file {
    sqlite3_file base;
    sqlite3_file *inner_file;
    bool shifts_offsets;
};

static int offset_close(sqlite3_file *file);
static int offset_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset);
static int offset_write(sqlite3_file *file, const void *buffer, int amount, sqlite3_int64 offset);
static int offset_truncate(sqlite3_file *file, sqlite3_int64 size);
static int offset_sync(sqlite3_file *file, int flags);
static int offset_file_size(sqlite3_file *file, sqlite3_int64 *out_size);
static int offset_lock(sqlite3_file *file, int lock_kind);
static int offset_unlock(sqlite3_file *file, int lock_kind);
static int offset_check_reserved_lock(sqlite3_file *file, int *out_reserved);
static int offset_file_control(sqlite3_file *file, int operation, void *argument);
static int offset_sector_size(sqlite3_file *file);
static int offset_device_characteristics(sqlite3_file *file);
static int offset_vfs_open(
    sqlite3_vfs *vfs,
    sqlite3_filename filename,
    sqlite3_file *file,
    int flags,
    int *out_flags
);
static int offset_vfs_delete(sqlite3_vfs *vfs, const char *filename, int sync_directory);
static int offset_vfs_access(sqlite3_vfs *vfs, const char *filename, int flags, int *out_result);
static int offset_vfs_full_pathname(
    sqlite3_vfs *vfs,
    const char *filename,
    int output_size,
    char *output
);
static void *offset_vfs_dl_open(sqlite3_vfs *vfs, const char *filename);
static void offset_vfs_dl_error(sqlite3_vfs *vfs, int output_size, char *output);
static void (*offset_vfs_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void);
static void offset_vfs_dl_close(sqlite3_vfs *vfs, void *handle);
static int offset_vfs_randomness(sqlite3_vfs *vfs, int output_size, char *output);
static int offset_vfs_sleep(sqlite3_vfs *vfs, int microseconds);
static int offset_vfs_current_time(sqlite3_vfs *vfs, double *out_time);
static int offset_vfs_get_last_error(sqlite3_vfs *vfs, int output_size, char *output);
static sqlite3_vfs *wrapped_vfs(sqlite3_vfs *vfs);
static struct mylite_offset_file *offset_file_from_sqlite_file(sqlite3_file *file);
static int open_inner_file(
    sqlite3_vfs *base_vfs,
    sqlite3_filename filename,
    sqlite3_file *file,
    int flags,
    int *out_flags
);
static void close_failed_inner_file(struct mylite_offset_file *file);
static bool file_shifts_offsets(int flags);
static bool logical_to_physical_offset(sqlite3_int64 logical, sqlite3_int64 *out_physical);
static sqlite3_int64 logical_size_from_physical_size(sqlite3_int64 physical_size);
static int translate_size_file_control(
    struct mylite_offset_file *file,
    int operation,
    void *argument
);
static int sqlite_status_to_mylite(int sqlite_status);

static const sqlite3_io_methods offset_io_methods = {
    .iVersion = 1,
    .xClose = offset_close,
    .xRead = offset_read,
    .xWrite = offset_write,
    .xTruncate = offset_truncate,
    .xSync = offset_sync,
    .xFileSize = offset_file_size,
    .xLock = offset_lock,
    .xUnlock = offset_unlock,
    .xCheckReservedLock = offset_check_reserved_lock,
    .xFileControl = offset_file_control,
    .xSectorSize = offset_sector_size,
    .xDeviceCharacteristics = offset_device_characteristics,
    .xShmMap = NULL,
    .xShmLock = NULL,
    .xShmBarrier = NULL,
    .xShmUnmap = NULL,
    .xFetch = NULL,
    .xUnfetch = NULL,
};

static sqlite3_vfs mylite_offset_vfs;

int mylite_storage_vfs_ensure_registered(void) {
    sqlite3_vfs *base_vfs = NULL;
    int rc = SQLITE_OK;

    if (sqlite3_vfs_find(mylite_storage_vfs_name()) != NULL) {
        return MYLITE_OK;
    }

    base_vfs = sqlite3_vfs_find(NULL);
    if (base_vfs == NULL || base_vfs->szOsFile <= 0) {
        return MYLITE_ERROR;
    }

    memset(&mylite_offset_vfs, 0, sizeof(mylite_offset_vfs));
    mylite_offset_vfs.iVersion = 1;
    mylite_offset_vfs.szOsFile = (int)sizeof(struct mylite_offset_file);
    mylite_offset_vfs.mxPathname = base_vfs->mxPathname;
    mylite_offset_vfs.zName = mylite_storage_vfs_name();
    mylite_offset_vfs.pAppData = base_vfs;
    mylite_offset_vfs.xOpen = offset_vfs_open;
    mylite_offset_vfs.xDelete = offset_vfs_delete;
    mylite_offset_vfs.xAccess = offset_vfs_access;
    mylite_offset_vfs.xFullPathname = offset_vfs_full_pathname;
    mylite_offset_vfs.xDlOpen = offset_vfs_dl_open;
    mylite_offset_vfs.xDlError = offset_vfs_dl_error;
    mylite_offset_vfs.xDlSym = offset_vfs_dl_sym;
    mylite_offset_vfs.xDlClose = offset_vfs_dl_close;
    mylite_offset_vfs.xRandomness = offset_vfs_randomness;
    mylite_offset_vfs.xSleep = offset_vfs_sleep;
    mylite_offset_vfs.xCurrentTime = offset_vfs_current_time;
    mylite_offset_vfs.xGetLastError = offset_vfs_get_last_error;

    rc = sqlite3_vfs_register(&mylite_offset_vfs, 0);

    return sqlite_status_to_mylite(rc);
}

const char *mylite_storage_vfs_name(void) {
    return "mylite-offset-vfs";
}

static int offset_close(sqlite3_file *file) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    int rc = SQLITE_OK;

    if (offset_file->inner_file != NULL) {
        if (offset_file->inner_file->pMethods != NULL) {
            rc = offset_file->inner_file->pMethods->xClose(offset_file->inner_file);
        }
        sqlite3_free(offset_file->inner_file);
    }

    memset(offset_file, 0, sizeof(*offset_file));

    return rc;
}

static int offset_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    sqlite3_int64 physical_offset = offset;

    if (offset_file->shifts_offsets && !logical_to_physical_offset(offset, &physical_offset)) {
        return SQLITE_IOERR_READ;
    }

    return offset_file->inner_file->pMethods
        ->xRead(offset_file->inner_file, buffer, amount, physical_offset);
}

static int offset_write(sqlite3_file *file, const void *buffer, int amount, sqlite3_int64 offset) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    sqlite3_int64 physical_offset = offset;

    if (offset_file->shifts_offsets && !logical_to_physical_offset(offset, &physical_offset)) {
        return SQLITE_IOERR_WRITE;
    }

    return offset_file->inner_file->pMethods
        ->xWrite(offset_file->inner_file, buffer, amount, physical_offset);
}

static int offset_truncate(sqlite3_file *file, sqlite3_int64 size) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    sqlite3_int64 physical_size = size;

    if (offset_file->shifts_offsets && !logical_to_physical_offset(size, &physical_size)) {
        return SQLITE_IOERR_TRUNCATE;
    }

    return offset_file->inner_file->pMethods->xTruncate(offset_file->inner_file, physical_size);
}

static int offset_sync(sqlite3_file *file, int flags) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    return offset_file->inner_file->pMethods->xSync(offset_file->inner_file, flags);
}

static int offset_file_size(sqlite3_file *file, sqlite3_int64 *out_size) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    sqlite3_int64 physical_size = 0;
    int rc = offset_file->inner_file->pMethods->xFileSize(offset_file->inner_file, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }

    *out_size = offset_file->shifts_offsets ? logical_size_from_physical_size(physical_size)
                                            : physical_size;

    return SQLITE_OK;
}

static int offset_lock(sqlite3_file *file, int lock_kind) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    return offset_file->inner_file->pMethods->xLock(offset_file->inner_file, lock_kind);
}

static int offset_unlock(sqlite3_file *file, int lock_kind) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    return offset_file->inner_file->pMethods->xUnlock(offset_file->inner_file, lock_kind);
}

static int offset_check_reserved_lock(sqlite3_file *file, int *out_reserved) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    return offset_file->inner_file->pMethods->xCheckReservedLock(
        offset_file->inner_file,
        out_reserved
    );
}

static int offset_file_control(sqlite3_file *file, int operation, void *argument) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (offset_file->shifts_offsets &&
        (operation == SQLITE_FCNTL_SIZE_HINT || operation == SQLITE_FCNTL_SIZE_LIMIT)) {
        return translate_size_file_control(offset_file, operation, argument);
    }

    return offset_file->inner_file->pMethods
        ->xFileControl(offset_file->inner_file, operation, argument);
}

static int offset_sector_size(sqlite3_file *file) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    return offset_file->inner_file->pMethods->xSectorSize(offset_file->inner_file);
}

static int offset_device_characteristics(sqlite3_file *file) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    return offset_file->inner_file->pMethods->xDeviceCharacteristics(offset_file->inner_file);
}

static int offset_vfs_open(
    sqlite3_vfs *vfs,
    sqlite3_filename filename,
    sqlite3_file *file,
    int flags,
    int *out_flags
) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    int rc = SQLITE_OK;

    memset(offset_file, 0, sizeof(*offset_file));

    rc = open_inner_file(base_vfs, filename, file, flags, out_flags);
    if (rc != SQLITE_OK) {
        close_failed_inner_file(offset_file);
        return rc;
    }

    offset_file->shifts_offsets = file_shifts_offsets(flags);
    offset_file->base.pMethods = &offset_io_methods;

    return SQLITE_OK;
}

static int offset_vfs_delete(sqlite3_vfs *vfs, const char *filename, int sync_directory) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xDelete(base_vfs, filename, sync_directory);
}

static int offset_vfs_access(sqlite3_vfs *vfs, const char *filename, int flags, int *out_result) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xAccess(base_vfs, filename, flags, out_result);
}

static int offset_vfs_full_pathname(
    sqlite3_vfs *vfs,
    const char *filename,
    int output_size,
    char *output
) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xFullPathname(base_vfs, filename, output_size, output);
}

static void *offset_vfs_dl_open(sqlite3_vfs *vfs, const char *filename) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xDlOpen(base_vfs, filename);
}

static void offset_vfs_dl_error(sqlite3_vfs *vfs, int output_size, char *output) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    base_vfs->xDlError(base_vfs, output_size, output);
}

static void (*offset_vfs_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xDlSym(base_vfs, handle, symbol);
}

static void offset_vfs_dl_close(sqlite3_vfs *vfs, void *handle) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    base_vfs->xDlClose(base_vfs, handle);
}

static int offset_vfs_randomness(sqlite3_vfs *vfs, int output_size, char *output) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xRandomness(base_vfs, output_size, output);
}

static int offset_vfs_sleep(sqlite3_vfs *vfs, int microseconds) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xSleep(base_vfs, microseconds);
}

static int offset_vfs_current_time(sqlite3_vfs *vfs, double *out_time) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xCurrentTime(base_vfs, out_time);
}

static int offset_vfs_get_last_error(sqlite3_vfs *vfs, int output_size, char *output) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    return base_vfs->xGetLastError(base_vfs, output_size, output);
}

static sqlite3_vfs *wrapped_vfs(sqlite3_vfs *vfs) {
    return vfs->pAppData;
}

static struct mylite_offset_file *offset_file_from_sqlite_file(sqlite3_file *file) {
    return (struct mylite_offset_file *)file;
}

static int open_inner_file(
    sqlite3_vfs *base_vfs,
    sqlite3_filename filename,
    sqlite3_file *file,
    int flags,
    int *out_flags
) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    offset_file->inner_file = sqlite3_malloc64((sqlite3_uint64)base_vfs->szOsFile);
    if (offset_file->inner_file == NULL) {
        return SQLITE_NOMEM;
    }
    memset(offset_file->inner_file, 0, (size_t)base_vfs->szOsFile);

    return base_vfs->xOpen(base_vfs, filename, offset_file->inner_file, flags, out_flags);
}

static void close_failed_inner_file(struct mylite_offset_file *file) {
    if (file == NULL || file->inner_file == NULL) {
        return;
    }

    if (file->inner_file->pMethods != NULL) {
        (void)file->inner_file->pMethods->xClose(file->inner_file);
    }
    sqlite3_free(file->inner_file);
    file->inner_file = NULL;
    file->base.pMethods = NULL;
}

static bool file_shifts_offsets(int flags) {
    return (flags & SQLITE_OPEN_MAIN_DB) != 0;
}

static bool logical_to_physical_offset(sqlite3_int64 logical, sqlite3_int64 *out_physical) {
    const sqlite3_int64 payload_offset = MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;

    if (logical < 0 || logical > INT64_MAX - payload_offset) {
        return false;
    }

    *out_physical = logical + payload_offset;

    return true;
}

static sqlite3_int64 logical_size_from_physical_size(sqlite3_int64 physical_size) {
    const sqlite3_int64 payload_offset = MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;

    if (physical_size <= payload_offset) {
        return 0;
    }

    return physical_size - payload_offset;
}

static int translate_size_file_control(
    struct mylite_offset_file *file,
    int operation,
    void *argument
) {
    sqlite3_int64 *size = argument;
    sqlite3_int64 original_size = 0;
    sqlite3_int64 physical_size = 0;
    int rc = SQLITE_OK;

    if (size == NULL) {
        return file->inner_file->pMethods->xFileControl(file->inner_file, operation, argument);
    }

    original_size = *size;
    if (original_size >= 0 && !logical_to_physical_offset(original_size, &physical_size)) {
        return SQLITE_IOERR;
    }
    if (original_size >= 0) {
        *size = physical_size;
    }

    rc = file->inner_file->pMethods->xFileControl(file->inner_file, operation, argument);
    if (rc != SQLITE_OK) {
        *size = original_size;
        return rc;
    }

    *size = logical_size_from_physical_size(*size);

    return SQLITE_OK;
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
