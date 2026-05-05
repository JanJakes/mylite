#include "mylite_information_schema.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_information_schema_dynamic.h"
#include "mylite_information_schema_target.h"
#include "mylite_show.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_storage_engine.h"
#include "sqlite3.h"

#include <stdlib.h>

static int information_schema_dynamic_table_sql(mylite_db *database,
                                                enum mylite_information_schema_table table,
                                                char **out_sql);
static const char *information_schema_table_sql(enum mylite_information_schema_table table);

static const char information_schema_schemata_sql[] =
    "SELECT 'def' AS CATALOG_NAME,"
    "name AS SCHEMA_NAME,"
    "default_character_set AS DEFAULT_CHARACTER_SET_NAME,"
    "default_collation AS DEFAULT_COLLATION_NAME,"
    "NULL AS SQL_PATH,"
    "CASE WHEN upper(default_encryption) = 'Y' THEN 'YES' ELSE 'NO' END AS DEFAULT_ENCRYPTION "
    "FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";
static const char information_schema_tables_sql[] =
    "SELECT * FROM ("
    "SELECT 'def' AS TABLE_CATALOG,"
    "'information_schema' AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "'SYSTEM VIEW' AS TABLE_TYPE,"
    "NULL AS ENGINE,"
    "10 AS VERSION,"
    "NULL AS ROW_FORMAT,"
    "0 AS TABLE_ROWS,"
    "NULL AS AVG_ROW_LENGTH,"
    "NULL AS DATA_LENGTH,"
    "NULL AS MAX_DATA_LENGTH,"
    "NULL AS INDEX_LENGTH,"
    "NULL AS DATA_FREE,"
    "NULL AS AUTO_INCREMENT,"
    "'1970-01-01 00:00:00' AS CREATE_TIME,"
    "NULL AS UPDATE_TIME,"
    "NULL AS CHECK_TIME,"
    "NULL AS TABLE_COLLATION,"
    "NULL AS CHECKSUM,"
    "'' AS CREATE_OPTIONS,"
    "'' AS TABLE_COMMENT "
    "FROM ("
    "SELECT 'CHARACTER_SETS' AS table_name "
    "UNION ALL SELECT 'CHECK_CONSTRAINTS' "
    "UNION ALL SELECT 'COLLATION_CHARACTER_SET_APPLICABILITY' "
    "UNION ALL SELECT 'COLLATIONS' "
    "UNION ALL SELECT 'SCHEMATA' "
    "UNION ALL SELECT 'TABLES' "
    "UNION ALL SELECT 'COLUMNS' "
    "UNION ALL SELECT 'ENGINES' "
    "UNION ALL SELECT 'KEYWORDS' "
    "UNION ALL SELECT 'KEY_COLUMN_USAGE' "
    "UNION ALL SELECT 'REFERENTIAL_CONSTRAINTS' "
    "UNION ALL SELECT 'STATISTICS' "
    "UNION ALL SELECT 'TABLE_CONSTRAINTS') "
    "UNION ALL "
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "table_type AS TABLE_TYPE,"
    "engine AS ENGINE,"
    "version AS VERSION,"
    "row_format AS ROW_FORMAT,"
    "table_rows AS TABLE_ROWS,"
    "avg_row_length AS AVG_ROW_LENGTH,"
    "data_length AS DATA_LENGTH,"
    "max_data_length AS MAX_DATA_LENGTH,"
    "index_length AS INDEX_LENGTH,"
    "data_free AS DATA_FREE,"
    "auto_increment AS AUTO_INCREMENT,"
    "create_time AS CREATE_TIME,"
    "update_time AS UPDATE_TIME,"
    "check_time AS CHECK_TIME,"
    "table_collation AS TABLE_COLLATION,"
    "checksum AS CHECKSUM,"
    "create_options AS CREATE_OPTIONS,"
    "table_comment AS TABLE_COMMENT "
    "FROM __mylite_table_catalog) "
    "ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY";
static const char information_schema_columns_sql[] =
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "column_name AS COLUMN_NAME,"
    "ordinal_position AS ORDINAL_POSITION,"
    "column_default AS COLUMN_DEFAULT,"
    "is_nullable AS IS_NULLABLE,"
    "data_type AS DATA_TYPE,"
    "character_maximum_length AS CHARACTER_MAXIMUM_LENGTH,"
    "character_octet_length AS CHARACTER_OCTET_LENGTH,"
    "numeric_precision AS NUMERIC_PRECISION,"
    "numeric_scale AS NUMERIC_SCALE,"
    "datetime_precision AS DATETIME_PRECISION,"
    "character_set_name AS CHARACTER_SET_NAME,"
    "collation_name AS COLLATION_NAME,"
    "column_type AS COLUMN_TYPE,"
    "column_key AS COLUMN_KEY,"
    "extra AS EXTRA,"
    "privileges AS PRIVILEGES,"
    "column_comment AS COLUMN_COMMENT,"
    "generation_expression AS GENERATION_EXPRESSION,"
    "srs_id AS SRS_ID "
    "FROM __mylite_column_catalog "
    "ORDER BY table_schema COLLATE BINARY, table_name COLLATE BINARY, ordinal_position";
static const char information_schema_statistics_sql[] =
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "non_unique AS NON_UNIQUE,"
    "index_schema AS INDEX_SCHEMA,"
    "index_name AS INDEX_NAME,"
    "seq_in_index AS SEQ_IN_INDEX,"
    "column_name AS COLUMN_NAME,"
    "collation AS COLLATION,"
    "cardinality AS CARDINALITY,"
    "sub_part AS SUB_PART,"
    "packed AS PACKED,"
    "nullable AS NULLABLE,"
    "index_type AS INDEX_TYPE,"
    "comment AS COMMENT,"
    "index_comment AS INDEX_COMMENT,"
    "is_visible AS IS_VISIBLE,"
    "expression AS EXPRESSION "
    "FROM __mylite_index_catalog "
    "ORDER BY table_schema COLLATE BINARY, table_name COLLATE BINARY, "
    "index_name COLLATE BINARY, seq_in_index";
static const char information_schema_table_constraints_sql[] =
    "SELECT CONSTRAINT_CATALOG,"
    "CONSTRAINT_SCHEMA,"
    "CONSTRAINT_NAME,"
    "TABLE_SCHEMA,"
    "TABLE_NAME,"
    "CONSTRAINT_TYPE,"
    "ENFORCED "
    "FROM ("
    "SELECT 'def' AS CONSTRAINT_CATALOG,"
    "table_schema AS CONSTRAINT_SCHEMA,"
    "CASE WHEN index_name = 'PRIMARY' THEN 'PRIMARY' ELSE index_name END AS CONSTRAINT_NAME,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "CASE WHEN index_name = 'PRIMARY' THEN 'PRIMARY KEY' ELSE 'UNIQUE' END AS CONSTRAINT_TYPE,"
    "'YES' AS ENFORCED,"
    "CASE WHEN index_name = 'PRIMARY' THEN 0 ELSE 1 END AS constraint_order,"
    "MIN(rowid) AS first_rowid "
    "FROM __mylite_index_catalog "
    "WHERE non_unique = 0 "
    "GROUP BY table_schema, table_name, index_name) "
    "ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY, "
    "constraint_order, first_rowid";
static const char information_schema_key_column_usage_sql[] =
    "SELECT CONSTRAINT_CATALOG,"
    "CONSTRAINT_SCHEMA,"
    "CONSTRAINT_NAME,"
    "TABLE_CATALOG,"
    "TABLE_SCHEMA,"
    "TABLE_NAME,"
    "COLUMN_NAME,"
    "ORDINAL_POSITION,"
    "POSITION_IN_UNIQUE_CONSTRAINT,"
    "REFERENCED_TABLE_SCHEMA,"
    "REFERENCED_TABLE_NAME,"
    "REFERENCED_COLUMN_NAME "
    "FROM ("
    "SELECT 'def' AS CONSTRAINT_CATALOG,"
    "i.table_schema AS CONSTRAINT_SCHEMA,"
    "CASE WHEN i.index_name = 'PRIMARY' THEN 'PRIMARY' ELSE i.index_name END AS CONSTRAINT_NAME,"
    "'def' AS TABLE_CATALOG,"
    "i.table_schema AS TABLE_SCHEMA,"
    "i.table_name AS TABLE_NAME,"
    "i.column_name AS COLUMN_NAME,"
    "i.seq_in_index AS ORDINAL_POSITION,"
    "CAST(NULL AS INTEGER) AS POSITION_IN_UNIQUE_CONSTRAINT,"
    "CAST(NULL AS TEXT) AS REFERENCED_TABLE_SCHEMA,"
    "CAST(NULL AS TEXT) AS REFERENCED_TABLE_NAME,"
    "CAST(NULL AS TEXT) AS REFERENCED_COLUMN_NAME,"
    "CASE WHEN i.index_name = 'PRIMARY' THEN 0 ELSE 1 END AS constraint_order,"
    "logical_index.first_rowid AS first_rowid "
    "FROM __mylite_index_catalog AS i "
    "JOIN ("
    "SELECT table_schema, table_name, index_name, MIN(rowid) AS first_rowid "
    "FROM __mylite_index_catalog "
    "WHERE non_unique = 0 "
    "GROUP BY table_schema, table_name, index_name"
    ") AS logical_index "
    "ON logical_index.table_schema = i.table_schema "
    "AND logical_index.table_name = i.table_name "
    "AND logical_index.index_name = i.index_name "
    "WHERE i.non_unique = 0 AND i.column_name IS NOT NULL) "
    "ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY, "
    "constraint_order, first_rowid, ORDINAL_POSITION";
static const char information_schema_check_constraints_sql[] = "SELECT 'def' AS CONSTRAINT_CATALOG,"
                                                               "'' AS CONSTRAINT_SCHEMA,"
                                                               "'' AS CONSTRAINT_NAME,"
                                                               "'' AS CHECK_CLAUSE "
                                                               "WHERE 0";
static const char information_schema_referential_constraints_sql[] =
    "SELECT 'def' AS CONSTRAINT_CATALOG,"
    "'' AS CONSTRAINT_SCHEMA,"
    "'' AS CONSTRAINT_NAME,"
    "'def' AS UNIQUE_CONSTRAINT_CATALOG,"
    "'' AS UNIQUE_CONSTRAINT_SCHEMA,"
    "'' AS UNIQUE_CONSTRAINT_NAME,"
    "'' AS MATCH_OPTION,"
    "'' AS UPDATE_RULE,"
    "'' AS DELETE_RULE,"
    "'' AS TABLE_NAME,"
    "'' AS REFERENCED_TABLE_NAME "
    "WHERE 0";

int mylite_information_schema_prepare_select_statement(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement,
                                                       mylite_stmt **out_stmt)
{
    enum mylite_information_schema_table table = MYLITE_INFORMATION_SCHEMA_NONE;
    char *sqlite_sql = NULL;
    const char *sql = NULL;
    int status = mylite_information_schema_table_from_select(statement, &table);

    *out_stmt = NULL;
    if (status != MYLITE_OK) {
        return status;
    }
    if (table == MYLITE_INFORMATION_SCHEMA_NONE) {
        return MYLITE_UNSUPPORTED;
    }

    status = information_schema_dynamic_table_sql(database, table, &sqlite_sql);
    if (status != MYLITE_UNSUPPORTED) {
        if (status == MYLITE_OK) {
            status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
        }
        sqlite3_free(sqlite_sql);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    sql = information_schema_table_sql(table);
    if (sql == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return mylite_statement_prepare_sqlite(database, sql, out_stmt);
}

bool mylite_information_schema_has_table(const char *name)
{
    return mylite_information_schema_table_from_name(name) != MYLITE_INFORMATION_SCHEMA_NONE;
}

int mylite_information_schema_set_unknown_table_error(mylite_db *database, const char *table_name)
{
    char *display_name = mylite_copy_nonempty_cstring(table_name);
    char *message = NULL;
    int status = MYLITE_OK;

    if (display_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    mylite_uppercase_ascii_text(display_name);

    message = sqlite3_mprintf("Unknown table '%q' in information_schema", display_name);
    free(display_name);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NO_SUCH_TABLE, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int information_schema_dynamic_table_sql(mylite_db *database,
                                                enum mylite_information_schema_table table,
                                                char **out_sql)
{
    *out_sql = NULL;
    switch (table) {
    case MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS:
        return mylite_information_schema_character_sets_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_COLLATIONS:
        return mylite_information_schema_collations_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY:
        return mylite_information_schema_collation_character_set_applicability_sql(database,
                                                                                   out_sql);
    case MYLITE_INFORMATION_SCHEMA_ENGINES:
        return mylite_storage_engine_information_schema_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_KEYWORDS:
        return mylite_information_schema_keywords_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_SCHEMATA:
    case MYLITE_INFORMATION_SCHEMA_TABLES:
    case MYLITE_INFORMATION_SCHEMA_COLUMNS:
    case MYLITE_INFORMATION_SCHEMA_STATISTICS:
    case MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS:
    case MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE:
    case MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS:
    case MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS:
    case MYLITE_INFORMATION_SCHEMA_NONE:
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_UNSUPPORTED;
}

static const char *information_schema_table_sql(enum mylite_information_schema_table table)
{
    switch (table) {
    case MYLITE_INFORMATION_SCHEMA_SCHEMATA:
        return information_schema_schemata_sql;
    case MYLITE_INFORMATION_SCHEMA_TABLES:
        return information_schema_tables_sql;
    case MYLITE_INFORMATION_SCHEMA_COLUMNS:
        return information_schema_columns_sql;
    case MYLITE_INFORMATION_SCHEMA_STATISTICS:
        return information_schema_statistics_sql;
    case MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS:
        return information_schema_table_constraints_sql;
    case MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE:
        return information_schema_key_column_usage_sql;
    case MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS:
        return information_schema_check_constraints_sql;
    case MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS:
        return information_schema_referential_constraints_sql;
    case MYLITE_INFORMATION_SCHEMA_ENGINES:
    case MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS:
    case MYLITE_INFORMATION_SCHEMA_COLLATIONS:
    case MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY:
    case MYLITE_INFORMATION_SCHEMA_KEYWORDS:
    case MYLITE_INFORMATION_SCHEMA_NONE:
        return NULL;
    }

    return NULL;
}
