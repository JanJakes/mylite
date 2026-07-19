// clang-format off
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <php.h>
#include <Zend/zend_exceptions.h>
#include <ext/standard/info.h>
// clang-format on

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PHP_MYLITE_EXT_VERSION "0.1.0"

typedef struct php_mylite_connection {
    mylite_db *db;
    zend_long affected_rows;
    zend_string *insert_id;
    zend_object std;
} php_mylite_connection;

typedef struct php_mylite_result {
    zval rows;
    zend_ulong position;
    zend_object std;
} php_mylite_result;

typedef struct php_mylite_statement {
    zval connection;
    mylite_stmt *native;
    zval result;
    zend_object std;
} php_mylite_statement;

static zend_class_entry *php_mylite_connection_ce;
static zend_class_entry *php_mylite_result_ce;
static zend_class_entry *php_mylite_statement_ce;
static zend_class_entry *php_mylite_exception_ce;
static zend_object_handlers php_mylite_connection_handlers;
static zend_object_handlers php_mylite_result_handlers;
static zend_object_handlers php_mylite_statement_handlers;

static zend_object *php_mylite_connection_create(zend_class_entry *class_entry);
static void php_mylite_connection_free(zend_object *object);
static zend_object *php_mylite_result_create(zend_class_entry *class_entry);
static void php_mylite_result_free(zend_object *object);
static zend_object *php_mylite_statement_create(zend_class_entry *class_entry);
static void php_mylite_statement_free(zend_object *object);

static php_mylite_connection *php_mylite_connection_from_object(zend_object *object);
static php_mylite_result *php_mylite_result_from_object(zend_object *object);
static php_mylite_statement *php_mylite_statement_from_object(zend_object *object);
static mylite_db *php_mylite_require_db(php_mylite_connection *connection);
static void php_mylite_open_into_object(zval *return_value, const char *path, size_t path_len);
static bool php_mylite_execute_result(
    php_mylite_connection *connection,
    const char *sql,
    size_t sql_len,
    zval *return_value
);
static bool php_mylite_result_from_native(const mylite_result *native, zval *return_value);
static bool php_mylite_result_from_statement(
    php_mylite_connection *connection,
    mylite_stmt *native,
    zval *return_value
);
static void php_mylite_cell_to_zval(
    const mylite_result *native,
    size_t row,
    size_t column,
    zval *value
);
static void php_mylite_fetch_assoc(php_mylite_result *result, zval *return_value);
static bool php_mylite_bind_statement_value(
    php_mylite_statement *statement,
    zend_ulong index,
    zval *value
);
static bool php_mylite_bind_array_values(php_mylite_statement *statement, zval *params);
static void php_mylite_throw_db(mylite_db *db, int status, const char *fallback);
static void php_mylite_throw_open(
    int status,
    const struct mylite_open_diagnostic *diagnostic
);
static void php_mylite_update_connection_status(
    php_mylite_connection *connection,
    const mylite_result *result
);
static void php_mylite_update_connection_statement_status(
    php_mylite_connection *connection,
    const mylite_stmt *statement
);
static void php_mylite_register_constants(int module_number);

#define Z_MYLITE_CONNECTION_P(value) php_mylite_connection_from_object(Z_OBJ_P((value)))
#define Z_MYLITE_RESULT_P(value) php_mylite_result_from_object(Z_OBJ_P((value)))
#define Z_MYLITE_STATEMENT_P(value) php_mylite_statement_from_object(Z_OBJ_P((value)))

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_version, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_mylite_open, 0, 1, MyLite\\Connection, 0)
ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mylite_connection_construct, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_connection_close, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_connection_exec, 0, 1, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_mylite_connection_query, 0, 1, MyLite\\Result, 0)
ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(
    arginfo_mylite_connection_prepare,
    0,
    1,
    MyLite\\Statement,
    0
)
ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_connection_long, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_connection_string, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_statement_bind_value, 0, 2, _IS_BOOL, 0)
ZEND_ARG_TYPE_INFO(0, index, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_statement_execute, 0, 0, _IS_BOOL, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_fetch_assoc, 0, 0, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mylite_result_fetch_all, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(mylite_version) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(mylite_version());
}

PHP_FUNCTION(mylite_open) {
    char *path = NULL;
    size_t path_len = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(path, path_len)
    ZEND_PARSE_PARAMETERS_END();

    php_mylite_open_into_object(return_value, path, path_len);
}

PHP_METHOD(MyLite_Connection, __construct) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);
    struct mylite_open_diagnostic diagnostic;
    char *path = NULL;
    size_t path_len = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(path, path_len)
    ZEND_PARSE_PARAMETERS_END();

    if (connection->db != NULL) {
        zend_throw_exception(
            php_mylite_exception_ce,
            "MyLite connection is already open",
            MYLITE_MISUSE
        );
        RETURN_THROWS();
    }

    mylite_db *db = NULL;
    int status = MYLITE_OK;
    if (path_len == strlen(":memory:") && memcmp(path, ":memory:", path_len) == 0) {
        status = mylite_open_memory_with_diagnostic(&db, &diagnostic);
    } else {
        status = mylite_open_with_diagnostic(path, &db, &diagnostic);
    }
    if (status != MYLITE_OK) {
        php_mylite_throw_open(status, &diagnostic);
        RETURN_THROWS();
    }
    connection->db = db;
}

PHP_METHOD(MyLite_Connection, close) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);
    int status = MYLITE_OK;

    ZEND_PARSE_PARAMETERS_NONE();

    if (connection->db != NULL) {
        status = mylite_close_checked(connection->db);
        if (status != MYLITE_OK) {
            php_mylite_throw_db(connection->db, status, "could not close MyLite database");
            RETURN_THROWS();
        }
        connection->db = NULL;
    }
    RETURN_TRUE;
}

PHP_METHOD(MyLite_Connection, exec) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);
    char *sql = NULL;
    size_t sql_len = 0U;
    mylite_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(sql, sql_len)
    ZEND_PARSE_PARAMETERS_END();

    mylite_db *db = php_mylite_require_db(connection);
    if (db == NULL) {
        RETURN_THROWS();
    }

    const int status = mylite_execute(db, sql, sql_len, &result);
    if (status != MYLITE_OK) {
        php_mylite_throw_db(db, status, "could not execute MyLite SQL");
        RETURN_THROWS();
    }
    php_mylite_update_connection_status(connection, result);
    mylite_result_free(result);
    RETURN_LONG(connection->affected_rows);
}

PHP_METHOD(MyLite_Connection, query) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);
    char *sql = NULL;
    size_t sql_len = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(sql, sql_len)
    ZEND_PARSE_PARAMETERS_END();

    if (!php_mylite_execute_result(connection, sql, sql_len, return_value)) {
        RETURN_THROWS();
    }
}

PHP_METHOD(MyLite_Connection, prepare) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);
    char *sql = NULL;
    size_t sql_len = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(sql, sql_len)
    ZEND_PARSE_PARAMETERS_END();

    mylite_db *db = php_mylite_require_db(connection);
    if (db == NULL) {
        RETURN_THROWS();
    }

    mylite_stmt *native = NULL;
    const int status = mylite_prepare(db, sql, sql_len, &native);
    if (status != MYLITE_OK) {
        php_mylite_throw_db(db, status, "could not prepare MyLite SQL");
        RETURN_THROWS();
    }

    object_init_ex(return_value, php_mylite_statement_ce);
    php_mylite_statement *statement = Z_MYLITE_STATEMENT_P(return_value);
    ZVAL_OBJ_COPY(&statement->connection, Z_OBJ_P(ZEND_THIS));
    statement->native = native;
}

PHP_METHOD(MyLite_Connection, changes) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(connection->affected_rows);
}

PHP_METHOD(MyLite_Connection, insertId) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STR_COPY(connection->insert_id);
}

PHP_METHOD(MyLite_Connection, errorCode) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(connection->db == NULL ? MYLITE_MISUSE : mylite_errcode(connection->db));
}

PHP_METHOD(MyLite_Connection, sqlState) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(connection->db == NULL ? "HY000" : mylite_sqlstate(connection->db));
}

PHP_METHOD(MyLite_Connection, errorMessage) {
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(
        connection->db == NULL ? "MyLite connection is closed" : mylite_errmsg(connection->db)
    );
}

PHP_METHOD(MyLite_Statement, bindValue) {
    php_mylite_statement *statement = Z_MYLITE_STATEMENT_P(ZEND_THIS);
    zend_long index = 0;
    zval *value = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(index)
    Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    if (index <= 0) {
        zend_throw_exception(
            php_mylite_exception_ce,
            "parameter indexes are 1-based",
            MYLITE_MISUSE
        );
        RETURN_THROWS();
    }
    RETURN_BOOL(php_mylite_bind_statement_value(statement, (zend_ulong)index, value));
}

PHP_METHOD(MyLite_Statement, execute) {
    php_mylite_statement *statement = Z_MYLITE_STATEMENT_P(ZEND_THIS);
    zval *params = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    if (statement->native == NULL) {
        zend_throw_exception(php_mylite_exception_ce, "MyLite statement is closed", MYLITE_MISUSE);
        RETURN_THROWS();
    }

    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(&statement->connection);
    int status = mylite_stmt_reset(statement->native);
    if (status != MYLITE_OK) {
        php_mylite_throw_db(connection->db, status, "could not reset MyLite statement");
        RETURN_THROWS();
    }
    if (params != NULL) {
        status = mylite_stmt_clear_bindings(statement->native);
        if (status != MYLITE_OK) {
            php_mylite_throw_db(connection->db, status, "could not clear MyLite bindings");
            RETURN_THROWS();
        }
        if (!php_mylite_bind_array_values(statement, params)) {
            RETURN_THROWS();
        }
    }

    zval_ptr_dtor(&statement->result);
    ZVAL_UNDEF(&statement->result);
    const bool ok =
        php_mylite_result_from_statement(connection, statement->native, &statement->result);
    RETURN_BOOL(ok);
}

PHP_METHOD(MyLite_Statement, fetchAssociative) {
    php_mylite_statement *statement = Z_MYLITE_STATEMENT_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    if (Z_TYPE(statement->result) != IS_OBJECT) {
        RETURN_NULL();
    }
    php_mylite_fetch_assoc(Z_MYLITE_RESULT_P(&statement->result), return_value);
}

PHP_METHOD(MyLite_Result, fetchAssociative) {
    php_mylite_result *result = Z_MYLITE_RESULT_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    php_mylite_fetch_assoc(result, return_value);
}

PHP_METHOD(MyLite_Result, fetchAll) {
    php_mylite_result *result = Z_MYLITE_RESULT_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();
    ZVAL_COPY(return_value, &result->rows);
}

static const zend_function_entry php_mylite_functions[] = {
    PHP_FE(mylite_version, arginfo_mylite_version) PHP_FE(mylite_open, arginfo_mylite_open)
        PHP_FE_END
};

static const zend_function_entry php_mylite_connection_methods[] = {
    PHP_ME(MyLite_Connection, __construct, arginfo_mylite_connection_construct, ZEND_ACC_PUBLIC)
        PHP_ME(MyLite_Connection, close, arginfo_mylite_connection_close, ZEND_ACC_PUBLIC) PHP_ME(
            MyLite_Connection,
            exec,
            arginfo_mylite_connection_exec,
            ZEND_ACC_PUBLIC
        ) PHP_ME(MyLite_Connection, query, arginfo_mylite_connection_query, ZEND_ACC_PUBLIC)
            PHP_ME(MyLite_Connection, prepare, arginfo_mylite_connection_prepare, ZEND_ACC_PUBLIC)
                PHP_ME(MyLite_Connection, changes, arginfo_mylite_connection_long, ZEND_ACC_PUBLIC)
                    PHP_ME(
                        MyLite_Connection,
                        insertId,
                        arginfo_mylite_connection_string,
                        ZEND_ACC_PUBLIC
                    )
                        PHP_ME(
                            MyLite_Connection,
                            errorCode,
                            arginfo_mylite_connection_long,
                            ZEND_ACC_PUBLIC
                        )
                            PHP_ME(
                                MyLite_Connection,
                                sqlState,
                                arginfo_mylite_connection_string,
                                ZEND_ACC_PUBLIC
                            )
                                PHP_ME(
                                    MyLite_Connection,
                                    errorMessage,
                                    arginfo_mylite_connection_string,
                                    ZEND_ACC_PUBLIC
                                ) PHP_FE_END
};

static const zend_function_entry php_mylite_statement_methods[] = {
    PHP_ME(MyLite_Statement, bindValue, arginfo_mylite_statement_bind_value, ZEND_ACC_PUBLIC)
        PHP_ME(MyLite_Statement, execute, arginfo_mylite_statement_execute, ZEND_ACC_PUBLIC)
            PHP_ME(MyLite_Statement, fetchAssociative, arginfo_mylite_fetch_assoc, ZEND_ACC_PUBLIC)
                PHP_FE_END
};

static const zend_function_entry php_mylite_result_methods[] = {
    PHP_ME(MyLite_Result, fetchAssociative, arginfo_mylite_fetch_assoc, ZEND_ACC_PUBLIC)
        PHP_ME(MyLite_Result, fetchAll, arginfo_mylite_result_fetch_all, ZEND_ACC_PUBLIC) PHP_FE_END
};

PHP_MINIT_FUNCTION(mylite) {
    zend_class_entry class_entry;

    INIT_NS_CLASS_ENTRY(class_entry, "MyLite", "Exception", NULL);
    php_mylite_exception_ce = zend_register_internal_class_ex(&class_entry, zend_ce_exception);

    INIT_NS_CLASS_ENTRY(class_entry, "MyLite", "Connection", php_mylite_connection_methods);
    php_mylite_connection_ce = zend_register_internal_class(&class_entry);
    php_mylite_connection_ce->create_object = php_mylite_connection_create;
    memcpy(&php_mylite_connection_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    php_mylite_connection_handlers.offset = XtOffsetOf(php_mylite_connection, std);
    php_mylite_connection_handlers.free_obj = php_mylite_connection_free;
    php_mylite_connection_handlers.clone_obj = NULL;

    INIT_NS_CLASS_ENTRY(class_entry, "MyLite", "Result", php_mylite_result_methods);
    php_mylite_result_ce = zend_register_internal_class(&class_entry);
    php_mylite_result_ce->create_object = php_mylite_result_create;
    memcpy(&php_mylite_result_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    php_mylite_result_handlers.offset = XtOffsetOf(php_mylite_result, std);
    php_mylite_result_handlers.free_obj = php_mylite_result_free;
    php_mylite_result_handlers.clone_obj = NULL;

    INIT_NS_CLASS_ENTRY(class_entry, "MyLite", "Statement", php_mylite_statement_methods);
    php_mylite_statement_ce = zend_register_internal_class(&class_entry);
    php_mylite_statement_ce->create_object = php_mylite_statement_create;
    memcpy(&php_mylite_statement_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    php_mylite_statement_handlers.offset = XtOffsetOf(php_mylite_statement, std);
    php_mylite_statement_handlers.free_obj = php_mylite_statement_free;
    php_mylite_statement_handlers.clone_obj = NULL;

    php_mylite_register_constants(module_number);
    return SUCCESS;
}

PHP_MINFO_FUNCTION(mylite) {
    php_info_print_table_start();
    php_info_print_table_header(2, "MyLite support", "enabled");
    php_info_print_table_row(2, "libmylite version", mylite_version());
    php_info_print_table_end();
}

zend_module_entry mylite_module_entry = {
    STANDARD_MODULE_HEADER,
    "mylite",
    php_mylite_functions,
    PHP_MINIT(mylite),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(mylite),
    PHP_MYLITE_EXT_VERSION,
    STANDARD_MODULE_PROPERTIES,
};

#ifdef COMPILE_DL_MYLITE
ZEND_GET_MODULE(mylite)
#endif

static zend_object *php_mylite_connection_create(zend_class_entry *class_entry) {
    php_mylite_connection *connection = zend_object_alloc(sizeof(*connection), class_entry);
    zend_object_std_init(&connection->std, class_entry);
    object_properties_init(&connection->std, class_entry);
    connection->db = NULL;
    connection->affected_rows = 0;
    connection->insert_id = zend_string_init("0", 1, false);
    connection->std.handlers = &php_mylite_connection_handlers;
    return &connection->std;
}

static void php_mylite_connection_free(zend_object *object) {
    php_mylite_connection *connection = php_mylite_connection_from_object(object);
    if (connection->db != NULL) {
        mylite_close(connection->db);
        connection->db = NULL;
    }
    zend_string_release(connection->insert_id);
    zend_object_std_dtor(&connection->std);
}

static zend_object *php_mylite_result_create(zend_class_entry *class_entry) {
    php_mylite_result *result = zend_object_alloc(sizeof(*result), class_entry);
    zend_object_std_init(&result->std, class_entry);
    object_properties_init(&result->std, class_entry);
    array_init(&result->rows);
    result->position = 0;
    result->std.handlers = &php_mylite_result_handlers;
    return &result->std;
}

static void php_mylite_result_free(zend_object *object) {
    php_mylite_result *result = php_mylite_result_from_object(object);
    zval_ptr_dtor(&result->rows);
    zend_object_std_dtor(&result->std);
}

static zend_object *php_mylite_statement_create(zend_class_entry *class_entry) {
    php_mylite_statement *statement = zend_object_alloc(sizeof(*statement), class_entry);
    zend_object_std_init(&statement->std, class_entry);
    object_properties_init(&statement->std, class_entry);
    ZVAL_UNDEF(&statement->connection);
    statement->native = NULL;
    ZVAL_UNDEF(&statement->result);
    statement->std.handlers = &php_mylite_statement_handlers;
    return &statement->std;
}

static void php_mylite_statement_free(zend_object *object) {
    php_mylite_statement *statement = php_mylite_statement_from_object(object);
    if (statement->native != NULL) {
        mylite_stmt_finalize(statement->native);
        statement->native = NULL;
    }
    if (!Z_ISUNDEF(statement->connection)) {
        zval_ptr_dtor(&statement->connection);
    }
    if (!Z_ISUNDEF(statement->result)) {
        zval_ptr_dtor(&statement->result);
    }
    zend_object_std_dtor(&statement->std);
}

static php_mylite_connection *php_mylite_connection_from_object(zend_object *object) {
    return (php_mylite_connection *)((char *)object - XtOffsetOf(php_mylite_connection, std));
}

static php_mylite_result *php_mylite_result_from_object(zend_object *object) {
    return (php_mylite_result *)((char *)object - XtOffsetOf(php_mylite_result, std));
}

static php_mylite_statement *php_mylite_statement_from_object(zend_object *object) {
    return (php_mylite_statement *)((char *)object - XtOffsetOf(php_mylite_statement, std));
}

static mylite_db *php_mylite_require_db(php_mylite_connection *connection) {
    if (connection->db == NULL) {
        zend_throw_exception(php_mylite_exception_ce, "MyLite connection is closed", MYLITE_MISUSE);
        return NULL;
    }
    return connection->db;
}

static void php_mylite_open_into_object(zval *return_value, const char *path, size_t path_len) {
    struct mylite_open_diagnostic diagnostic;
    object_init_ex(return_value, php_mylite_connection_ce);
    php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(return_value);
    mylite_db *db = NULL;
    int status = MYLITE_OK;

    if (path_len == strlen(":memory:") && memcmp(path, ":memory:", path_len) == 0) {
        status = mylite_open_memory_with_diagnostic(&db, &diagnostic);
    } else {
        status = mylite_open_with_diagnostic(path, &db, &diagnostic);
    }
    if (status != MYLITE_OK) {
        zval_ptr_dtor(return_value);
        ZVAL_UNDEF(return_value);
        php_mylite_throw_open(status, &diagnostic);
        return;
    }
    connection->db = db;
}

static bool php_mylite_execute_result(
    php_mylite_connection *connection,
    const char *sql,
    size_t sql_len,
    zval *return_value
) {
    mylite_db *db = php_mylite_require_db(connection);
    if (db == NULL) {
        return false;
    }

    mylite_result *native = NULL;
    const int status = mylite_execute(db, sql, sql_len, &native);
    if (status != MYLITE_OK) {
        php_mylite_throw_db(db, status, "could not execute MyLite SQL");
        return false;
    }
    php_mylite_update_connection_status(connection, native);
    const bool ok = php_mylite_result_from_native(native, return_value);
    mylite_result_free(native);
    if (!ok) {
        zend_throw_exception(
            php_mylite_exception_ce,
            "could not allocate MyLite result",
            MYLITE_NOMEM
        );
    }
    return ok;
}

static bool php_mylite_result_from_native(const mylite_result *native, zval *return_value) {
    object_init_ex(return_value, php_mylite_result_ce);
    php_mylite_result *result = Z_MYLITE_RESULT_P(return_value);
    const size_t row_count = mylite_result_row_count(native);
    const size_t column_count = mylite_result_column_count(native);

    for (size_t row = 0U; row < row_count; ++row) {
        zval row_value;
        array_init(&row_value);
        for (size_t column = 0U; column < column_count; ++column) {
            const char *name = mylite_result_column_name(native, column);
            zval value;
            php_mylite_cell_to_zval(native, row, column, &value);
            add_assoc_zval(&row_value, name == NULL ? "" : name, &value);
        }
        add_next_index_zval(&result->rows, &row_value);
    }
    return true;
}

static bool php_mylite_result_from_statement(
    php_mylite_connection *connection,
    mylite_stmt *native,
    zval *return_value
) {
    object_init_ex(return_value, php_mylite_result_ce);
    php_mylite_result *result = Z_MYLITE_RESULT_P(return_value);
    int status = MYLITE_OK;

    while ((status = mylite_stmt_step(native)) == MYLITE_ROW) {
        zval row_value;
        const size_t column_count = mylite_stmt_column_count(native);
        array_init(&row_value);
        for (size_t column = 0U; column < column_count; ++column) {
            const char *name = mylite_stmt_column_name(native, column);
            zval value;
            if (mylite_stmt_value_is_null(native, column)) {
                ZVAL_NULL(&value);
            } else {
                const void *bytes = mylite_stmt_value_bytes(native, column);
                ZVAL_STRINGL(
                    &value,
                    bytes == NULL ? "" : (const char *)bytes,
                    mylite_stmt_value_size(native, column)
                );
            }
            add_assoc_zval(&row_value, name == NULL ? "" : name, &value);
        }
        add_next_index_zval(&result->rows, &row_value);
    }
    if (status != MYLITE_DONE) {
        zval_ptr_dtor(return_value);
        ZVAL_UNDEF(return_value);
        php_mylite_throw_db(connection->db, status, "could not execute MyLite statement");
        return false;
    }
    php_mylite_update_connection_statement_status(connection, native);
    return true;
}

static void php_mylite_cell_to_zval(
    const mylite_result *native,
    size_t row,
    size_t column,
    zval *value
) {
    if (mylite_result_value_is_null(native, row, column)) {
        ZVAL_NULL(value);
        return;
    }
    const void *bytes = mylite_result_value_bytes(native, row, column);
    ZVAL_STRINGL(
        value,
        bytes == NULL ? "" : (const char *)bytes,
        mylite_result_value_size(native, row, column)
    );
}

static void php_mylite_fetch_assoc(php_mylite_result *result, zval *return_value) {
    zval *row = zend_hash_index_find(Z_ARRVAL(result->rows), result->position);
    if (row == NULL) {
        RETURN_NULL();
    }
    ++result->position;
    ZVAL_COPY(return_value, row);
}

static bool php_mylite_bind_statement_value(
    php_mylite_statement *statement,
    zend_ulong index,
    zval *value
) {
    if (statement->native == NULL || index == 0U) {
        zend_throw_exception(php_mylite_exception_ce, "invalid parameter index", MYLITE_MISUSE);
        return false;
    }
    ZVAL_DEREF(value);
    int status = MYLITE_OK;
    switch (Z_TYPE_P(value)) {
    case IS_NULL:
        status = mylite_stmt_bind_null(statement->native, index - 1U);
        break;
    case IS_FALSE:
    case IS_TRUE:
        status = mylite_stmt_bind_int64(
            statement->native,
            index - 1U,
            Z_TYPE_P(value) == IS_TRUE ? 1 : 0
        );
        break;
    case IS_LONG:
        status = mylite_stmt_bind_int64(statement->native, index - 1U, Z_LVAL_P(value));
        break;
    case IS_DOUBLE:
        status = mylite_stmt_bind_double(statement->native, index - 1U, Z_DVAL_P(value));
        break;
    default: {
        zend_string *text = zval_get_string(value);
        if (UNEXPECTED(EG(exception) != NULL)) {
            return false;
        }
        status =
            mylite_stmt_bind_text(statement->native, index - 1U, ZSTR_VAL(text), ZSTR_LEN(text));
        zend_string_release(text);
        break;
    }
    }
    if (status != MYLITE_OK) {
        php_mylite_connection *connection = Z_MYLITE_CONNECTION_P(&statement->connection);
        php_mylite_throw_db(connection->db, status, "could not bind MyLite parameter");
        return false;
    }
    return true;
}

static bool php_mylite_bind_array_values(php_mylite_statement *statement, zval *params) {
    zval *value = NULL;
    zend_ulong index = 1U;

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(params), value) {
        if (!php_mylite_bind_statement_value(statement, index, value)) {
            return false;
        }
        ++index;
    }
    ZEND_HASH_FOREACH_END();

    return true;
}

static void php_mylite_throw_db(mylite_db *db, int status, const char *fallback) {
    const char *message = db == NULL ? fallback : mylite_errmsg(db);
    int code = db == NULL ? status : mylite_errcode(db);

    if (message == NULL || message[0] == '\0') {
        message = fallback;
    }
    if (code == MYLITE_OK) {
        code = status;
    }
    zend_throw_exception(php_mylite_exception_ce, message, code);
}

static void php_mylite_throw_open(
    int status,
    const struct mylite_open_diagnostic *diagnostic
) {
    const char *message = diagnostic == NULL ? NULL : diagnostic->message;
    int code = diagnostic == NULL ? status : diagnostic->error_code;

    if (message == NULL || message[0] == '\0') {
        message = "could not open MyLite database";
    }
    if (code == MYLITE_OK) {
        code = status;
    }
    zend_throw_exception(php_mylite_exception_ce, message, code);
}

static void php_mylite_update_connection_status(
    php_mylite_connection *connection,
    const mylite_result *result
) {
    const int64_t affected_rows = mylite_result_affected_rows(result);
    connection->affected_rows = affected_rows < 0 ? 0 : (zend_long)affected_rows;
    zend_string_release(connection->insert_id);
    connection->insert_id = zend_u64_to_str(mylite_result_insert_id(result));
}

static void php_mylite_update_connection_statement_status(
    php_mylite_connection *connection,
    const mylite_stmt *statement
) {
    const int64_t affected_rows = mylite_stmt_affected_rows(statement);
    connection->affected_rows = affected_rows < 0 ? 0 : (zend_long)affected_rows;
    zend_string_release(connection->insert_id);
    connection->insert_id = zend_u64_to_str(mylite_stmt_insert_id(statement));
}

static void php_mylite_register_constants(int module_number) {
    REGISTER_LONG_CONSTANT("MYLITE_OK", MYLITE_OK, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYLITE_ERROR", MYLITE_ERROR, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYLITE_NOMEM", MYLITE_NOMEM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYLITE_MISUSE", MYLITE_MISUSE, CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT(
        "MYLITE_RESULT_COLUMN_TYPE_LONG",
        MYLITE_RESULT_COLUMN_TYPE_LONG,
        CONST_CS | CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT(
        "MYLITE_RESULT_COLUMN_TYPE_VAR_STRING",
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        CONST_CS | CONST_PERSISTENT
    );
}
