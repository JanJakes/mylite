#include "mylite_execution_diagnostics_internal.h"

void mylite_execution_diagnostics_set_unsupported_error(
    struct mylite_db *database,
    const char *message
) {
    if (database->cursor_plan_attempt_active) {
        database->cursor_plan_attempt_unsupported = true;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_alter_table_instant_lock_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        "Incorrect usage of ALGORITHM=INSTANT and LOCK=NONE/SHARED/EXCLUSIVE"
    );
}

void mylite_execution_diagnostics_set_alter_table_instant_algorithm_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported,
        "0A000",
        "ALGORITHM=INSTANT is not supported for this operation. Try ALGORITHM=COPY/INPLACE."
    );
}

void mylite_execution_diagnostics_set_alter_table_rebuild_instant_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "ALGORITHM=INSTANT is not supported. Reason: Need to rebuild the table to change column "
        "type. Try ALGORITHM=COPY/INPLACE."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_foreign_key_instant_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "ALGORITHM=INSTANT is not supported. Reason: Adding foreign keys needs "
        "foreign_key_checks=OFF. Try ALGORITHM=COPY/INPLACE."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_foreign_key_inplace_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "ALGORITHM=INPLACE is not supported. Reason: Adding foreign keys needs "
        "foreign_key_checks=OFF. Try ALGORITHM=COPY."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_foreign_key_lock_none_error(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        mylite_execution_diagnostics_set_alter_table_copy_lock_none_error(database);
        return;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "LOCK=NONE is not supported. Reason: Adding foreign keys needs "
        "foreign_key_checks=OFF. Try LOCK=SHARED."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_fulltext_instant_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "ALGORITHM=INSTANT is not supported. Reason: Fulltext index creation requires a lock. "
        "Try ALGORITHM=COPY/INPLACE."
    );
}

void mylite_execution_diagnostics_set_alter_table_copy_lock_none_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "LOCK=NONE is not supported. Reason: COPY algorithm requires a lock. Try LOCK=SHARED."
    );
}

void mylite_execution_diagnostics_set_alter_table_key_maintenance_lock_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported,
        "0A000",
        "LOCK=NONE/SHARED is not supported for this operation. Try LOCK=EXCLUSIVE."
    );
}

void mylite_execution_diagnostics_set_alter_table_add_fulltext_lock_none_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_algorithm_not_supported_reason,
        "0A000",
        "LOCK=NONE is not supported. Reason: Fulltext index creation requires a lock. "
        "Try LOCK=SHARED."
    );
}

void mylite_execution_diagnostics_set_nomem_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        MYLITE_NOMEM,
        "HY001",
        "out of memory"
    );
}

void mylite_execution_diagnostics_set_physical_sqlite_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        "internal SQLite schema operation failed"
    );
}

void mylite_execution_diagnostics_set_physical_sqlite_row_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        "internal SQLite row operation failed"
    );
}

void mylite_execution_diagnostics_set_runtime_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_internal_error_if_clear(
    struct mylite_db *database,
    int rc,
    const char *message
) {
    if (database == NULL) {
        return;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return;
    }
    if (rc == MYLITE_MISUSE) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_MISUSE,
            "HY000",
            mylite_diagnostics_misuse_message()
        );
        return;
    }

    mylite_execution_diagnostics_set_runtime_error(database, message);
}

int mylite_execution_diagnostics_status_from_parse_status(enum mylite_sql_parse_status status) {
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return MYLITE_OK;
    case MYLITE_SQL_PARSE_NOMEM:
        return MYLITE_NOMEM;
    case MYLITE_SQL_PARSE_MISUSE:
        return MYLITE_MISUSE;
    case MYLITE_SQL_PARSE_LEXER_ERROR:
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        return MYLITE_ERROR;
    }

    return MYLITE_ERROR;
}
