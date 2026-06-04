#include "mylite_catalog_internal.h"

#include "sqlite3.h"

#include <stdint.h>

enum {
    catalog_schema_version_v5 = 5U,
    catalog_schema_version_v6 = 6U,
    catalog_schema_version_v7 = 7U,
    catalog_schema_version_v8 = 8U,
    catalog_schema_version_v9 = 9U,
    catalog_schema_version_v10 = 10U,
    catalog_schema_version_v11 = 11U,
    catalog_schema_version_v12 = 12U,
    catalog_schema_version_v13 = 13U,
    catalog_schema_version_v14 = 14U,
    catalog_schema_version_v15 = 15U,
    catalog_schema_version_v16 = 16U,
    catalog_schema_version_v17 = 17U,
    catalog_schema_version_v18 = 18U,
    catalog_schema_version_v19 = 19U,
    catalog_schema_version_v20 = 20U,
    catalog_schema_version_v21 = 21U,
    catalog_schema_version_v22 = 22U,
    catalog_schema_version_v23 = 23U,
    catalog_schema_version_v24 = 24U,
    catalog_schema_version_v25 = 25U,
    catalog_schema_version_v26 = 26U,
    catalog_schema_version_v27 = 27U,
    catalog_schema_version_v28 = 28U,
    catalog_schema_version_v29 = 29U,
    catalog_schema_version_v30 = 30U,
    catalog_schema_version_v31 = 31U,
    catalog_schema_version_v32 = 32U,
};

static int migrate_catalog_schema_v1_to_v2(sqlite3 *sqlite);
static int migrate_catalog_schema_v2_to_v3(sqlite3 *sqlite);
static int migrate_catalog_schema_v3_to_v4(sqlite3 *sqlite);
static int migrate_catalog_schema_v4_to_v5(sqlite3 *sqlite);
static int migrate_catalog_schema_v5_to_v6(sqlite3 *sqlite);
static int migrate_catalog_schema_v6_to_v7(sqlite3 *sqlite);
static int migrate_catalog_schema_v7_to_v8(sqlite3 *sqlite);
static int migrate_catalog_schema_v8_to_v9(sqlite3 *sqlite);
static int migrate_catalog_schema_v9_to_v10(sqlite3 *sqlite);
static int migrate_catalog_schema_v10_to_v11(sqlite3 *sqlite);
static int migrate_catalog_schema_v11_to_v12(sqlite3 *sqlite);
static int migrate_catalog_schema_v12_to_v13(sqlite3 *sqlite);
static int migrate_catalog_schema_v13_to_v14(sqlite3 *sqlite);
static int migrate_catalog_schema_v14_to_v15(sqlite3 *sqlite);
static int migrate_catalog_schema_v15_to_v16(sqlite3 *sqlite);
static int migrate_catalog_schema_v16_to_v17(sqlite3 *sqlite);
static int migrate_catalog_schema_v17_to_v18(sqlite3 *sqlite);
static int migrate_catalog_schema_v18_to_v19(sqlite3 *sqlite);
static int migrate_catalog_schema_v19_to_v20(sqlite3 *sqlite);
static int migrate_catalog_schema_v20_to_v21(sqlite3 *sqlite);
static int migrate_catalog_schema_v21_to_v22(sqlite3 *sqlite);
static int migrate_catalog_schema_v22_to_v23(sqlite3 *sqlite);
static int migrate_catalog_schema_v23_to_v24(sqlite3 *sqlite);
static int migrate_catalog_schema_v24_to_v25(sqlite3 *sqlite);
static int migrate_catalog_schema_v25_to_v26(sqlite3 *sqlite);
static int migrate_catalog_schema_v26_to_v27(sqlite3 *sqlite);
static int migrate_catalog_schema_v27_to_v28(sqlite3 *sqlite);
static int migrate_catalog_schema_v28_to_v29(sqlite3 *sqlite);
static int migrate_catalog_schema_v29_to_v30(sqlite3 *sqlite);
static int migrate_catalog_schema_v30_to_v31(sqlite3 *sqlite);
static int migrate_catalog_schema_v31_to_v32(sqlite3 *sqlite);
static int migrate_catalog_schema_v32_to_v33(sqlite3 *sqlite);
static void rollback_catalog_transaction(sqlite3 *sqlite);

int mylite_catalog_migrate_schema_one_step(sqlite3 *sqlite, uint32_t *schema_version) {
    uint32_t next_schema_version = 0U;
    int rc = MYLITE_ERROR;

    if (schema_version == NULL) {
        return MYLITE_MISUSE;
    }

    switch (*schema_version) {
    case 1U:
        rc = migrate_catalog_schema_v1_to_v2(sqlite);
        next_schema_version = 2U;
        break;
    case 2U:
        rc = migrate_catalog_schema_v2_to_v3(sqlite);
        next_schema_version = 3U;
        break;
    case 3U:
        rc = migrate_catalog_schema_v3_to_v4(sqlite);
        next_schema_version = 4U;
        break;
    case 4U:
        rc = migrate_catalog_schema_v4_to_v5(sqlite);
        next_schema_version = catalog_schema_version_v5;
        break;
    case catalog_schema_version_v5:
        rc = migrate_catalog_schema_v5_to_v6(sqlite);
        next_schema_version = catalog_schema_version_v6;
        break;
    case catalog_schema_version_v6:
        rc = migrate_catalog_schema_v6_to_v7(sqlite);
        next_schema_version = catalog_schema_version_v7;
        break;
    case catalog_schema_version_v7:
        rc = migrate_catalog_schema_v7_to_v8(sqlite);
        next_schema_version = catalog_schema_version_v8;
        break;
    case catalog_schema_version_v8:
        rc = migrate_catalog_schema_v8_to_v9(sqlite);
        next_schema_version = catalog_schema_version_v9;
        break;
    case catalog_schema_version_v9:
        rc = migrate_catalog_schema_v9_to_v10(sqlite);
        next_schema_version = catalog_schema_version_v10;
        break;
    case catalog_schema_version_v10:
        rc = migrate_catalog_schema_v10_to_v11(sqlite);
        next_schema_version = catalog_schema_version_v11;
        break;
    case catalog_schema_version_v11:
        rc = migrate_catalog_schema_v11_to_v12(sqlite);
        next_schema_version = catalog_schema_version_v12;
        break;
    case catalog_schema_version_v12:
        rc = migrate_catalog_schema_v12_to_v13(sqlite);
        next_schema_version = catalog_schema_version_v13;
        break;
    case catalog_schema_version_v13:
        rc = migrate_catalog_schema_v13_to_v14(sqlite);
        next_schema_version = catalog_schema_version_v14;
        break;
    case catalog_schema_version_v14:
        rc = migrate_catalog_schema_v14_to_v15(sqlite);
        next_schema_version = catalog_schema_version_v15;
        break;
    case catalog_schema_version_v15:
        rc = migrate_catalog_schema_v15_to_v16(sqlite);
        next_schema_version = catalog_schema_version_v16;
        break;
    case catalog_schema_version_v16:
        rc = migrate_catalog_schema_v16_to_v17(sqlite);
        next_schema_version = catalog_schema_version_v17;
        break;
    case catalog_schema_version_v17:
        rc = migrate_catalog_schema_v17_to_v18(sqlite);
        next_schema_version = catalog_schema_version_v18;
        break;
    case catalog_schema_version_v18:
        rc = migrate_catalog_schema_v18_to_v19(sqlite);
        next_schema_version = catalog_schema_version_v19;
        break;
    case catalog_schema_version_v19:
        rc = migrate_catalog_schema_v19_to_v20(sqlite);
        next_schema_version = catalog_schema_version_v20;
        break;
    case catalog_schema_version_v20:
        rc = migrate_catalog_schema_v20_to_v21(sqlite);
        next_schema_version = catalog_schema_version_v21;
        break;
    case catalog_schema_version_v21:
        rc = migrate_catalog_schema_v21_to_v22(sqlite);
        next_schema_version = catalog_schema_version_v22;
        break;
    case catalog_schema_version_v22:
        rc = migrate_catalog_schema_v22_to_v23(sqlite);
        next_schema_version = catalog_schema_version_v23;
        break;
    case catalog_schema_version_v23:
        rc = migrate_catalog_schema_v23_to_v24(sqlite);
        next_schema_version = catalog_schema_version_v24;
        break;
    case catalog_schema_version_v24:
        rc = migrate_catalog_schema_v24_to_v25(sqlite);
        next_schema_version = catalog_schema_version_v25;
        break;
    case catalog_schema_version_v25:
        rc = migrate_catalog_schema_v25_to_v26(sqlite);
        next_schema_version = catalog_schema_version_v26;
        break;
    case catalog_schema_version_v26:
        rc = migrate_catalog_schema_v26_to_v27(sqlite);
        next_schema_version = catalog_schema_version_v27;
        break;
    case catalog_schema_version_v27:
        rc = migrate_catalog_schema_v27_to_v28(sqlite);
        next_schema_version = catalog_schema_version_v28;
        break;
    case catalog_schema_version_v28:
        rc = migrate_catalog_schema_v28_to_v29(sqlite);
        next_schema_version = catalog_schema_version_v29;
        break;
    case catalog_schema_version_v29:
        rc = migrate_catalog_schema_v29_to_v30(sqlite);
        next_schema_version = catalog_schema_version_v30;
        break;
    case catalog_schema_version_v30:
        rc = migrate_catalog_schema_v30_to_v31(sqlite);
        next_schema_version = catalog_schema_version_v31;
        break;
    case catalog_schema_version_v31:
        rc = migrate_catalog_schema_v31_to_v32(sqlite);
        next_schema_version = catalog_schema_version_v32;
        break;
    case catalog_schema_version_v32:
        rc = migrate_catalog_schema_v32_to_v33(sqlite);
        next_schema_version = MYLITE_CATALOG_SCHEMA_VERSION;
        break;
    default:
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *schema_version = next_schema_version;
    }

    return rc;
}

static int migrate_catalog_schema_v1_to_v2(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN default_kind INTEGER NOT NULL DEFAULT 0;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN default_integer INTEGER;"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 2, minimum_reader_schema_version = 2;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v2_to_v3(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v2;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v2;"
        "DROP TABLE _mylite_catalog_columns_v2;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 3, minimum_reader_schema_version = 3;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v3_to_v4(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v3;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, 1, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v3;"
        "DROP TABLE _mylite_catalog_columns_v3;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 4, minimum_reader_schema_version = 4;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v4_to_v5(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "CREATE TABLE _mylite_catalog_indexes ("
                             "index_id INTEGER PRIMARY KEY,"
                             "table_id INTEGER NOT NULL,"
                             "name TEXT NOT NULL,"
                             "kind INTEGER NOT NULL CHECK(kind = 1),"
                             "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
                             "physical_name TEXT NOT NULL UNIQUE,"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(table_id, name)"
                             ");"
                             "CREATE TABLE _mylite_catalog_index_columns ("
                             "index_column_id INTEGER PRIMARY KEY,"
                             "index_id INTEGER NOT NULL,"
                             "table_id INTEGER NOT NULL,"
                             "column_id INTEGER NOT NULL,"
                             "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(index_id, ordinal_position),"
                             "UNIQUE(index_id, column_id)"
                             ");"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 5, minimum_reader_schema_version = 5;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v5_to_v6(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN auto_increment_next INTEGER NOT NULL DEFAULT 1;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN is_auto_increment INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(is_auto_increment IN (0, 1));"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 6, minimum_reader_schema_version = 6;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v6_to_v7(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v6;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "NULL, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v6;"
        "DROP TABLE _mylite_catalog_columns_v6;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 7, minimum_reader_schema_version = 7;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v7_to_v8(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v7;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v7;"
        "DROP TABLE _mylite_catalog_columns_v7;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 8, minimum_reader_schema_version = 8;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v8_to_v9(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes RENAME TO _mylite_catalog_indexes_v8;"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes_v8;"
        "DROP TABLE _mylite_catalog_indexes_v8;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 9, minimum_reader_schema_version = 9;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v9_to_v10(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_index_columns "
        "ADD COLUMN prefix_length INTEGER CHECK(prefix_length IS NULL OR prefix_length > 0);"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 10, minimum_reader_schema_version = 10;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v10_to_v11(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v10;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v10;"
        "DROP TABLE _mylite_catalog_columns_v10;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 11, minimum_reader_schema_version = 11;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v11_to_v12(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v11;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6, 7)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, 0, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation FROM _mylite_catalog_columns_v11;"
        "DROP TABLE _mylite_catalog_columns_v11;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 12, minimum_reader_schema_version = 12;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v12_to_v13(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN default_charset TEXT NOT NULL DEFAULT '" MYLITE_CATALOG_DEFAULT_TABLE_CHARSET
        "';"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN default_collation TEXT NOT NULL DEFAULT "
        "'" MYLITE_CATALOG_DEFAULT_TABLE_COLLATION "';"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 13, minimum_reader_schema_version = 13;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v13_to_v14(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "CREATE TABLE IF NOT EXISTS _mylite_catalog_foreign_keys ("
                             "foreign_key_id INTEGER PRIMARY KEY,"
                             "child_table_id INTEGER NOT NULL,"
                             "parent_table_id INTEGER NOT NULL,"
                             "name TEXT NOT NULL,"
                             "parent_index_id INTEGER NOT NULL,"
                             "child_index_id INTEGER NOT NULL,"
                             "update_rule TEXT NOT NULL,"
                             "delete_rule TEXT NOT NULL,"
                             "match_option TEXT NOT NULL,"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(child_table_id, name)"
                             ");"
                             "CREATE TABLE IF NOT EXISTS _mylite_catalog_foreign_key_columns ("
                             "foreign_key_column_id INTEGER PRIMARY KEY,"
                             "foreign_key_id INTEGER NOT NULL,"
                             "child_table_id INTEGER NOT NULL,"
                             "parent_table_id INTEGER NOT NULL,"
                             "child_column_id INTEGER NOT NULL,"
                             "parent_column_id INTEGER NOT NULL,"
                             "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
                             "position_in_unique_constraint INTEGER NOT NULL "
                             "CHECK(position_in_unique_constraint > 0),"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(foreign_key_id, ordinal_position)"
                             ");"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 14, minimum_reader_schema_version = 14;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v14_to_v15(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v14;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v14;"
        "DROP TABLE _mylite_catalog_columns_v14;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 15, minimum_reader_schema_version = 15;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v15_to_v16(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN character_set_name TEXT NOT NULL DEFAULT '';"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN collation_name TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 16, minimum_reader_schema_version = 16;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v16_to_v17(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_index_columns "
                             "ADD COLUMN sort_direction INTEGER NOT NULL DEFAULT 1 "
                             "CHECK(sort_direction IN (1, 2));"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 17, minimum_reader_schema_version = 17;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v17_to_v18(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "CREATE TABLE IF NOT EXISTS _mylite_catalog_check_constraints ("
        "check_constraint_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "physical_name TEXT NOT NULL,"
        "check_clause TEXT NOT NULL,"
        "sqlite_expression TEXT NOT NULL,"
        "is_enforced INTEGER NOT NULL CHECK(is_enforced IN (0, 1)),"
        "name_is_generated INTEGER NOT NULL CHECK(name_is_generated IN (0, 1)),"
        "generated_ordinal INTEGER NOT NULL CHECK(generated_ordinal > 0),"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name),"
        "UNIQUE(table_id, physical_name),"
        "UNIQUE(table_id, ordinal_position)"
        ");"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 18, minimum_reader_schema_version = 18;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v18_to_v19(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes RENAME TO _mylite_catalog_indexes_v18;"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2, 3, 4)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes_v18;"
        "DROP TABLE _mylite_catalog_indexes_v18;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 19, minimum_reader_schema_version = 19;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v19_to_v20(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN fulltext_doc_id_initialized INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(fulltext_doc_id_initialized IN (0, 1));"
                             "UPDATE _mylite_catalog_tables "
                             "SET fulltext_doc_id_initialized = 1 "
                             "WHERE table_id IN ("
                             "SELECT DISTINCT table_id FROM _mylite_catalog_indexes WHERE kind = 3"
                             ");"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 20, minimum_reader_schema_version = 20;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v20_to_v21(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN created_time_utc_epoch INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(created_time_utc_epoch >= 0);"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN updated_time_utc_epoch INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(updated_time_utc_epoch >= 0);"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 21, minimum_reader_schema_version = 21;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v21_to_v22(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_schemas "
        "ADD COLUMN default_charset TEXT NOT NULL DEFAULT '" MYLITE_CATALOG_DEFAULT_TABLE_CHARSET
        "';"
        "ALTER TABLE _mylite_catalog_schemas "
        "ADD COLUMN default_collation TEXT NOT NULL DEFAULT "
        "'" MYLITE_CATALOG_DEFAULT_TABLE_COLLATION "';"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 22, minimum_reader_schema_version = 22;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v22_to_v23(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_indexes "
                             "ADD COLUMN is_visible INTEGER NOT NULL DEFAULT 1 "
                             "CHECK(is_visible IN (0, 1));"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 23, minimum_reader_schema_version = 23;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v23_to_v24(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN comment TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 24, minimum_reader_schema_version = 24;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v24_to_v25(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes ADD COLUMN comment TEXT NOT NULL DEFAULT '';"
        "ALTER TABLE _mylite_catalog_indexes "
        "ADD COLUMN show_create_explicit_btree INTEGER NOT NULL DEFAULT 0 "
        "CHECK(show_create_explicit_btree IN (0, 1));"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 25, minimum_reader_schema_version = 25;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v25_to_v26(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN comment TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 26, minimum_reader_schema_version = 26;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v26_to_v27(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes RENAME TO _mylite_catalog_indexes_v26;"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2, 3, 4)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "comment TEXT NOT NULL,"
        "show_create_explicit_btree INTEGER NOT NULL "
        "CHECK(show_create_explicit_btree IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, is_visible, physical_name, comment, "
        "show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation) "
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, comment, "
        "show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes_v26;"
        "DROP TABLE _mylite_catalog_indexes_v26;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 27, minimum_reader_schema_version = 27;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v27_to_v28(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v27;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "character_set_name TEXT NOT NULL,"
        "collation_name TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v27;"
        "DROP TABLE _mylite_catalog_columns_v27;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 28, minimum_reader_schema_version = 28;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v28_to_v29(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v28;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN "
        "(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "character_set_name TEXT NOT NULL,"
        "collation_name TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v28;"
        "DROP TABLE _mylite_catalog_columns_v28;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 29, minimum_reader_schema_version = 29;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v29_to_v30(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN is_generated INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(is_generated IN (0, 1));"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN generated_kind INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(generated_kind IN (0, 1, 2));"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN generation_expression TEXT NOT NULL DEFAULT '';"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN sqlite_generation_expression TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 30, minimum_reader_schema_version = 30;"
                             "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v30_to_v31(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN row_format_option TEXT NOT NULL DEFAULT '';"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN key_block_size INTEGER NOT NULL DEFAULT 0 "
        "CHECK(key_block_size IN (0, 1, 2, 4, 8, 16));"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN pack_keys INTEGER NOT NULL DEFAULT -1 CHECK(pack_keys IN (-1, 0, 1));"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN checksum INTEGER NOT NULL DEFAULT 0 CHECK(checksum IN (0, 1));"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN stats_persistent INTEGER NOT NULL DEFAULT -1 "
        "CHECK(stats_persistent IN (-1, 0, 1));"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN stats_auto_recalc INTEGER NOT NULL DEFAULT -1 "
        "CHECK(stats_auto_recalc IN (-1, 0, 1));"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN stats_sample_pages INTEGER NOT NULL DEFAULT 0 "
        "CHECK(stats_sample_pages BETWEEN 0 AND 65535);"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 31, minimum_reader_schema_version = 30;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v31_to_v32(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_tables RENAME TO _mylite_catalog_tables_v31;"
        "CREATE TABLE _mylite_catalog_tables ("
        "table_id INTEGER PRIMARY KEY,"
        "schema_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 3)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "auto_increment_next INTEGER NOT NULL CHECK(auto_increment_next > 0),"
        "default_charset TEXT NOT NULL,"
        "default_collation TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "row_format_option TEXT NOT NULL DEFAULT '',"
        "key_block_size INTEGER NOT NULL DEFAULT 0 "
        "CHECK(key_block_size IN (0, 1, 2, 4, 8, 16)),"
        "pack_keys INTEGER NOT NULL DEFAULT -1 CHECK(pack_keys IN (-1, 0, 1)),"
        "checksum INTEGER NOT NULL DEFAULT 0 CHECK(checksum IN (0, 1)),"
        "stats_persistent INTEGER NOT NULL DEFAULT -1 CHECK(stats_persistent IN (-1, 0, 1)),"
        "stats_auto_recalc INTEGER NOT NULL DEFAULT -1 "
        "CHECK(stats_auto_recalc IN (-1, 0, 1)),"
        "stats_sample_pages INTEGER NOT NULL DEFAULT 0 "
        "CHECK(stats_sample_pages BETWEEN 0 AND 65535),"
        "fulltext_doc_id_initialized INTEGER NOT NULL "
        "CHECK(fulltext_doc_id_initialized IN (0, 1)),"
        "created_time_utc_epoch INTEGER NOT NULL CHECK(created_time_utc_epoch >= 0),"
        "updated_time_utc_epoch INTEGER NOT NULL CHECK(updated_time_utc_epoch >= 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(schema_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_tables "
        "(table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, row_format_option, key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "fulltext_doc_id_initialized, created_time_utc_epoch, updated_time_utc_epoch, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, row_format_option, key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "fulltext_doc_id_initialized, created_time_utc_epoch, updated_time_utc_epoch, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables_v31;"
        "DROP TABLE _mylite_catalog_tables_v31;"
        "DROP TABLE IF EXISTS _mylite_catalog_views;"
        "CREATE TABLE _mylite_catalog_views ("
        "table_id INTEGER PRIMARY KEY,"
        "view_definition TEXT NOT NULL,"
        "show_create_sql TEXT NOT NULL,"
        "check_option TEXT NOT NULL,"
        "is_updatable TEXT NOT NULL,"
        "definer TEXT NOT NULL,"
        "security_type TEXT NOT NULL,"
        "character_set_client TEXT NOT NULL,"
        "collation_connection TEXT NOT NULL,"
        "source_schema_id INTEGER NOT NULL,"
        "source_table_id INTEGER NOT NULL,"
        "source_schema_name TEXT NOT NULL,"
        "source_table_name TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL"
        ");"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 32, minimum_reader_schema_version = 32;"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v32_to_v33(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN auto_increment_status INTEGER NOT NULL DEFAULT 0 "
        "CHECK(auto_increment_status >= 0);"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = " MYLITE_CATALOG_SCHEMA_VERSION_TEXT
        ", minimum_reader_schema_version = " MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION_TEXT ";"
        "COMMIT;";
    int rc = mylite_catalog_execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static void rollback_catalog_transaction(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return;
    }

    (void)sqlite3_exec(sqlite, "ROLLBACK", NULL, NULL, NULL);
}
