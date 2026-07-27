#include <mylite/mylite.h>

#include "mylite_fuzzer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum {
    path_capacity = 256,
    structured_mutation_offset_bytes = 4,
    bits_per_byte = 8,
};

static bool input_requests_structured_seed(const uint8_t *data, size_t size);
static int create_seed_database(const char *path, bool rich);
static int execute_seed_sql(mylite_db *database, const char *sql);
static int mutate_seed_database(const char *path, const uint8_t *data, size_t size);
static int write_input(const char *path, const uint8_t *data, size_t size);
static void exercise_open_database(mylite_db *database);
static void remove_database_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char path[path_capacity];
    mylite_db *database = NULL;
    bool structured_seed = input_requests_structured_seed(data, size);
    int length = snprintf(path, sizeof(path), "/tmp/mylite-open-fuzzer-%ld.mylite", (long)getpid());

    if (length < 0 || (size_t)length >= sizeof(path)) {
        return 0;
    }

    remove_database_files(path);
    if (structured_seed) {
        if (create_seed_database(path, (data[0] & 2U) != 0U) != 0 ||
            mutate_seed_database(path, data + 1U, size - 1U) != 0) {
            remove_database_files(path);
            return 0;
        }
    } else if (write_input(path, data, size) != 0) {
        remove_database_files(path);
        return 0;
    }
    if (mylite_open(path, &database) == MYLITE_OK) {
        exercise_open_database(database);
        mylite_close(database);
    }
    remove_database_files(path);
    return 0;
}

static bool input_requests_structured_seed(const uint8_t *data, size_t size) {
    return data != NULL && size > 0U && (data[0] & 1U) != 0U;
}

static int create_seed_database(const char *path, bool rich) {
    mylite_db *database = NULL;
    int rc = mylite_open(path, &database);

    if (rc != MYLITE_OK) {
        mylite_close(database);
        return -1;
    }
    if (rich) {
        rc = execute_seed_sql(database, "CREATE DATABASE fuzz");
    }
    if (rc == MYLITE_OK && rich) {
        rc = execute_seed_sql(
            database,
            "CREATE TABLE fuzz.parent ("
            "id INT PRIMARY KEY, label VARCHAR(32) NOT NULL, "
            "generated_id INT GENERATED ALWAYS AS (id + 1) STORED, "
            "CONSTRAINT positive_id CHECK (id > 0))"
        );
    }
    if (rc == MYLITE_OK && rich) {
        rc = execute_seed_sql(
            database,
            "CREATE TABLE fuzz.child ("
            "id INT PRIMARY KEY, parent_id INT, KEY parent_key (parent_id), "
            "CONSTRAINT child_parent FOREIGN KEY (parent_id) REFERENCES fuzz.parent (id))"
        );
    }
    if (rc == MYLITE_OK && rich) {
        rc = execute_seed_sql(database, "INSERT INTO fuzz.parent (id, label) VALUES (1, 'seed')");
    }
    if (rc == MYLITE_OK && rich) {
        rc = execute_seed_sql(database, "INSERT INTO fuzz.child (id, parent_id) VALUES (1, 1)");
    }
    mylite_close(database);
    return rc == MYLITE_OK ? 0 : -1;
}

static int execute_seed_sql(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    mylite_result_free(result);
    return rc;
}

static int mutate_seed_database(const char *path, const uint8_t *data, size_t size) {
    FILE *file = NULL;
    long file_size = 0;
    uint32_t requested_offset = 0U;
    size_t patch_size = 0U;
    long patch_offset = 0;
    int rc = 0;

    if (size <= structured_mutation_offset_bytes) {
        return 0;
    }
    for (size_t index = 0U; index < structured_mutation_offset_bytes; ++index) {
        requested_offset |= (uint32_t)data[index] << (index * bits_per_byte);
    }

    file = fopen(path, "r+b");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        rc = -1;
    }
    if (rc == 0) {
        file_size = ftell(file);
        if (file_size <= 0) {
            rc = -1;
        }
    }
    if (rc == 0) {
        patch_offset = (long)((uint64_t)requested_offset % (uint64_t)file_size);
        patch_size = size - structured_mutation_offset_bytes;
        if (patch_size > (size_t)(file_size - patch_offset)) {
            patch_size = (size_t)(file_size - patch_offset);
        }
        if (fseek(file, patch_offset, SEEK_SET) != 0 ||
            fwrite(data + structured_mutation_offset_bytes, 1U, patch_size, file) != patch_size) {
            rc = -1;
        }
    }
    if (file != NULL && fclose(file) != 0) {
        rc = -1;
    }
    return rc;
}

static int write_input(const char *path, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    int rc = 0;

    if (file == NULL) {
        return -1;
    }
    if (size > 0U && fwrite(data, 1U, size, file) != size) {
        rc = -1;
    }
    if (fclose(file) != 0) {
        rc = -1;
    }
    return rc;
}

static void exercise_open_database(mylite_db *database) {
    static const char *const sql[] = {
        "SELECT COUNT(*) FROM information_schema.tables",
        "SELECT id, label, generated_id FROM fuzz.parent",
        "SHOW TABLES FROM fuzz",
    };

    for (size_t index = 0U; index < sizeof(sql) / sizeof(sql[0U]); ++index) {
        mylite_result *result = NULL;

        (void)mylite_execute(database, sql[index], strlen(sql[index]), &result);
        mylite_result_free(result);
    }
}

static void remove_database_files(const char *path) {
    (void)remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char suffixed_path[path_capacity];
    int length = snprintf(suffixed_path, sizeof(suffixed_path), "%s%s", path, suffix);

    if (length >= 0 && (size_t)length < sizeof(suffixed_path)) {
        (void)remove(suffixed_path);
    }
}
