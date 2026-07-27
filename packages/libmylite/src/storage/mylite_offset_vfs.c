#include "mylite_file_open.h"

#include "mylite_file_format.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct mylite_offset_file {
    sqlite3_file base;
    sqlite3_file *inner_file;
    uint16_t format_version;
    bool shifts_offsets;
    bool initialization_owner;
    bool initialization_lock_held;
    bool initialization_preamble_written;
};

struct mylite_storage_vfs_fault {
    enum mylite_storage_vfs_fault_operation operation;
    size_t calls_until_failure;
    size_t matching_call_count;
    bool triggered;
};

#ifdef _WIN32
#  define MYLITE_STORAGE_THREAD_LOCAL __declspec(thread)
#else
#  define MYLITE_STORAGE_THREAD_LOCAL _Thread_local
#endif

static int offset_close(sqlite3_file *file);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): SQLite fixes this VFS ABI.
static int offset_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): SQLite fixes this VFS ABI.
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
static int ensure_vfs_registered_locked(void);
static int register_offset_vfs(sqlite3_vfs *base_vfs);
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
static int prepare_main_database_file(struct mylite_offset_file *file, bool exclusive_create);
static int initialize_new_main_database_file(struct mylite_offset_file *file);
static int validate_existing_main_database_file(struct mylite_offset_file *file);
static int acquire_initialization_lock(struct mylite_offset_file *file);
static int release_initialization_lock(struct mylite_offset_file *file);
static int validate_sector_alignment(struct mylite_offset_file *file);
static int validate_sqlite_payload_header(struct mylite_offset_file *file);
static int transition_initialization_state(
    struct mylite_offset_file *file,
    enum mylite_file_lifecycle_state state
);
static int read_shifted(
    struct mylite_offset_file *file,
    void *buffer,
    int amount,
    sqlite3_int64 logical_offset
);
static int write_shifted(
    struct mylite_offset_file *file,
    const void *buffer,
    int amount,
    sqlite3_int64 logical_offset
);
static bool logical_range_end(sqlite3_int64 offset, int amount, sqlite3_int64 *out_end);
static bool logical_offset_to_physical(
    const struct mylite_offset_file *file,
    sqlite3_int64 logical,
    sqlite3_int64 *out_physical
);
static bool logical_size_to_physical(
    const struct mylite_offset_file *file,
    sqlite3_int64 logical,
    sqlite3_int64 *out_physical
);
static bool logical_size_from_physical(
    const struct mylite_offset_file *file,
    sqlite3_int64 physical,
    sqlite3_int64 *out_logical
);
static bool format_uses_lock_gap(const struct mylite_offset_file *file);
static int atomic_write_capability_mask(void);
static int translate_size_file_control(
    struct mylite_offset_file *file,
    int operation,
    void *argument
);
static int sqlite_status_to_mylite(int sqlite_status);
static bool inject_fault(enum mylite_storage_vfs_fault_operation operation);

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
static MYLITE_STORAGE_THREAD_LOCAL bool exclusive_create_enabled;
static MYLITE_STORAGE_THREAD_LOCAL struct mylite_storage_vfs_fault injected_fault;

int mylite_storage_vfs_ensure_registered(void) {
    sqlite3_mutex *mutex = NULL;
    int rc = SQLITE_OK;

    rc = sqlite3_initialize();
    if (rc != SQLITE_OK) {
        return sqlite_status_to_mylite(rc);
    }

    mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_VFS3);
    sqlite3_mutex_enter(mutex);
    rc = ensure_vfs_registered_locked();
    sqlite3_mutex_leave(mutex);

    return rc;
}

const char *mylite_storage_vfs_name(void) {
    return "mylite-offset-vfs";
}

void mylite_storage_vfs_set_exclusive_create(bool enabled) {
    exclusive_create_enabled = enabled;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): the enum identifies the failpoint.
void mylite_storage_vfs_test_set_fault(
    enum mylite_storage_vfs_fault_operation operation,
    size_t fail_on_call
) {
    injected_fault.operation = operation;
    injected_fault.calls_until_failure = fail_on_call == 0U ? 1U : fail_on_call;
    injected_fault.matching_call_count = 0U;
    injected_fault.triggered = false;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

void mylite_storage_vfs_test_clear_fault(void) {
    memset(&injected_fault, 0, sizeof(injected_fault));
}

bool mylite_storage_vfs_test_fault_was_triggered(void) {
    return injected_fault.triggered;
}

size_t mylite_storage_vfs_test_matching_call_count(void) {
    return injected_fault.matching_call_count;
}

int mylite_storage_vfs_transition_initialization(sqlite3_file *file, bool commit) {
    if (file == NULL || file->pMethods != &offset_io_methods) {
        return SQLITE_NOTFOUND;
    }

    return transition_initialization_state(
        offset_file_from_sqlite_file(file),
        commit ? MYLITE_FILE_LIFECYCLE_COMMITTED : MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED
    );
}

static int ensure_vfs_registered_locked(void) {
    sqlite3_vfs *registered_vfs = sqlite3_vfs_find(mylite_storage_vfs_name());
    sqlite3_vfs *base_vfs = NULL;

    if (registered_vfs != NULL) {
        return registered_vfs == &mylite_offset_vfs ? MYLITE_OK : MYLITE_ERROR;
    }

    base_vfs = sqlite3_vfs_find(NULL);
    if (base_vfs == NULL || base_vfs->szOsFile <= 0) {
        return MYLITE_ERROR;
    }

    return register_offset_vfs(base_vfs);
}

static int register_offset_vfs(sqlite3_vfs *base_vfs) {
    int rc = SQLITE_OK;

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

static int offset_close(sqlite3_file *file) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    bool fail_close = inject_fault(MYLITE_STORAGE_VFS_FAULT_CLOSE);
    int rc = SQLITE_OK;

    if (offset_file->inner_file != NULL) {
        if (offset_file->initialization_owner) {
            (void
            )transition_initialization_state(offset_file, MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED);
        }
        if (offset_file->inner_file->pMethods != NULL) {
            rc = offset_file->inner_file->pMethods->xClose(offset_file->inner_file);
        }
        sqlite3_free(offset_file->inner_file);
    }

    memset(offset_file, 0, sizeof(*offset_file));

    return fail_close && rc == SQLITE_OK ? SQLITE_IOERR_CLOSE : rc;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): SQLite fixes this VFS ABI.
static int offset_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (offset_file->shifts_offsets) {
        return read_shifted(offset_file, buffer, amount, offset);
    }

    return offset_file->inner_file->pMethods
        ->xRead(offset_file->inner_file, buffer, amount, offset);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): SQLite fixes this VFS ABI.
static int offset_write(sqlite3_file *file, const void *buffer, int amount, sqlite3_int64 offset) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_WRITE)) {
        return SQLITE_IOERR_WRITE;
    }

    if (offset_file->shifts_offsets) {
        return write_shifted(offset_file, buffer, amount, offset);
    }

    return offset_file->inner_file->pMethods
        ->xWrite(offset_file->inner_file, buffer, amount, offset);
}

static int offset_truncate(sqlite3_file *file, sqlite3_int64 size) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    sqlite3_int64 physical_size = size;

    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_TRUNCATE)) {
        return SQLITE_IOERR_TRUNCATE;
    }

    if (offset_file->shifts_offsets) {
        if (!format_uses_lock_gap(offset_file) && size > MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE) {
            return SQLITE_FULL;
        }
        if (!logical_size_to_physical(offset_file, size, &physical_size)) {
            return SQLITE_IOERR_TRUNCATE;
        }
    }

    return offset_file->inner_file->pMethods->xTruncate(offset_file->inner_file, physical_size);
}

static int offset_sync(sqlite3_file *file, int flags) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_SYNC)) {
        return SQLITE_IOERR_FSYNC;
    }

    return offset_file->inner_file->pMethods->xSync(offset_file->inner_file, flags);
}

static int offset_file_size(sqlite3_file *file, sqlite3_int64 *out_size) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    sqlite3_int64 physical_size = 0;
    int rc = offset_file->inner_file->pMethods->xFileSize(offset_file->inner_file, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }

    if (offset_file->shifts_offsets) {
        if (!logical_size_from_physical(offset_file, physical_size, out_size)) {
            return SQLITE_IOERR_FSTAT;
        }
    } else {
        *out_size = physical_size;
    }

    return SQLITE_OK;
}

static int offset_lock(sqlite3_file *file, int lock_kind) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (offset_file->initialization_lock_held) {
        return SQLITE_OK;
    }

    return offset_file->inner_file->pMethods->xLock(offset_file->inner_file, lock_kind);
}

static int offset_unlock(sqlite3_file *file, int lock_kind) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (offset_file->initialization_lock_held) {
        return SQLITE_OK;
    }

    return offset_file->inner_file->pMethods->xUnlock(offset_file->inner_file, lock_kind);
}

static int offset_check_reserved_lock(sqlite3_file *file, int *out_reserved) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (offset_file->initialization_lock_held) {
        *out_reserved = 0;
        return SQLITE_OK;
    }

    return offset_file->inner_file->pMethods->xCheckReservedLock(
        offset_file->inner_file,
        out_reserved
    );
}

static int offset_file_control(sqlite3_file *file, int operation, void *argument) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (offset_file->shifts_offsets && operation == SQLITE_FCNTL_CHUNK_SIZE) {
        int disabled_chunk_size = 0;

        return offset_file->inner_file->pMethods
            ->xFileControl(offset_file->inner_file, operation, &disabled_chunk_size);
    }
    if (offset_file->shifts_offsets && operation == SQLITE_FCNTL_MMAP_SIZE) {
        sqlite3_int64 disabled_mmap_size = 0;
        int rc = offset_file->inner_file->pMethods
                     ->xFileControl(offset_file->inner_file, operation, &disabled_mmap_size);

        if (argument != NULL) {
            *(sqlite3_int64 *)argument = 0;
        }
        return rc == SQLITE_NOTFOUND ? SQLITE_OK : rc;
    }
    if (offset_file->shifts_offsets && (operation == SQLITE_FCNTL_BEGIN_ATOMIC_WRITE ||
                                        operation == SQLITE_FCNTL_COMMIT_ATOMIC_WRITE ||
                                        operation == SQLITE_FCNTL_ROLLBACK_ATOMIC_WRITE)) {
        return SQLITE_NOTFOUND;
    }
    if (offset_file->shifts_offsets &&
        (operation == SQLITE_FCNTL_SIZE_HINT || operation == SQLITE_FCNTL_SIZE_LIMIT)) {
        return translate_size_file_control(offset_file, operation, argument);
    }

    return offset_file->inner_file->pMethods
        ->xFileControl(offset_file->inner_file, operation, argument);
}

static int offset_sector_size(sqlite3_file *file) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);

    if (offset_file->inner_file->pMethods->xSectorSize == NULL) {
        return MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;
    }

    return offset_file->inner_file->pMethods->xSectorSize(offset_file->inner_file);
}

static int offset_device_characteristics(sqlite3_file *file) {
    struct mylite_offset_file *offset_file = offset_file_from_sqlite_file(file);
    int characteristics =
        offset_file->inner_file->pMethods->xDeviceCharacteristics(offset_file->inner_file);

    if (offset_file->shifts_offsets) {
        characteristics &= ~atomic_write_capability_mask();
    }

    return characteristics;
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
    bool exclusive_create = exclusive_create_enabled && file_shifts_offsets(flags);
    int inner_flags = exclusive_create ? flags | SQLITE_OPEN_EXCLUSIVE : flags;
    int rc = SQLITE_OK;

    memset(offset_file, 0, sizeof(*offset_file));

    if (file_shifts_offsets(flags) &&
        inject_fault(
            exclusive_create ? MYLITE_STORAGE_VFS_FAULT_CREATE : MYLITE_STORAGE_VFS_FAULT_OPEN
        )) {
        return SQLITE_CANTOPEN;
    }

    rc = open_inner_file(base_vfs, filename, file, inner_flags, out_flags);
    if (rc != SQLITE_OK) {
        close_failed_inner_file(offset_file);
        return rc;
    }
    if (exclusive_create && out_flags != NULL && (*out_flags & SQLITE_OPEN_READONLY) != 0) {
        close_failed_inner_file(offset_file);
        return SQLITE_CANTOPEN;
    }

    offset_file->shifts_offsets = file_shifts_offsets(flags);
    if (offset_file->shifts_offsets) {
        rc = prepare_main_database_file(offset_file, exclusive_create);
        if (rc == SQLITE_OK) {
            rc = validate_sector_alignment(offset_file);
        }
        if (rc != SQLITE_OK) {
            close_failed_inner_file(offset_file);
            return rc;
        }
    }
    offset_file->base.pMethods = &offset_io_methods;

    return SQLITE_OK;
}

static int offset_vfs_delete(sqlite3_vfs *vfs, const char *filename, int sync_directory) {
    sqlite3_vfs *base_vfs = wrapped_vfs(vfs);

    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_DELETE)) {
        return SQLITE_IOERR_DELETE;
    }

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

    if (file->initialization_owner && file->inner_file->pMethods != NULL) {
        (void)transition_initialization_state(file, MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED);
    }
    if (file->inner_file->pMethods != NULL) {
        (void)file->inner_file->pMethods->xClose(file->inner_file);
    }
    sqlite3_free(file->inner_file);
    file->inner_file = NULL;
    file->initialization_owner = false;
    file->base.pMethods = NULL;
}

static bool file_shifts_offsets(int flags) {
    return (flags & SQLITE_OPEN_MAIN_DB) != 0;
}

static int prepare_main_database_file(struct mylite_offset_file *file, bool exclusive_create) {
    if (exclusive_create) {
        return initialize_new_main_database_file(file);
    }

    return validate_existing_main_database_file(file);
}

static int initialize_new_main_database_file(struct mylite_offset_file *file) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    sqlite3_int64 physical_size = 0;
    int rc = file->inner_file->pMethods->xFileSize(file->inner_file, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }
    if (physical_size != 0) {
        return SQLITE_CANTOPEN;
    }

    file->format_version = MYLITE_FILE_FORMAT_VERSION;
    rc = acquire_initialization_lock(file);
    if (rc != SQLITE_OK) {
        return rc;
    }
    mylite_file_preamble_init_with_state(preamble, MYLITE_FILE_LIFECYCLE_INITIALIZING);
    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_WRITE)) {
        return SQLITE_IOERR_WRITE;
    }
    rc = file->inner_file->pMethods
             ->xWrite(file->inner_file, preamble, MYLITE_FILE_PREAMBLE_SIZE, 0);
    if (rc != SQLITE_OK) {
        return rc;
    }
    file->initialization_preamble_written = true;

    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_SYNC)) {
        return SQLITE_IOERR_FSYNC;
    }
    return file->inner_file->pMethods->xSync(file->inner_file, SQLITE_SYNC_FULL);
}

static int validate_existing_main_database_file(struct mylite_offset_file *file) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    enum mylite_file_lifecycle_state lifecycle_state = MYLITE_FILE_LIFECYCLE_INVALID;
    sqlite3_int64 physical_size = 0;
    int rc = file->inner_file->pMethods->xFileSize(file->inner_file, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }
    if (physical_size < MYLITE_FILE_PREAMBLE_SIZE) {
        return SQLITE_NOTADB;
    }
    rc =
        file->inner_file->pMethods->xRead(file->inner_file, preamble, MYLITE_FILE_PREAMBLE_SIZE, 0);
    if (rc != SQLITE_OK) {
        return rc;
    }
    lifecycle_state = mylite_file_preamble_lifecycle_state(preamble);
    if (lifecycle_state == MYLITE_FILE_LIFECYCLE_INVALID) {
        return SQLITE_NOTADB;
    }
    file->format_version = mylite_file_preamble_format_version(preamble);
    if (!format_uses_lock_gap(file) && physical_size > MYLITE_FILE_PHYSICAL_LOCK_BYTE) {
        return SQLITE_NOTADB;
    }

    if (lifecycle_state == MYLITE_FILE_LIFECYCLE_COMMITTED) {
        return validate_sqlite_payload_header(file);
    }

    rc = acquire_initialization_lock(file);
    if (rc != SQLITE_OK) {
        return rc;
    }
    file->initialization_preamble_written = true;
    if (physical_size == MYLITE_FILE_PREAMBLE_SIZE) {
        return SQLITE_OK;
    }
    return validate_sqlite_payload_header(file);
}

static int acquire_initialization_lock(struct mylite_offset_file *file) {
    int rc = SQLITE_OK;

    if (file->initialization_lock_held) {
        return SQLITE_OK;
    }
    rc = file->inner_file->pMethods->xLock(file->inner_file, SQLITE_LOCK_EXCLUSIVE);
    if (rc == SQLITE_OK) {
        file->initialization_owner = true;
        file->initialization_lock_held = true;
    }
    return rc;
}

static int release_initialization_lock(struct mylite_offset_file *file) {
    int rc = SQLITE_OK;

    if (!file->initialization_lock_held) {
        return SQLITE_OK;
    }
    rc = file->inner_file->pMethods->xUnlock(file->inner_file, SQLITE_LOCK_NONE);
    if (rc == SQLITE_OK) {
        file->initialization_lock_held = false;
    }
    return rc;
}

static int validate_sector_alignment(struct mylite_offset_file *file) {
    int sector_size = MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;

    if (file->inner_file->pMethods->xSectorSize != NULL) {
        sector_size = file->inner_file->pMethods->xSectorSize(file->inner_file);
    }

    if (sector_size <= 0 || MYLITE_FILE_SQLITE_PAYLOAD_OFFSET % sector_size != 0) {
        return SQLITE_CANTOPEN;
    }

    return SQLITE_OK;
}

static int validate_sqlite_payload_header(struct mylite_offset_file *file) {
    static const unsigned char sqlite_header[] = "SQLite format 3";
    unsigned char actual_header[sizeof(sqlite_header)];
    sqlite3_int64 physical_size = 0;
    int rc = file->inner_file->pMethods->xFileSize(file->inner_file, &physical_size);

    if (rc != SQLITE_OK) {
        return rc;
    }
    if (physical_size < (sqlite3_int64)MYLITE_FILE_SQLITE_PAYLOAD_OFFSET +
                            (sqlite3_int64)MYLITE_FILE_SQLITE_MINIMUM_DATABASE_SIZE) {
        return SQLITE_NOTADB;
    }
    rc = file->inner_file->pMethods->xRead(
        file->inner_file,
        actual_header,
        (int)sizeof(actual_header),
        MYLITE_FILE_SQLITE_PAYLOAD_OFFSET
    );
    if (rc != SQLITE_OK || memcmp(actual_header, sqlite_header, sizeof(sqlite_header)) != 0) {
        return SQLITE_NOTADB;
    }

    return SQLITE_OK;
}

static int transition_initialization_state(
    struct mylite_offset_file *file,
    enum mylite_file_lifecycle_state state
) {
    unsigned char recovery_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char stored_state = (unsigned char)state;
    int rc = SQLITE_OK;

    if (!file->initialization_owner) {
        return SQLITE_OK;
    }
    if (state == MYLITE_FILE_LIFECYCLE_COMMITTED) {
        rc = validate_sqlite_payload_header(file);
        if (rc != SQLITE_OK) {
            return rc;
        }
    }
    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_WRITE)) {
        return SQLITE_IOERR_WRITE;
    }
    if (file->initialization_preamble_written) {
        rc = file->inner_file->pMethods
                 ->xWrite(file->inner_file, &stored_state, 1, MYLITE_FILE_LIFECYCLE_STATE_OFFSET);
    } else {
        mylite_file_preamble_init_with_state(recovery_preamble, state);
        rc = file->inner_file->pMethods
                 ->xWrite(file->inner_file, recovery_preamble, MYLITE_FILE_PREAMBLE_SIZE, 0);
        if (rc == SQLITE_OK) {
            file->initialization_preamble_written = true;
        }
    }
    if (rc != SQLITE_OK) {
        return rc;
    }
    if (inject_fault(MYLITE_STORAGE_VFS_FAULT_SYNC)) {
        return SQLITE_IOERR_FSYNC;
    }
    rc = file->inner_file->pMethods->xSync(file->inner_file, SQLITE_SYNC_FULL);
    if (rc == SQLITE_OK) {
        file->initialization_owner = false;
        rc = release_initialization_lock(file);
    }

    return rc;
}

static int read_shifted(
    struct mylite_offset_file *file,
    void *buffer,
    int amount,
    sqlite3_int64 logical_offset
) {
    sqlite3_int64 logical_end = 0;
    sqlite3_int64 physical_offset = 0;
    int first_amount = 0;
    int rc = SQLITE_OK;

    if (buffer == NULL || !logical_range_end(logical_offset, amount, &logical_end)) {
        return SQLITE_IOERR_READ;
    }
    if (!format_uses_lock_gap(file) && logical_end > MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE) {
        memset(buffer, 0, (size_t)amount);
        return SQLITE_IOERR_SHORT_READ;
    }
    if (!logical_offset_to_physical(file, logical_offset, &physical_offset)) {
        return SQLITE_IOERR_READ;
    }
    if (!format_uses_lock_gap(file) || logical_offset >= MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET ||
        logical_end <= MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET) {
        return file->inner_file->pMethods->xRead(file->inner_file, buffer, amount, physical_offset);
    }

    first_amount = (int)(MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - logical_offset);
    rc = file->inner_file->pMethods->xRead(file->inner_file, buffer, first_amount, physical_offset);
    if (rc != SQLITE_OK) {
        return rc;
    }
    if (!logical_offset_to_physical(file, MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET, &physical_offset)) {
        return SQLITE_IOERR_READ;
    }

    return file->inner_file->pMethods->xRead(
        file->inner_file,
        (unsigned char *)buffer + first_amount,
        amount - first_amount,
        physical_offset
    );
}

static int write_shifted(
    struct mylite_offset_file *file,
    const void *buffer,
    int amount,
    sqlite3_int64 logical_offset
) {
    sqlite3_int64 logical_end = 0;
    sqlite3_int64 physical_offset = 0;
    int first_amount = 0;
    int rc = SQLITE_OK;

    if (buffer == NULL || !logical_range_end(logical_offset, amount, &logical_end)) {
        return SQLITE_IOERR_WRITE;
    }
    if (!format_uses_lock_gap(file) && logical_end > MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE) {
        return SQLITE_FULL;
    }
    if (!logical_offset_to_physical(file, logical_offset, &physical_offset)) {
        return SQLITE_IOERR_WRITE;
    }
    if (!format_uses_lock_gap(file) || logical_offset >= MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET ||
        logical_end <= MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET) {
        return file->inner_file->pMethods
            ->xWrite(file->inner_file, buffer, amount, physical_offset);
    }

    first_amount = (int)(MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - logical_offset);
    rc =
        file->inner_file->pMethods->xWrite(file->inner_file, buffer, first_amount, physical_offset);
    if (rc != SQLITE_OK) {
        return rc;
    }
    if (!logical_offset_to_physical(file, MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET, &physical_offset)) {
        return SQLITE_IOERR_WRITE;
    }

    return file->inner_file->pMethods->xWrite(
        file->inner_file,
        (const unsigned char *)buffer + first_amount,
        amount - first_amount,
        physical_offset
    );
}

static bool logical_range_end(sqlite3_int64 offset, int amount, sqlite3_int64 *out_end) {
    if (offset < 0 || amount < 0 || offset > INT64_MAX - amount) {
        return false;
    }

    *out_end = offset + amount;
    return true;
}

static bool logical_offset_to_physical(
    const struct mylite_offset_file *file,
    sqlite3_int64 logical,
    sqlite3_int64 *out_physical
) {
    sqlite3_int64 physical_offset = MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;

    if (format_uses_lock_gap(file) && logical >= MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET) {
        physical_offset += MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;
    }
    if (logical < 0 || logical > INT64_MAX - physical_offset) {
        return false;
    }

    *out_physical = logical + physical_offset;
    return true;
}

static bool logical_size_to_physical(
    const struct mylite_offset_file *file,
    sqlite3_int64 logical,
    sqlite3_int64 *out_physical
) {
    sqlite3_int64 physical_offset = MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;

    if (format_uses_lock_gap(file) && logical > MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET) {
        physical_offset += MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;
    }
    if (logical < 0 || logical > INT64_MAX - physical_offset) {
        return false;
    }

    *out_physical = logical + physical_offset;
    return true;
}

static bool logical_size_from_physical(
    const struct mylite_offset_file *file,
    sqlite3_int64 physical,
    sqlite3_int64 *out_logical
) {
    if (physical < 0) {
        return false;
    }
    if (physical <= MYLITE_FILE_SQLITE_PAYLOAD_OFFSET) {
        *out_logical = 0;
        return true;
    }
    if (!format_uses_lock_gap(file) || physical <= MYLITE_FILE_PHYSICAL_LOCK_BYTE) {
        *out_logical = physical - MYLITE_FILE_SQLITE_PAYLOAD_OFFSET;
        return true;
    }
    if (physical <= MYLITE_FILE_PHYSICAL_LOCK_BYTE + MYLITE_FILE_SQLITE_PAYLOAD_OFFSET) {
        return false;
    }

    *out_logical = physical - (2 * (sqlite3_int64)MYLITE_FILE_SQLITE_PAYLOAD_OFFSET);
    return true;
}

static bool format_uses_lock_gap(const struct mylite_offset_file *file) {
    return file->format_version >= MYLITE_FILE_FORMAT_VERSION;
}

static int atomic_write_capability_mask(void) {
    return SQLITE_IOCAP_ATOMIC | SQLITE_IOCAP_ATOMIC512 | SQLITE_IOCAP_ATOMIC1K |
           SQLITE_IOCAP_ATOMIC2K | SQLITE_IOCAP_ATOMIC4K | SQLITE_IOCAP_ATOMIC8K |
           SQLITE_IOCAP_ATOMIC16K | SQLITE_IOCAP_ATOMIC32K | SQLITE_IOCAP_ATOMIC64K |
           SQLITE_IOCAP_BATCH_ATOMIC;
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
    if (!format_uses_lock_gap(file) && original_size > MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE) {
        return SQLITE_FULL;
    }
    if (original_size >= 0 && !logical_size_to_physical(file, original_size, &physical_size)) {
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

    if (!logical_size_from_physical(file, *size, size)) {
        *size = original_size;
        return SQLITE_IOERR;
    }
    if (!format_uses_lock_gap(file) && *size > MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE) {
        *size = MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE;
    }

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

static bool inject_fault(enum mylite_storage_vfs_fault_operation operation) {
    if (injected_fault.operation != operation) {
        return false;
    }
    injected_fault.matching_call_count += 1U;
    if (injected_fault.triggered) {
        return false;
    }
    if (injected_fault.calls_until_failure > 1U) {
        injected_fault.calls_until_failure -= 1U;
        return false;
    }

    injected_fault.calls_until_failure = 0U;
    injected_fault.triggered = true;
    return true;
}
