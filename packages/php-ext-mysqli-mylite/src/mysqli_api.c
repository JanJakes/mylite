#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "mysqli_extension.h"

static bool mylite_mysqli_begin_transaction_with_flags(
    mylite_mysqli_link *link,
    zend_long flags,
    const char *name,
    size_t name_length
);
static bool mylite_mysqli_complete_transaction_with_flags(
    mylite_mysqli_link *link,
    const char *command,
    zend_long flags,
    const char *name,
    size_t name_length
);
static void mylite_mysqli_fetch_custom_object(
    mylite_mysqli_result *result,
    const char *class_name,
    size_t class_name_length,
    zval *constructor_args,
    zval *out_object
);
static bool mylite_mysqli_reject_link_feature(mylite_mysqli_link *link, const char *message);
static bool mylite_mysqli_reject_stmt_feature(mylite_mysqli_stmt *stmt, const char *message);

PHP_FUNCTION(mysqli_connect) {
    char *host = NULL;
    char *username = NULL;
    char *password = NULL;
    char *database = NULL;
    char *socket = NULL;
    size_t host_length = 0U;
    size_t username_length = 0U;
    size_t password_length = 0U;
    size_t database_length = 0U;
    size_t socket_length = 0U;
    zend_long port = 0;
    bool port_is_null = true;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 6)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING_OR_NULL(host, host_length)
    Z_PARAM_STRING_OR_NULL(username, username_length)
    Z_PARAM_STRING_OR_NULL(password, password_length)
    Z_PARAM_STRING_OR_NULL(database, database_length)
    Z_PARAM_LONG_OR_NULL(port, port_is_null)
    Z_PARAM_STRING_OR_NULL(socket, socket_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)username;
    (void)username_length;
    (void)password;
    (void)password_length;
    object_init_ex(return_value, mylite_mysqli_link_ce);
    link = mylite_mysqli_link_from_obj(Z_OBJ_P(return_value));
    if (!mylite_mysqli_connect_link(
            link,
            host,
            host_length,
            database,
            database_length,
            socket,
            socket_length,
            port,
            port_is_null,
            0
        )) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_init) {
    ZEND_PARSE_PARAMETERS_NONE();
    object_init_ex(return_value, mylite_mysqli_link_ce);
}

PHP_FUNCTION(mysqli_real_connect) {
    zval *mysql = NULL;
    char *host = NULL;
    char *username = NULL;
    char *password = NULL;
    char *database = NULL;
    char *socket = NULL;
    size_t host_length = 0U;
    size_t username_length = 0U;
    size_t password_length = 0U;
    size_t database_length = 0U;
    size_t socket_length = 0U;
    zend_long port = 0;
    zend_long flags = 0;
    bool port_is_null = true;

    ZEND_PARSE_PARAMETERS_START(1, 8)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING_OR_NULL(host, host_length)
    Z_PARAM_STRING_OR_NULL(username, username_length)
    Z_PARAM_STRING_OR_NULL(password, password_length)
    Z_PARAM_STRING_OR_NULL(database, database_length)
    Z_PARAM_LONG_OR_NULL(port, port_is_null)
    Z_PARAM_STRING_OR_NULL(socket, socket_length)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    (void)username;
    (void)username_length;
    (void)password;
    (void)password_length;
    RETURN_BOOL(mylite_mysqli_connect_link(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        host,
        host_length,
        database,
        database_length,
        socket,
        socket_length,
        port,
        port_is_null,
        flags
    ));
}

PHP_FUNCTION(mysqli_query) {
    zval *mysql = NULL;
    char *query = NULL;
    size_t query_length = 0U;
    zend_long result_mode = MYLITE_MYSQLI_STORE_RESULT;

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(query, query_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(result_mode)
    ZEND_PARSE_PARAMETERS_END();

    if (!mylite_mysqli_link_query(
            mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
            query,
            query_length,
            result_mode,
            return_value
        )) {
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_execute_query) {
    zval *mysql = NULL;
    char *query = NULL;
    size_t query_length = 0U;
    zval *params = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(query, query_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (!mylite_mysqli_link_execute_query(link, query, query_length, params, return_value)) {
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_real_query) {
    zval *mysql = NULL;
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        query,
        query_length
    ));
}

PHP_FUNCTION(mysqli_store_result) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    zval result;
    zend_long mode = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (mode != 0 && mode != MYLITE_MYSQLI_STORE_RESULT_COPY_DATA) {
        RETURN_BOOL(mylite_mysqli_reject_link_feature(link, "unknown mysqli store-result mode"));
    }
    if (mylite_mysqli_link_store_result(link, &result)) {
        ZVAL_COPY_VALUE(return_value, &result);
        return;
    }

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_use_result) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    zval result;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (mylite_mysqli_link_use_result(link, &result)) {
        ZVAL_COPY_VALUE(return_value, &result);
        return;
    }

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_close) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_close_link(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))));
}

PHP_FUNCTION(mysqli_connect_errno) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(MYLITE_MYSQLI_G(connect_errno));
}

PHP_FUNCTION(mysqli_connect_error) {
    ZEND_PARSE_PARAMETERS_NONE();
    if (MYLITE_MYSQLI_G(connect_errno) == 0) {
        RETURN_NULL();
    }
    RETURN_STRING(MYLITE_MYSQLI_G(connect_error));
}

PHP_FUNCTION(mysqli_errno) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->error_code);
}

PHP_FUNCTION(mysqli_error) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    RETURN_STR_COPY(link->error);
}

PHP_FUNCTION(mysqli_error_list) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    zval entry;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    array_init_size(return_value, link->error_code == 0 ? 0U : 1U);
    if (link->error_code == 0) {
        return;
    }

    array_init(&entry);
    add_assoc_long(&entry, "errno", link->error_code);
    add_assoc_string(&entry, "sqlstate", link->sqlstate);
    add_assoc_str(&entry, "error", zend_string_copy(link->error));
    add_next_index_zval(return_value, &entry);
}

PHP_FUNCTION(mysqli_sqlstate) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->sqlstate);
}

PHP_FUNCTION(mysqli_field_count) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->field_count);
}

PHP_FUNCTION(mysqli_affected_rows) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->affected_rows);
}

PHP_FUNCTION(mysqli_insert_id) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->insert_id);
}

PHP_FUNCTION(mysqli_info) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (link->info == NULL) {
        RETURN_NULL();
    }
    RETURN_STR_COPY(link->info);
}

PHP_FUNCTION(mysqli_warning_count) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->warning_count);
}

PHP_FUNCTION(mysqli_get_warnings) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (!mylite_mysqli_link_get_warnings(link, return_value)) {
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_get_client_info) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_OBJECT_OF_CLASS_OR_NULL(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    (void)mysql;
    RETURN_STRING("mylite mysqli " PHP_MYLITE_MYSQLI_VERSION);
}

PHP_FUNCTION(mysqli_get_client_version) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(100);
}

PHP_FUNCTION(mysqli_get_host_info) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (link->path != NULL) {
        RETURN_STR(zend_strpprintf(0, "MyLite embedded file %s", ZSTR_VAL(link->path)));
    }
    RETURN_STRING("MyLite embedded memory");
}

PHP_FUNCTION(mysqli_get_proto_info) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(10);
}

PHP_FUNCTION(mysqli_get_server_info) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(MYLITE_MYSQLI_SERVER_INFO);
}

PHP_FUNCTION(mysqli_get_server_version) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(MYLITE_MYSQLI_SERVER_VERSION);
}

PHP_FUNCTION(mysqli_character_set_name) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING("utf8mb4");
}

PHP_FUNCTION(mysqli_get_charset) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    object_init(return_value);
    add_property_string(return_value, "charset", "utf8mb4");
    add_property_string(return_value, "collation", "utf8mb4_0900_ai_ci");
    add_property_string(return_value, "dir", "");
    add_property_long(return_value, "min_length", 1);
    add_property_long(return_value, "max_length", 4);
    add_property_long(return_value, "number", 255);
    add_property_long(return_value, "state", 0);
    add_property_string(return_value, "comment", "MyLite default charset");
}

PHP_FUNCTION(mysqli_set_charset) {
    zval *mysql = NULL;
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    sql = zend_strpprintf(0, "SET NAMES %.*s", (int)value_length, value);
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_select_db) {
    zval *mysql = NULL;
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *quoted = NULL;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    quoted = mylite_mysqli_quote_identifier(value, value_length);
    sql = zend_strpprintf(0, "USE %s", ZSTR_VAL(quoted));
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_real_escape_string) {
    zval *mysql = NULL;
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *escaped = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    if (!mylite_mysqli_escape_string(
            mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
            value,
            value_length,
            &escaped
        )) {
        RETURN_FALSE;
    }
    RETURN_STR(escaped);
}

PHP_FUNCTION(mysqli_escape_string) {
    zif_mysqli_real_escape_string(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_autocommit) {
    zval *mysql = NULL;
    bool enable = false;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_autocommit(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), enable));
}

PHP_FUNCTION(mysqli_begin_transaction) {
    zval *mysql = NULL;
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_begin_transaction_with_flags(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        flags,
        name,
        name_length
    ));
}

PHP_FUNCTION(mysqli_commit) {
    zval *mysql = NULL;
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_complete_transaction_with_flags(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        "COMMIT",
        flags,
        name,
        name_length
    ));
}

PHP_FUNCTION(mysqli_rollback) {
    zval *mysql = NULL;
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_complete_transaction_with_flags(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        "ROLLBACK",
        flags,
        name,
        name_length
    ));
}

PHP_FUNCTION(mysqli_savepoint) {
    zval *mysql = NULL;
    char *name = NULL;
    size_t name_length = 0U;
    zend_string *quoted = NULL;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    quoted = mylite_mysqli_quote_identifier(name, name_length);
    sql = zend_strpprintf(0, "SAVEPOINT %s", ZSTR_VAL(quoted));
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_release_savepoint) {
    zval *mysql = NULL;
    char *name = NULL;
    size_t name_length = 0U;
    zend_string *quoted = NULL;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    quoted = mylite_mysqli_quote_identifier(name, name_length);
    sql = zend_strpprintf(0, "RELEASE SAVEPOINT %s", ZSTR_VAL(quoted));
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_change_user) {
    zval *mysql = NULL;
    char *username = NULL;
    char *password = NULL;
    char *database = NULL;
    size_t username_length = 0U;
    size_t password_length = 0U;
    size_t database_length = 0U;

    ZEND_PARSE_PARAMETERS_START(4, 4)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(username, username_length)
    Z_PARAM_STRING(password, password_length)
    Z_PARAM_STRING_OR_NULL(database, database_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)username;
    (void)username_length;
    (void)password;
    (void)password_length;
    if (database == NULL) {
        RETURN_TRUE;
    }

    zend_string *quoted = mylite_mysqli_quote_identifier(database, database_length);
    zend_string *sql = zend_strpprintf(0, "USE %s", ZSTR_VAL(quoted));
    bool ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );

    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_options) {
    zval *mysql = NULL;
    zval *value = NULL;
    zend_long option = 0;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_LONG(option)
    Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)option;
    (void)value;
    RETURN_BOOL(mylite_mysqli_reject_link_feature(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        "mysqli connection options are not supported by the embedded driver"
    ));
}

PHP_FUNCTION(mysqli_set_opt) {
    ZEND_MN(mysqli_options)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_ssl_set) {
    zval *mysql = NULL;
    char *key = NULL;
    char *certificate = NULL;
    char *ca_certificate = NULL;
    char *ca_path = NULL;
    char *cipher_algos = NULL;
    size_t key_length = 0U;
    size_t certificate_length = 0U;
    size_t ca_certificate_length = 0U;
    size_t ca_path_length = 0U;
    size_t cipher_algos_length = 0U;

    ZEND_PARSE_PARAMETERS_START(6, 6)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING_OR_NULL(key, key_length)
    Z_PARAM_STRING_OR_NULL(certificate, certificate_length)
    Z_PARAM_STRING_OR_NULL(ca_certificate, ca_certificate_length)
    Z_PARAM_STRING_OR_NULL(ca_path, ca_path_length)
    Z_PARAM_STRING_OR_NULL(cipher_algos, cipher_algos_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)key;
    (void)key_length;
    (void)certificate;
    (void)certificate_length;
    (void)ca_certificate;
    (void)ca_certificate_length;
    (void)ca_path;
    (void)ca_path_length;
    (void)cipher_algos;
    (void)cipher_algos_length;
    RETURN_BOOL(mylite_mysqli_reject_link_feature(
        mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
        "TLS configuration is not supported by the embedded driver"
    ));
}

PHP_FUNCTION(mysqli_stat) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (!mylite_mysqli_link_require_ready_without_report(link)) {
        RETURN_FALSE;
    }
    RETURN_STRING("MyLite embedded mysqli connection");
}

PHP_FUNCTION(mysqli_ping) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (!mylite_mysqli_link_require_ready(link)) {
        RETURN_FALSE;
    }
    RETURN_BOOL(link->connected);
}

PHP_FUNCTION(mysqli_kill) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    zend_long process_id = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_LONG(process_id)
    ZEND_PARSE_PARAMETERS_END();

    (void)process_id;
    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (!mylite_mysqli_link_require_ready(link)) {
        RETURN_FALSE;
    }
    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_dump_debug_info) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (!mylite_mysqli_link_require_ready_without_report(link)) {
        RETURN_FALSE;
    }
    RETURN_BOOL(mylite_mysqli_reject_link_feature(
        link,
        "server debug information is not supported by the embedded driver"
    ));
}

PHP_FUNCTION(mysqli_debug) {
    char *options = NULL;
    size_t options_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(options, options_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)options;
    (void)options_length;
    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_refresh) {
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    (void)flags;
    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (!mylite_mysqli_link_require_ready_without_report(link)) {
        RETURN_FALSE;
    }
    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_multi_query) {
    ZEND_MN(mysqli_real_query)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_more_results) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_next_result) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_reap_async_query) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_poll) {
    zval *read = NULL;
    zval *error = NULL;
    zval *reject = NULL;
    zend_long seconds = 0;
    zend_long microseconds = 0;

    ZEND_PARSE_PARAMETERS_START(4, 5)
    Z_PARAM_ZVAL(read)
    Z_PARAM_ZVAL(error)
    Z_PARAM_ARRAY(reject)
    Z_PARAM_LONG(seconds)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(microseconds)
    ZEND_PARSE_PARAMETERS_END();

    (void)read;
    (void)error;
    (void)reject;
    (void)seconds;
    (void)microseconds;
    RETURN_LONG(0);
}

PHP_FUNCTION(mysqli_thread_id) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(1);
}

PHP_FUNCTION(mysqli_thread_safe) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_get_connection_stats) {
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    array_init(return_value);
}

PHP_FUNCTION(mysqli_get_client_stats) {
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
}

PHP_FUNCTION(mysqli_get_links_stats) {
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
}

PHP_FUNCTION(mysqli_report) {
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    MYLITE_MYSQLI_G(report_mode) = (int)flags;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_prepare) {
    zval *mysql = NULL;
    char *query = NULL;
    size_t query_length = 0U;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, mylite_mysqli_stmt_ce);
    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(return_value));
    ZVAL_COPY(&stmt->link, mysql);
    if (!mylite_mysqli_stmt_prepare_internal(stmt, query, query_length)) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_stmt_init) {
    zval *mysql = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, mylite_mysqli_stmt_ce);
    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(return_value));
    ZVAL_COPY(&stmt->link, mysql);
    mylite_mysqli_update_stmt_properties(stmt);
}

PHP_FUNCTION(mysqli_fetch_assoc) {
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(
        mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
        MYLITE_MYSQLI_ASSOC,
        return_value
    );
}

PHP_FUNCTION(mysqli_fetch_row) {
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(
        mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
        MYLITE_MYSQLI_NUM,
        return_value
    );
}

PHP_FUNCTION(mysqli_fetch_array) {
    zval *result_zval = NULL;
    zend_long mode = MYLITE_MYSQLI_BOTH;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(
        mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
        (int)mode,
        return_value
    );
}

PHP_FUNCTION(mysqli_fetch_all) {
    zval *result_zval = NULL;
    zend_long mode = MYLITE_MYSQLI_NUM;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    array_init_size(return_value, result->unbuffered ? 0U : result->row_count - result->cursor);
    for (;;) {
        zval row;

        if (!result->unbuffered && result->cursor >= result->row_count) {
            break;
        }
        mylite_mysqli_result_fetch(result, (int)mode, &row);
        if (Z_TYPE(row) != IS_ARRAY) {
            zval_ptr_dtor(&row);
            break;
        }
        add_next_index_zval(return_value, &row);
    }
}

PHP_FUNCTION(mysqli_fetch_object) {
    zval *result_zval = NULL;
    char *class_name = NULL;
    size_t class_name_length = 0U;
    zval *constructor_args = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING(class_name, class_name_length)
    Z_PARAM_ARRAY(constructor_args)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_fetch_custom_object(
        mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
        class_name,
        class_name_length,
        constructor_args,
        return_value
    );
}

PHP_FUNCTION(mysqli_fetch_column) {
    zval *result_zval = NULL;
    zend_long column = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(column)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch_column(
        mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
        column,
        return_value
    );
}

PHP_FUNCTION(mysqli_fetch_field) {
    zval *result_zval = NULL;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    if (result->field_cursor >= result->column_count) {
        RETURN_FALSE;
    }
    mylite_mysqli_result_fetch_field(result, result->field_cursor, return_value);
    result->field_cursor++;
    mylite_mysqli_update_result_properties(result);
}

PHP_FUNCTION(mysqli_fetch_fields) {
    zval *result_zval = NULL;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    array_init_size(return_value, result->column_count);
    for (uint32_t index = 0; index < result->column_count; index++) {
        zval field;

        mylite_mysqli_result_fetch_field(result, index, &field);
        add_next_index_zval(return_value, &field);
    }
}

PHP_FUNCTION(mysqli_fetch_field_direct) {
    zval *result_zval = NULL;
    zend_long index = 0;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_LONG(index)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    if (index < 0 || (uint32_t)index >= result->column_count) {
        RETURN_FALSE;
    }
    mylite_mysqli_result_fetch_field(result, (uint32_t)index, return_value);
}

PHP_FUNCTION(mysqli_fetch_lengths) {
    zval *result_zval = NULL;
    mylite_mysqli_result *result = NULL;
    uint32_t row_index = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    if (result->unbuffered) {
        if (!result->current_row_valid) {
            RETURN_FALSE;
        }
    } else if (result->cursor == 0 || result->cursor > result->row_count) {
        RETURN_FALSE;
    }

    row_index = result->unbuffered ? 0U : result->cursor - 1U;
    array_init_size(return_value, result->column_count);
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval *value = &result->values[(size_t)row_index * result->column_count + column];

        add_next_index_long(
            return_value,
            Z_TYPE_P(value) == IS_STRING ? (zend_long)Z_STRLEN_P(value) : 0
        );
    }
}

PHP_FUNCTION(mysqli_data_seek) {
    zval *result_zval = NULL;
    zend_long offset = 0;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_LONG(offset)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    if (result->unbuffered) {
        RETURN_FALSE;
    }
    if (offset < 0 || (uint32_t)offset > result->row_count) {
        RETURN_FALSE;
    }
    result->cursor = (uint32_t)offset;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_field_seek) {
    zval *result_zval = NULL;
    zend_long index = 0;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_LONG(index)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    if (index < 0 || (uint32_t)index > result->column_count) {
        RETURN_FALSE;
    }
    result->field_cursor = (uint32_t)index;
    mylite_mysqli_update_result_properties(result);
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_field_tell) {
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval))->field_cursor);
}

PHP_FUNCTION(mysqli_free_result) {
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_discard(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)));
}

PHP_FUNCTION(mysqli_num_fields) {
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval))->column_count);
}

PHP_FUNCTION(mysqli_num_rows) {
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_result_num_rows(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval))));
}

PHP_FUNCTION(mysqli_stmt_prepare) {
    zval *stmt_zval = NULL;
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_prepare_internal(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval)),
        query,
        query_length
    ));
}

PHP_FUNCTION(mysqli_stmt_execute) {
    zval *stmt_zval = NULL;
    zval *params = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(
        mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval)), params)
    );
}

PHP_FUNCTION(mysqli_stmt_bind_param) {
    zval *stmt_zval = NULL;
    char *types = NULL;
    size_t types_length = 0U;
    zval *vars = NULL;
    uint32_t vars_count = 0U;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(2, -1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_STRING(types, types_length)
    Z_PARAM_VARIADIC('*', vars, vars_count)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (types_length != vars_count || vars_count != stmt->param_count) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "parameter count mismatch"
        );
        mylite_mysqli_report_stmt_error(stmt);
        RETURN_FALSE;
    }

    if (stmt->bound_params != NULL) {
        for (uint32_t index = 0; index < stmt->bound_param_count; index++) {
            zval_ptr_dtor(&stmt->bound_params[index]);
        }
        efree(stmt->bound_params);
    }
    stmt->bound_params = safe_emalloc(vars_count, sizeof(zval), 0);
    stmt->bound_param_count = vars_count;
    for (uint32_t index = 0; index < vars_count; index++) {
        ZVAL_COPY(&stmt->bound_params[index], &vars[index]);
    }
    if (stmt->types != NULL) {
        zend_string_release(stmt->types);
    }
    stmt->types = zend_string_init(types, types_length, false);
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stmt_bind_result) {
    zval *stmt_zval = NULL;
    zval *vars = NULL;
    uint32_t vars_count = 0U;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, -1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_VARIADIC('*', vars, vars_count)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (stmt->bound_results != NULL) {
        for (uint32_t index = 0; index < stmt->bound_result_count; index++) {
            zval_ptr_dtor(&stmt->bound_results[index]);
        }
        efree(stmt->bound_results);
    }
    stmt->bound_results = safe_emalloc(vars_count, sizeof(zval), 0);
    stmt->bound_result_count = vars_count;
    for (uint32_t index = 0; index < vars_count; index++) {
        ZVAL_COPY(&stmt->bound_results[index], &vars[index]);
    }
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stmt_fetch) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;
    int status = MYLITE_ERROR;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    status = mylite_mysqli_stmt_fetch_internal(stmt);
    if (status == MYLITE_ROW) {
        RETURN_TRUE;
    }
    if (status == MYLITE_DONE) {
        RETURN_NULL();
    }
    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_stmt_get_result) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (!mylite_mysqli_stmt_get_result_internal(stmt, return_value)) {
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_stmt_result_metadata) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (!mylite_mysqli_stmt_result_metadata_internal(stmt, return_value)) {
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_stmt_store_result) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    RETURN_BOOL(mylite_mysqli_stmt_store_result_internal(stmt));
}

PHP_FUNCTION(mysqli_stmt_free_result) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    mylite_mysqli_stmt_free_result_internal(stmt);
}

PHP_FUNCTION(mysqli_stmt_close) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_close_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))));
}

PHP_FUNCTION(mysqli_stmt_data_seek) {
    zval *stmt_zval = NULL;
    zend_long offset = 0;
    mylite_mysqli_stmt *stmt = NULL;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_LONG(offset)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (Z_TYPE(stmt->result) == IS_OBJECT) {
        result = mylite_mysqli_result_from_obj(Z_OBJ(stmt->result));
        if (!result->unbuffered && offset >= 0 && (uint32_t)offset <= result->row_count) {
            result->cursor = (uint32_t)offset;
        }
    }
}

PHP_FUNCTION(mysqli_stmt_reset) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_reset_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))));
}

PHP_FUNCTION(mysqli_stmt_attr_get) {
    zval *stmt_zval = NULL;
    zend_long attribute = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_LONG(attribute)
    ZEND_PARSE_PARAMETERS_END();

    (void)attribute;
    RETURN_BOOL(mylite_mysqli_reject_stmt_feature(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval)),
        "mysqli statement attributes are not supported by the embedded driver"
    ));
}

PHP_FUNCTION(mysqli_stmt_attr_set) {
    zval *stmt_zval = NULL;
    zend_long attribute = 0;
    zend_long value = 0;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_LONG(attribute)
    Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)attribute;
    (void)value;
    RETURN_BOOL(mylite_mysqli_reject_stmt_feature(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval)),
        "mysqli statement attributes are not supported by the embedded driver"
    ));
}

PHP_FUNCTION(mysqli_stmt_send_long_data) {
    zval *stmt_zval = NULL;
    char *data = NULL;
    size_t data_length = 0U;
    zend_long param_num = 0;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_LONG(param_num)
    Z_PARAM_STRING(data, data_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_send_long_data_internal(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval)),
        param_num,
        data,
        data_length
    ));
}

PHP_FUNCTION(mysqli_stmt_errno) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->error_code);
}

PHP_FUNCTION(mysqli_stmt_error) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    RETURN_STR_COPY(stmt->error);
}

PHP_FUNCTION(mysqli_stmt_error_list) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;
    zval entry;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    array_init_size(return_value, stmt->error_code == 0 ? 0U : 1U);
    if (stmt->error_code == 0) {
        return;
    }

    array_init(&entry);
    add_assoc_long(&entry, "errno", stmt->error_code);
    add_assoc_string(&entry, "sqlstate", stmt->sqlstate);
    add_assoc_str(&entry, "error", zend_string_copy(stmt->error));
    add_next_index_zval(return_value, &entry);
}

PHP_FUNCTION(mysqli_stmt_sqlstate) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->sqlstate);
}

PHP_FUNCTION(mysqli_stmt_field_count) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->field_count);
}

PHP_FUNCTION(mysqli_stmt_affected_rows) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->affected_rows);
}

PHP_FUNCTION(mysqli_stmt_insert_id) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->insert_id);
}

PHP_FUNCTION(mysqli_stmt_num_rows) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->num_rows);
}

PHP_FUNCTION(mysqli_stmt_param_count) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->param_count);
}

PHP_FUNCTION(mysqli_stmt_get_warnings) {
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (!mylite_mysqli_stmt_get_warnings(stmt, return_value)) {
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_stmt_more_results) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_stmt_next_result) {
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_METHOD(mysqli, __construct) {
    if (ZEND_NUM_ARGS() > 0) {
        zval *object = getThis();
        char *host = NULL;
        char *username = NULL;
        char *password = NULL;
        char *database = NULL;
        char *socket = NULL;
        size_t host_length = 0U;
        size_t username_length = 0U;
        size_t password_length = 0U;
        size_t database_length = 0U;
        size_t socket_length = 0U;
        zend_long port = 0;
        bool port_is_null = true;

        if (object == NULL) {
            return;
        }

        ZEND_PARSE_PARAMETERS_START(0, 6)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING_OR_NULL(host, host_length)
        Z_PARAM_STRING_OR_NULL(username, username_length)
        Z_PARAM_STRING_OR_NULL(password, password_length)
        Z_PARAM_STRING_OR_NULL(database, database_length)
        Z_PARAM_LONG_OR_NULL(port, port_is_null)
        Z_PARAM_STRING_OR_NULL(socket, socket_length)
        ZEND_PARSE_PARAMETERS_END();

        (void)username;
        (void)username_length;
        (void)password;
        (void)password_length;
        (void)mylite_mysqli_connect_link(
            mylite_mysqli_link_from_obj(Z_OBJ_P(object)),
            host,
            host_length,
            database,
            database_length,
            socket,
            socket_length,
            port,
            port_is_null,
            0
        );
    }
}

PHP_METHOD(mysqli, connect) {
    zval *object = getThis();
    char *host = NULL;
    char *username = NULL;
    char *password = NULL;
    char *database = NULL;
    char *socket = NULL;
    size_t host_length = 0U;
    size_t username_length = 0U;
    size_t password_length = 0U;
    size_t database_length = 0U;
    size_t socket_length = 0U;
    zend_long port = 0;
    bool port_is_null = true;

    if (object == NULL) {
        RETURN_FALSE;
    }

    ZEND_PARSE_PARAMETERS_START(0, 6)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING_OR_NULL(host, host_length)
    Z_PARAM_STRING_OR_NULL(username, username_length)
    Z_PARAM_STRING_OR_NULL(password, password_length)
    Z_PARAM_STRING_OR_NULL(database, database_length)
    Z_PARAM_LONG_OR_NULL(port, port_is_null)
    Z_PARAM_STRING_OR_NULL(socket, socket_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)username;
    (void)username_length;
    (void)password;
    (void)password_length;
    RETURN_BOOL(mylite_mysqli_connect_link(
        mylite_mysqli_link_from_obj(Z_OBJ_P(object)),
        host,
        host_length,
        database,
        database_length,
        socket,
        socket_length,
        port,
        port_is_null,
        0
    ));
}

PHP_METHOD(mysqli, real_connect) {
    zval *object = getThis();
    char *host = NULL;
    char *username = NULL;
    char *password = NULL;
    char *database = NULL;
    char *socket = NULL;
    size_t host_length = 0U;
    size_t username_length = 0U;
    size_t password_length = 0U;
    size_t database_length = 0U;
    size_t socket_length = 0U;
    zend_long port = 0;
    zend_long flags = 0;
    bool port_is_null = true;

    if (object == NULL) {
        RETURN_FALSE;
    }

    ZEND_PARSE_PARAMETERS_START(0, 7)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING_OR_NULL(host, host_length)
    Z_PARAM_STRING_OR_NULL(username, username_length)
    Z_PARAM_STRING_OR_NULL(password, password_length)
    Z_PARAM_STRING_OR_NULL(database, database_length)
    Z_PARAM_LONG_OR_NULL(port, port_is_null)
    Z_PARAM_STRING_OR_NULL(socket, socket_length)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    (void)username;
    (void)username_length;
    (void)password;
    (void)password_length;
    RETURN_BOOL(mylite_mysqli_connect_link(
        mylite_mysqli_link_from_obj(Z_OBJ_P(object)),
        host,
        host_length,
        database,
        database_length,
        socket,
        socket_length,
        port,
        port_is_null,
        flags
    ));
}

PHP_METHOD(mysqli, query) {
    char *query = NULL;
    size_t query_length = 0U;
    zend_long result_mode = MYLITE_MYSQLI_STORE_RESULT;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(query, query_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(result_mode)
    ZEND_PARSE_PARAMETERS_END();

    if (!mylite_mysqli_link_query(
            mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
            query,
            query_length,
            result_mode,
            return_value
        )) {
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli, execute_query) {
    char *query = NULL;
    size_t query_length = 0U;
    zval *params = NULL;
    zval *object = getThis();

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(query, query_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    if (object == NULL) {
        RETURN_FALSE;
    }
    if (!mylite_mysqli_link_execute_query(
            mylite_mysqli_link_from_obj(Z_OBJ_P(object)),
            query,
            query_length,
            params,
            return_value
        )) {
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli, real_query) {
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        query,
        query_length
    ));
}

PHP_METHOD(mysqli, store_result) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));
    zval result;
    zend_long mode = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    if (mode != 0 && mode != MYLITE_MYSQLI_STORE_RESULT_COPY_DATA) {
        RETURN_BOOL(mylite_mysqli_reject_link_feature(link, "unknown mysqli store-result mode"));
    }
    if (mylite_mysqli_link_store_result(link, &result)) {
        ZVAL_COPY_VALUE(return_value, &result);
        return;
    }
    RETURN_FALSE;
}

PHP_METHOD(mysqli, use_result) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));
    zval result;

    ZEND_PARSE_PARAMETERS_NONE();
    if (mylite_mysqli_link_use_result(link, &result)) {
        ZVAL_COPY_VALUE(return_value, &result);
        return;
    }
    RETURN_FALSE;
}

PHP_METHOD(mysqli, close) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(mylite_mysqli_close_link(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()))));
}

PHP_METHOD(mysqli, prepare) {
    char *query = NULL;
    size_t query_length = 0U;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, mylite_mysqli_stmt_ce);
    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(return_value));
    ZVAL_OBJ(&stmt->link, Z_OBJ_P(getThis()));
    Z_ADDREF(stmt->link);
    if (!mylite_mysqli_stmt_prepare_internal(stmt, query, query_length)) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli, stmt_init) {
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_NONE();
    object_init_ex(return_value, mylite_mysqli_stmt_ce);
    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(return_value));
    ZVAL_OBJ(&stmt->link, Z_OBJ_P(getThis()));
    Z_ADDREF(stmt->link);
    mylite_mysqli_update_stmt_properties(stmt);
}

PHP_METHOD(mysqli, autocommit) {
    bool enable = false;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(
        mylite_mysqli_link_autocommit(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())), enable)
    );
}

PHP_METHOD(mysqli, begin_transaction) {
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_begin_transaction_with_flags(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        flags,
        name,
        name_length
    ));
}

PHP_METHOD(mysqli, commit) {
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_complete_transaction_with_flags(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        "COMMIT",
        flags,
        name,
        name_length
    ));
}

PHP_METHOD(mysqli, rollback) {
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_complete_transaction_with_flags(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        "ROLLBACK",
        flags,
        name,
        name_length
    ));
}

PHP_METHOD(mysqli, savepoint) {
    char *name = NULL;
    size_t name_length = 0U;
    zend_string *quoted = NULL;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    quoted = mylite_mysqli_quote_identifier(name, name_length);
    sql = zend_strpprintf(0, "SAVEPOINT %s", ZSTR_VAL(quoted));
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, release_savepoint) {
    char *name = NULL;
    size_t name_length = 0U;
    zend_string *quoted = NULL;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    quoted = mylite_mysqli_quote_identifier(name, name_length);
    sql = zend_strpprintf(0, "RELEASE SAVEPOINT %s", ZSTR_VAL(quoted));
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, change_user) {
    zval *object = getThis();
    char *username = NULL;
    char *password = NULL;
    char *database = NULL;
    size_t username_length = 0U;
    size_t password_length = 0U;
    size_t database_length = 0U;

    if (object == NULL) {
        RETURN_FALSE;
    }

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_STRING(username, username_length)
    Z_PARAM_STRING(password, password_length)
    Z_PARAM_STRING_OR_NULL(database, database_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)username;
    (void)username_length;
    (void)password;
    (void)password_length;
    if (database == NULL) {
        RETURN_TRUE;
    }

    zend_string *quoted = mylite_mysqli_quote_identifier(database, database_length);
    zend_string *sql = zend_strpprintf(0, "USE %s", ZSTR_VAL(quoted));
    bool ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(object)),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );

    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, character_set_name) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING("utf8mb4");
}

PHP_METHOD(mysqli, get_charset) {
    ZEND_PARSE_PARAMETERS_NONE();
    object_init(return_value);
    add_property_string(return_value, "charset", "utf8mb4");
    add_property_string(return_value, "collation", "utf8mb4_0900_ai_ci");
    add_property_long(return_value, "number", 255);
}

PHP_METHOD(mysqli, get_client_info) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING("mylite mysqli " PHP_MYLITE_MYSQLI_VERSION);
}

PHP_METHOD(mysqli, get_connection_stats) {
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
}

PHP_METHOD(mysqli, get_server_info) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(MYLITE_MYSQLI_SERVER_INFO);
}

PHP_METHOD(mysqli, get_warnings) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!mylite_mysqli_link_get_warnings(link, return_value)) {
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli, init) {
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(mysqli, kill) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));
    zend_long process_id = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(process_id)
    ZEND_PARSE_PARAMETERS_END();

    (void)process_id;
    if (!mylite_mysqli_link_require_ready(link)) {
        RETURN_FALSE;
    }
    RETURN_FALSE;
}

PHP_METHOD(mysqli, multi_query) {
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        query,
        query_length
    ));
}

PHP_METHOD(mysqli, more_results) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli, next_result) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli, ping) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!mylite_mysqli_link_require_ready(link)) {
        RETURN_FALSE;
    }
    RETURN_BOOL(link->connected);
}

PHP_METHOD(mysqli, poll) {
    zif_mysqli_poll(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(mysqli, real_escape_string) {
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *escaped = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    if (!mylite_mysqli_escape_string(
            mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
            value,
            value_length,
            &escaped
        )) {
        RETURN_FALSE;
    }
    RETURN_STR(escaped);
}

PHP_METHOD(mysqli, reap_async_query) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli, escape_string) {
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *escaped = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    if (!mylite_mysqli_escape_string(
            mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
            value,
            value_length,
            &escaped
        )) {
        RETURN_FALSE;
    }
    RETURN_STR(escaped);
}

PHP_METHOD(mysqli, select_db) {
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *quoted = NULL;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    quoted = mylite_mysqli_quote_identifier(value, value_length);
    sql = zend_strpprintf(0, "USE %s", ZSTR_VAL(quoted));
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, set_charset) {
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    sql = zend_strpprintf(0, "SET NAMES %.*s", (int)value_length, value);
    ok = mylite_mysqli_link_real_query(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        ZSTR_VAL(sql),
        ZSTR_LEN(sql)
    );
    zend_string_release(sql);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, options) {
    zend_long option = 0;
    zval *value = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(option)
    Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)option;
    (void)value;
    RETURN_BOOL(mylite_mysqli_reject_link_feature(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        "mysqli connection options are not supported by the embedded driver"
    ));
}

PHP_METHOD(mysqli, set_opt) {
    zend_long option = 0;
    zval *value = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(option)
    Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)option;
    (void)value;
    RETURN_BOOL(mylite_mysqli_reject_link_feature(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        "mysqli connection options are not supported by the embedded driver"
    ));
}

PHP_METHOD(mysqli, ssl_set) {
    char *key = NULL;
    char *certificate = NULL;
    char *ca_certificate = NULL;
    char *ca_path = NULL;
    char *cipher_algos = NULL;
    size_t key_length = 0U;
    size_t certificate_length = 0U;
    size_t ca_certificate_length = 0U;
    size_t ca_path_length = 0U;
    size_t cipher_algos_length = 0U;

    ZEND_PARSE_PARAMETERS_START(5, 5)
    Z_PARAM_STRING_OR_NULL(key, key_length)
    Z_PARAM_STRING_OR_NULL(certificate, certificate_length)
    Z_PARAM_STRING_OR_NULL(ca_certificate, ca_certificate_length)
    Z_PARAM_STRING_OR_NULL(ca_path, ca_path_length)
    Z_PARAM_STRING_OR_NULL(cipher_algos, cipher_algos_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)key;
    (void)key_length;
    (void)certificate;
    (void)certificate_length;
    (void)ca_certificate;
    (void)ca_certificate_length;
    (void)ca_path;
    (void)ca_path_length;
    (void)cipher_algos;
    (void)cipher_algos_length;
    RETURN_BOOL(mylite_mysqli_reject_link_feature(
        mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
        "TLS configuration is not supported by the embedded driver"
    ));
}

PHP_METHOD(mysqli, stat) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!mylite_mysqli_link_require_ready_without_report(link)) {
        RETURN_FALSE;
    }
    RETURN_STRING("MyLite embedded mysqli connection");
}

PHP_METHOD(mysqli, thread_safe) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_METHOD(mysqli, dump_debug_info) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!mylite_mysqli_link_require_ready_without_report(link)) {
        RETURN_FALSE;
    }
    RETURN_BOOL(mylite_mysqli_reject_link_feature(
        link,
        "server debug information is not supported by the embedded driver"
    ));
}

PHP_METHOD(mysqli, debug) {
    char *options = NULL;
    size_t options_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(options, options_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)options;
    (void)options_length;
    RETURN_FALSE;
}

PHP_METHOD(mysqli, refresh) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    (void)flags;
    if (!mylite_mysqli_link_require_ready_without_report(link)) {
        RETURN_FALSE;
    }
    RETURN_FALSE;
}

PHP_METHOD(mysqli_result, __construct) {
    zend_throw_error(NULL, "mysqli_result objects are created by query execution");
}

PHP_METHOD(mysqli_result, close) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_result_discard(result);
}

PHP_METHOD(mysqli_result, free) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_result_discard(result);
}

PHP_METHOD(mysqli_result, free_result) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_result_discard(result);
}

PHP_METHOD(mysqli_result, data_seek) {
    zend_long offset = 0;
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(offset)
    ZEND_PARSE_PARAMETERS_END();

    if (result->unbuffered) {
        RETURN_FALSE;
    }
    if (offset < 0 || (uint32_t)offset > result->row_count) {
        RETURN_FALSE;
    }
    result->cursor = (uint32_t)offset;
    RETURN_TRUE;
}

PHP_METHOD(mysqli_result, fetch_assoc) {
    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_result_fetch(
        mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())),
        MYLITE_MYSQLI_ASSOC,
        return_value
    );
}

PHP_METHOD(mysqli_result, fetch_row) {
    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_result_fetch(
        mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())),
        MYLITE_MYSQLI_NUM,
        return_value
    );
}

PHP_METHOD(mysqli_result, fetch_array) {
    zend_long mode = MYLITE_MYSQLI_BOTH;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(
        mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())),
        (int)mode,
        return_value
    );
}

PHP_METHOD(mysqli_result, fetch_all) {
    zend_long mode = MYLITE_MYSQLI_NUM;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));
    array_init_size(return_value, result->unbuffered ? 0U : result->row_count - result->cursor);
    for (;;) {
        zval row;

        if (!result->unbuffered && result->cursor >= result->row_count) {
            break;
        }
        mylite_mysqli_result_fetch(result, (int)mode, &row);
        if (Z_TYPE(row) != IS_ARRAY) {
            zval_ptr_dtor(&row);
            break;
        }
        add_next_index_zval(return_value, &row);
    }
}

PHP_METHOD(mysqli_result, fetch_object) {
    char *class_name = NULL;
    size_t class_name_length = 0U;
    zval *constructor_args = NULL;
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(ZEND_THIS));

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING(class_name, class_name_length)
    Z_PARAM_ARRAY(constructor_args)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_fetch_custom_object(
        result,
        class_name,
        class_name_length,
        constructor_args,
        return_value
    );
}

PHP_METHOD(mysqli_result, fetch_column) {
    zend_long column = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(column)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch_column(
        mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())),
        column,
        return_value
    );
}

PHP_METHOD(mysqli_result, fetch_field) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (result->field_cursor >= result->column_count) {
        RETURN_FALSE;
    }
    mylite_mysqli_result_fetch_field(result, result->field_cursor, return_value);
    result->field_cursor++;
    mylite_mysqli_update_result_properties(result);
}

PHP_METHOD(mysqli_result, fetch_fields) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    array_init_size(return_value, result->column_count);
    for (uint32_t index = 0; index < result->column_count; index++) {
        zval field;

        mylite_mysqli_result_fetch_field(result, index, &field);
        add_next_index_zval(return_value, &field);
    }
}

PHP_METHOD(mysqli_result, fetch_field_direct) {
    zend_long index = 0;
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(index)
    ZEND_PARSE_PARAMETERS_END();

    if (index < 0 || (uint32_t)index >= result->column_count) {
        RETURN_FALSE;
    }
    mylite_mysqli_result_fetch_field(result, (uint32_t)index, return_value);
}

PHP_METHOD(mysqli_result, field_seek) {
    zend_long index = 0;
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(index)
    ZEND_PARSE_PARAMETERS_END();

    if (index < 0 || (uint32_t)index > result->column_count) {
        RETURN_FALSE;
    }
    result->field_cursor = (uint32_t)index;
    mylite_mysqli_update_result_properties(result);
    RETURN_TRUE;
}

PHP_METHOD(mysqli_result, getIterator) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!result->unbuffered) {
        result->cursor = 0;
    }
    array_init_size(return_value, result->unbuffered ? 0U : result->row_count);
    for (;;) {
        zval row;

        if (!result->unbuffered && result->cursor >= result->row_count) {
            break;
        }
        mylite_mysqli_result_fetch(result, MYLITE_MYSQLI_ASSOC, &row);
        if (Z_TYPE(row) != IS_ARRAY) {
            zval_ptr_dtor(&row);
            break;
        }
        add_next_index_zval(return_value, &row);
    }
    zend_create_internal_iterator_zval(return_value, return_value);
}

PHP_METHOD(mysqli_stmt, __construct) {
    zval *mysql = NULL;
    char *query = NULL;
    size_t query_length = 0U;
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING_OR_NULL(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    ZVAL_COPY(&stmt->link, mysql);
    if (query != NULL) {
        (void)mylite_mysqli_stmt_prepare_internal(stmt, query, query_length);
    }
}

PHP_METHOD(mysqli_stmt, prepare) {
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_prepare_internal(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis())),
        query,
        query_length
    ));
}

PHP_METHOD(mysqli_stmt, execute) {
    zval *params = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(
        mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis())), params)
    );
}

PHP_METHOD(mysqli_stmt, bind_param) {
    char *types = NULL;
    size_t types_length = 0U;
    zval *vars = NULL;
    uint32_t vars_count = 0U;
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, -1)
    Z_PARAM_STRING(types, types_length)
    Z_PARAM_VARIADIC('*', vars, vars_count)
    ZEND_PARSE_PARAMETERS_END();

    if (types_length != vars_count || vars_count != stmt->param_count) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "parameter count mismatch"
        );
        mylite_mysqli_report_stmt_error(stmt);
        RETURN_FALSE;
    }
    if (stmt->bound_params != NULL) {
        for (uint32_t index = 0; index < stmt->bound_param_count; index++) {
            zval_ptr_dtor(&stmt->bound_params[index]);
        }
        efree(stmt->bound_params);
    }
    stmt->bound_params = safe_emalloc(vars_count, sizeof(zval), 0);
    stmt->bound_param_count = vars_count;
    for (uint32_t index = 0; index < vars_count; index++) {
        ZVAL_COPY(&stmt->bound_params[index], &vars[index]);
    }
    if (stmt->types != NULL) {
        zend_string_release(stmt->types);
    }
    stmt->types = zend_string_init(types, types_length, false);
    RETURN_TRUE;
}

PHP_METHOD(mysqli_stmt, bind_result) {
    zval *vars = NULL;
    uint32_t vars_count = 0U;
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(0, -1)
    Z_PARAM_VARIADIC('*', vars, vars_count)
    ZEND_PARSE_PARAMETERS_END();

    if (stmt->bound_results != NULL) {
        for (uint32_t index = 0; index < stmt->bound_result_count; index++) {
            zval_ptr_dtor(&stmt->bound_results[index]);
        }
        efree(stmt->bound_results);
    }
    stmt->bound_results = safe_emalloc(vars_count, sizeof(zval), 0);
    stmt->bound_result_count = vars_count;
    for (uint32_t index = 0; index < vars_count; index++) {
        ZVAL_COPY(&stmt->bound_results[index], &vars[index]);
    }
    RETURN_TRUE;
}

PHP_METHOD(mysqli_stmt, fetch) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));
    int status = MYLITE_ERROR;

    ZEND_PARSE_PARAMETERS_NONE();
    status = mylite_mysqli_stmt_fetch_internal(stmt);
    if (status == MYLITE_ROW) {
        RETURN_TRUE;
    }
    if (status == MYLITE_DONE) {
        RETURN_NULL();
    }
    RETURN_FALSE;
}

PHP_METHOD(mysqli_stmt, get_result) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!mylite_mysqli_stmt_get_result_internal(stmt, return_value)) {
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli_stmt, result_metadata) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!mylite_mysqli_stmt_result_metadata_internal(stmt, return_value)) {
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli_stmt, store_result) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(mylite_mysqli_stmt_store_result_internal(stmt));
}

PHP_METHOD(mysqli_stmt, free_result) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_stmt_free_result_internal(stmt);
}

PHP_METHOD(mysqli_stmt, close) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(mylite_mysqli_stmt_close_internal(stmt));
}

PHP_METHOD(mysqli_stmt, data_seek) {
    zend_long offset = 0;
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(offset)
    ZEND_PARSE_PARAMETERS_END();

    if (Z_TYPE(stmt->result) == IS_OBJECT) {
        mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ(stmt->result));

        if (!result->unbuffered && offset >= 0 && (uint32_t)offset <= result->row_count) {
            result->cursor = (uint32_t)offset;
        }
    }
}

PHP_METHOD(mysqli_stmt, reset) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(mylite_mysqli_stmt_reset_internal(stmt));
}

PHP_METHOD(mysqli_stmt, attr_get) {
    zend_long attribute = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(attribute)
    ZEND_PARSE_PARAMETERS_END();

    (void)attribute;
    RETURN_BOOL(mylite_mysqli_reject_stmt_feature(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis())),
        "mysqli statement attributes are not supported by the embedded driver"
    ));
}

PHP_METHOD(mysqli_stmt, attr_set) {
    zend_long attribute = 0;
    zend_long value = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(attribute)
    Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)attribute;
    (void)value;
    RETURN_BOOL(mylite_mysqli_reject_stmt_feature(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis())),
        "mysqli statement attributes are not supported by the embedded driver"
    ));
}

PHP_METHOD(mysqli_stmt, send_long_data) {
    zend_long param_num = 0;
    char *data = NULL;
    size_t data_length = 0U;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(param_num)
    Z_PARAM_STRING(data, data_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_send_long_data_internal(
        mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis())),
        param_num,
        data,
        data_length
    ));
}

PHP_METHOD(mysqli_stmt, get_warnings) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!mylite_mysqli_stmt_get_warnings(stmt, return_value)) {
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli_stmt, more_results) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli_stmt, next_result) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli_stmt, num_rows) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()))->num_rows);
}

static void mylite_mysqli_fetch_custom_object(
    mylite_mysqli_result *result,
    const char *class_name,
    size_t class_name_length,
    zval *constructor_args,
    zval *out_object
) {
    zend_class_entry *class_entry = NULL;
    zval row;

    if (class_name == NULL) {
        mylite_mysqli_result_fetch_object(result, out_object);
        return;
    }
    mylite_mysqli_result_fetch(result, MYLITE_MYSQLI_ASSOC, &row);
    if (Z_TYPE(row) != IS_ARRAY) {
        ZVAL_COPY_VALUE(out_object, &row);
        return;
    }

    {
        zend_string *lookup_name = zend_string_init(class_name, class_name_length, false);

        class_entry = zend_lookup_class(lookup_name);
        zend_string_release(lookup_name);
    }
    if (class_entry == NULL) {
        zval_ptr_dtor(&row);
        ZVAL_FALSE(out_object);
        return;
    }
    object_init_ex(out_object, class_entry);
    if (EG(exception) != NULL) {
        zval_ptr_dtor(&row);
        return;
    }
    {
        zend_string *property_name = NULL;
        zval *property_value = NULL;

        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL(row), property_name, property_value) {
            if (property_name != NULL) {
                zend_update_property(
                    class_entry,
                    Z_OBJ_P(out_object),
                    ZSTR_VAL(property_name),
                    ZSTR_LEN(property_name),
                    property_value
                );
                if (EG(exception) != NULL) {
                    break;
                }
            }
        }
        ZEND_HASH_FOREACH_END();
    }
    zval_ptr_dtor(&row);
    if (EG(exception) != NULL) {
        return;
    }

    if (class_entry->constructor == NULL) {
        if (constructor_args != NULL &&
            zend_hash_num_elements(Z_ARRVAL_P(constructor_args)) != 0U) {
            zend_throw_error(
                NULL,
                "Class %s does not have a constructor",
                ZSTR_VAL(class_entry->name)
            );
        }
        return;
    }

    {
        zval constructor_result;

        ZVAL_UNDEF(&constructor_result);
        zend_call_known_function(
            class_entry->constructor,
            Z_OBJ_P(out_object),
            class_entry,
            &constructor_result,
            0U,
            NULL,
            constructor_args == NULL ? NULL : Z_ARRVAL_P(constructor_args)
        );
        if (!Z_ISUNDEF(constructor_result)) {
            zval_ptr_dtor(&constructor_result);
        }
    }
}

static bool mylite_mysqli_begin_transaction_with_flags(
    mylite_mysqli_link *link,
    zend_long flags,
    const char *name,
    size_t name_length
) {
    const zend_long allowed_flags = MYLITE_MYSQLI_TRANS_START_WITH_CONSISTENT_SNAPSHOT |
                                    MYLITE_MYSQLI_TRANS_START_READ_WRITE |
                                    MYLITE_MYSQLI_TRANS_START_READ_ONLY;
    const bool read_write = (flags & MYLITE_MYSQLI_TRANS_START_READ_WRITE) != 0;
    const bool read_only = (flags & MYLITE_MYSQLI_TRANS_START_READ_ONLY) != 0;
    bool has_characteristic = false;
    bool ok = false;
    smart_str sql = {0};

    if (flags < 0 || (flags & ~allowed_flags) != 0 || (read_write && read_only)) {
        return mylite_mysqli_reject_link_feature(
            link,
            "unsupported or conflicting mysqli transaction start flags"
        );
    }
    if (name != NULL && name_length != 0U) {
        return mylite_mysqli_reject_link_feature(
            link,
            "named mysqli transactions are not supported by the embedded driver"
        );
    }

    smart_str_appends(&sql, "START TRANSACTION");
    if ((flags & MYLITE_MYSQLI_TRANS_START_WITH_CONSISTENT_SNAPSHOT) != 0) {
        smart_str_appends(&sql, " WITH CONSISTENT SNAPSHOT");
        has_characteristic = true;
    }
    if (read_write || read_only) {
        smart_str_appends(&sql, has_characteristic ? ", " : " ");
        smart_str_appends(&sql, read_only ? "READ ONLY" : "READ WRITE");
    }
    smart_str_0(&sql);
    ok = mylite_mysqli_link_real_query(link, ZSTR_VAL(sql.s), ZSTR_LEN(sql.s));
    zend_string_release(sql.s);
    return ok;
}

static bool mylite_mysqli_complete_transaction_with_flags(
    mylite_mysqli_link *link,
    const char *command,
    zend_long flags,
    const char *name,
    size_t name_length
) {
    const zend_long allowed_flags =
        MYLITE_MYSQLI_TRANS_COR_AND_CHAIN | MYLITE_MYSQLI_TRANS_COR_AND_NO_CHAIN |
        MYLITE_MYSQLI_TRANS_COR_RELEASE | MYLITE_MYSQLI_TRANS_COR_NO_RELEASE;
    const bool chain = (flags & MYLITE_MYSQLI_TRANS_COR_AND_CHAIN) != 0;
    const bool no_chain = (flags & MYLITE_MYSQLI_TRANS_COR_AND_NO_CHAIN) != 0;
    bool ok = false;
    smart_str sql = {0};

    if (flags < 0 || (flags & ~allowed_flags) != 0 || (chain && no_chain)) {
        return mylite_mysqli_reject_link_feature(
            link,
            "unsupported or conflicting mysqli transaction completion flags"
        );
    }
    if ((flags & MYLITE_MYSQLI_TRANS_COR_RELEASE) != 0) {
        return mylite_mysqli_reject_link_feature(
            link,
            "MYSQLI_TRANS_COR_RELEASE is not supported by the embedded driver"
        );
    }
    if (name != NULL && name_length != 0U) {
        return mylite_mysqli_reject_link_feature(
            link,
            "named mysqli transactions are not supported by the embedded driver"
        );
    }

    smart_str_appends(&sql, command);
    if (chain) {
        smart_str_appends(&sql, " AND CHAIN");
    } else if (no_chain) {
        smart_str_appends(&sql, " AND NO CHAIN");
    }
    if ((flags & MYLITE_MYSQLI_TRANS_COR_NO_RELEASE) != 0) {
        smart_str_appends(&sql, " NO RELEASE");
    }
    smart_str_0(&sql);
    ok = mylite_mysqli_link_real_query(link, ZSTR_VAL(sql.s), ZSTR_LEN(sql.s));
    zend_string_release(sql.s);
    return ok;
}

static bool mylite_mysqli_reject_link_feature(mylite_mysqli_link *link, const char *message) {
    mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_UNSUPPORTED, "42000", message);
    mylite_mysqli_report_link_error(link);
    return false;
}

static bool mylite_mysqli_reject_stmt_feature(mylite_mysqli_stmt *stmt, const char *message) {
    mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_UNSUPPORTED, "42000", message);
    mylite_mysqli_report_stmt_error(stmt);
    return false;
}

PHP_METHOD(mysqli_warning, __construct) {
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(mysqli_warning, next) {
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(
        mylite_mysqli_warning_next_internal(mylite_mysqli_warning_from_obj(Z_OBJ_P(getThis())))
    );
}

PHP_METHOD(mysqli_sql_exception, getSqlState) {
    zval *property = zend_read_property(
        mylite_mysqli_exception_ce,
        Z_OBJ_P(getThis()),
        "sqlstate",
        strlen("sqlstate"),
        true,
        NULL
    );

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_ZVAL(property, true, false);
}
