#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <Zend/zend_smart_str.h>
#include <ext/spl/spl_exceptions.h>
#include <ext/standard/info.h>
#include <mylite/mylite.h>
#include <php.h>
#include <zend_exceptions.h>
#include <zend_interfaces.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PHP_MYLITE_MYSQLI_VERSION "0.1.0"

#define MYLITE_MYSQLI_ASSOC 1
#define MYLITE_MYSQLI_NUM 2
#define MYLITE_MYSQLI_BOTH 3
#define MYLITE_MYSQLI_STORE_RESULT 0
#define MYLITE_MYSQLI_USE_RESULT 1
#define MYLITE_MYSQLI_REPORT_OFF 0
#define MYLITE_MYSQLI_REPORT_ERROR 1
#define MYLITE_MYSQLI_REPORT_STRICT 2
#define MYLITE_MYSQLI_REPORT_INDEX 4
#define MYLITE_MYSQLI_REPORT_ALL 255
#define MYLITE_MYSQLI_SERVER_INFO "8.4.9"
#define MYLITE_MYSQLI_SERVER_VERSION 80409
#define MYLITE_MYSQLI_FIELD_FLAG_NOT_NULL 1
#define MYLITE_MYSQLI_FIELD_FLAG_PRI_KEY 2
#define MYLITE_MYSQLI_FIELD_FLAG_UNIQUE_KEY 4
#define MYLITE_MYSQLI_FIELD_FLAG_MULTIPLE_KEY 8
#define MYLITE_MYSQLI_FIELD_FLAG_BLOB 16
#define MYLITE_MYSQLI_FIELD_FLAG_UNSIGNED 32
#define MYLITE_MYSQLI_FIELD_FLAG_ZEROFILL 64
#define MYLITE_MYSQLI_FIELD_FLAG_BINARY 128
#define MYLITE_MYSQLI_FIELD_FLAG_AUTO_INCREMENT 512
#define MYLITE_MYSQLI_FIELD_FLAG_NUM 32768
#define MYLITE_MYSQLI_FIELD_TYPE_DECIMAL 0
#define MYLITE_MYSQLI_FIELD_TYPE_TINY 1
#define MYLITE_MYSQLI_FIELD_TYPE_SHORT 2
#define MYLITE_MYSQLI_FIELD_TYPE_LONG 3
#define MYLITE_MYSQLI_FIELD_TYPE_FLOAT 4
#define MYLITE_MYSQLI_FIELD_TYPE_DOUBLE 5
#define MYLITE_MYSQLI_FIELD_TYPE_NULL 6
#define MYLITE_MYSQLI_FIELD_TYPE_TIMESTAMP 7
#define MYLITE_MYSQLI_FIELD_TYPE_LONGLONG 8
#define MYLITE_MYSQLI_FIELD_TYPE_INT24 9
#define MYLITE_MYSQLI_FIELD_TYPE_DATE 10
#define MYLITE_MYSQLI_FIELD_TYPE_TIME 11
#define MYLITE_MYSQLI_FIELD_TYPE_DATETIME 12
#define MYLITE_MYSQLI_FIELD_TYPE_YEAR 13
#define MYLITE_MYSQLI_FIELD_TYPE_NEWDATE 14
#define MYLITE_MYSQLI_FIELD_TYPE_BIT 16
#define MYLITE_MYSQLI_FIELD_TYPE_NEWDECIMAL 246
#define MYLITE_MYSQLI_FIELD_TYPE_ENUM 247
#define MYLITE_MYSQLI_FIELD_TYPE_SET 248
#define MYLITE_MYSQLI_FIELD_TYPE_BLOB 252
#define MYLITE_MYSQLI_FIELD_TYPE_VAR_STRING 253
#define MYLITE_MYSQLI_FIELD_TYPE_STRING 254

enum mylite_mysqli_error_code {
    MYLITE_MYSQLI_ERROR_NONE = 0,
    MYLITE_MYSQLI_ERROR_CLIENT = 2000,
    MYLITE_MYSQLI_ERROR_CONNECTION = 2002,
    MYLITE_MYSQLI_ERROR_PARSE = 1064,
    MYLITE_MYSQLI_ERROR_UNSUPPORTED = 1235,
    MYLITE_MYSQLI_ERROR_EXEC = 1105,
};

typedef struct {
    mylite_db *database;
    zend_string *path;
    zend_string *error;
    char sqlstate[6];
    zend_long affected_rows;
    zend_long insert_id;
    zend_long field_count;
    zend_long warning_count;
    int error_code;
    bool connected;
    zval last_result;
    zend_object std;
} mylite_mysqli_link;

typedef struct {
    zend_string **names;
    zend_string **schemas;
    zend_string **tables;
    zend_string **origin_tables;
    zend_string **origin_names;
    int *types;
    unsigned int *flags;
    uint64_t *lengths;
    uint64_t *max_lengths;
    unsigned int *decimals;
    unsigned int *charsets;
    bool *nullable;
    zval *values;
    uint32_t column_count;
    uint32_t row_count;
    uint32_t row_capacity;
    uint32_t cursor;
    uint32_t field_cursor;
    zend_object std;
} mylite_mysqli_result;

typedef struct {
    zval link;
    zval result;
    zval *bound_params;
    zval *bound_results;
    zend_string *query;
    zend_string *types;
    char sqlstate[6];
    zend_string *error;
    zend_long affected_rows;
    zend_long insert_id;
    zend_long num_rows;
    zend_long field_count;
    uint32_t bound_param_count;
    uint32_t bound_result_count;
    uint32_t param_count;
    int error_code;
    zend_object std;
} mylite_mysqli_stmt;

typedef struct {
    uint32_t index;
    zend_object std;
} mylite_mysqli_warning;

ZEND_BEGIN_MODULE_GLOBALS(mylite_mysqli)
int report_mode;
int connect_errno;
char connect_error[512];
ZEND_END_MODULE_GLOBALS(mylite_mysqli)

ZEND_DECLARE_MODULE_GLOBALS(mylite_mysqli)

#ifdef ZTS
#  define MYLITE_MYSQLI_G(v) ZEND_TSRMG(mylite_mysqli_globals_id, zend_mylite_mysqli_globals *, v)
#else
#  define MYLITE_MYSQLI_G(v) (mylite_mysqli_globals.v)
#endif

static zend_class_entry *mylite_mysqli_link_ce;
static zend_class_entry *mylite_mysqli_result_ce;
static zend_class_entry *mylite_mysqli_stmt_ce;
static zend_class_entry *mylite_mysqli_driver_ce;
static zend_class_entry *mylite_mysqli_warning_ce;
static zend_class_entry *mylite_mysqli_exception_ce;
static zend_object_handlers mylite_mysqli_link_handlers;
static zend_object_handlers mylite_mysqli_result_handlers;
static zend_object_handlers mylite_mysqli_stmt_handlers;
static zend_object_handlers mylite_mysqli_warning_handlers;

static zend_object *mylite_mysqli_link_create(zend_class_entry *class_entry);
static void mylite_mysqli_link_free(zend_object *object);
static zend_object *mylite_mysqli_result_create(zend_class_entry *class_entry);
static void mylite_mysqli_result_free(zend_object *object);
static zend_object *mylite_mysqli_stmt_create(zend_class_entry *class_entry);
static void mylite_mysqli_stmt_free(zend_object *object);
static zend_object *mylite_mysqli_warning_create(zend_class_entry *class_entry);
static void mylite_mysqli_warning_free(zend_object *object);

static mylite_mysqli_link *mylite_mysqli_link_from_obj(zend_object *object);
static mylite_mysqli_result *mylite_mysqli_result_from_obj(zend_object *object);
static mylite_mysqli_stmt *mylite_mysqli_stmt_from_obj(zend_object *object);

static bool mylite_mysqli_connect_link(mylite_mysqli_link *link, const char *host,
                                       size_t host_length, const char *database,
                                       size_t database_length, const char *socket,
                                       size_t socket_length);
static bool mylite_mysqli_link_query(mylite_mysqli_link *link, const char *sql, size_t sql_length,
                                     zval *out_result);
static bool mylite_mysqli_link_real_query(mylite_mysqli_link *link, const char *sql,
                                          size_t sql_length);
static bool mylite_mysqli_stmt_prepare_internal(mylite_mysqli_stmt *stmt, const char *sql,
                                                size_t sql_length);
static bool mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt *stmt, zval *params);
static bool mylite_mysqli_execute_sql(mylite_mysqli_link *link, const char *sql, size_t sql_length,
                                      zval *out_result);
static bool mylite_mysqli_buffer_result(mylite_mysqli_link *link, const mylite_result *source,
                                        zval *out_result);
static void mylite_mysqli_result_fetch(mylite_mysqli_result *result, int mode, zval *return_value);
static void mylite_mysqli_result_fetch_column(mylite_mysqli_result *result, zend_long column,
                                              zval *return_value);
static void mylite_mysqli_result_fetch_field(mylite_mysqli_result *result, uint32_t index,
                                             zval *return_value);
static zend_string *mylite_mysqli_interpolate_query(zend_string *query, zval *params,
                                                    uint32_t param_count);
static zend_string *mylite_mysqli_param_to_sql(zval *value);
static uint32_t mylite_mysqli_count_markers(const char *sql, size_t sql_length);
static bool mylite_mysqli_is_local_path(const char *value, size_t length);
static zend_string *mylite_mysqli_resolve_path(const char *host, size_t host_length,
                                               const char *database, size_t database_length,
                                               const char *socket, size_t socket_length,
                                               bool *out_memory, bool *out_use_database);
static zend_string *mylite_mysqli_quote_identifier(const char *value, size_t length);
static zend_string *mylite_mysqli_escape_string(const char *value, size_t length);
static bool mylite_mysqli_is_line_comment_terminator(char ch);
static bool mylite_mysqli_is_dash_comment_start(const char *sql, size_t sql_length, size_t index);
static void mylite_mysqli_set_error(mylite_mysqli_link *link, int error_code, const char *sqlstate,
                                    const char *message);
static void mylite_mysqli_set_stmt_error(mylite_mysqli_stmt *stmt, int error_code,
                                         const char *sqlstate, const char *message);
static void mylite_mysqli_clear_error(mylite_mysqli_link *link);
static void mylite_mysqli_clear_stmt_error(mylite_mysqli_stmt *stmt);
static void mylite_mysqli_report_link_error(mylite_mysqli_link *link);
static void mylite_mysqli_report_stmt_error(mylite_mysqli_stmt *stmt);
static int mylite_mysqli_error_from_status(int status, const char **out_sqlstate);
static void mylite_mysqli_update_link_properties(mylite_mysqli_link *link);
static void mylite_mysqli_update_result_properties(mylite_mysqli_result *result);
static void mylite_mysqli_update_stmt_properties(mylite_mysqli_stmt *stmt);
static void mylite_mysqli_set_global_connect_error(int error_code, const char *message);
static void mylite_mysqli_init_globals(zend_mylite_mysqli_globals *globals);
static void mylite_mysqli_register_constants(int module_number);
static void mylite_mysqli_register_classes(void);

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_connect, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hostname, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, username, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, password, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, database, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, socket, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_real_connect, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hostname, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, username, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, password, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, database, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, socket, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_real_connect_method, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hostname, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, username, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, password, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, database, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, socket, IS_STRING, 1, "null")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_query, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, result_mode, IS_LONG, 0, "MYSQLI_STORE_RESULT")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_query_string, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_query_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, result_mode, IS_LONG, 0, "MYSQLI_STORE_RESULT")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_query_string_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_execute_query, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_execute_query_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_link_only, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_store_result, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mode, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_store_result_method, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mode, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_get_client_info, 0, 0, 0)
ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(0, mysql, mysqli, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_bool_link, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_bool_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_string_link, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_string_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_debug, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, options, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_tx, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, name, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_tx_method, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, name, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_change_user, 0, 0, 4)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, username, IS_STRING, 0)
ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
ZEND_ARG_TYPE_INFO(0, database, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_change_user_method, 0, 0, 3)
ZEND_ARG_TYPE_INFO(0, username, IS_STRING, 0)
ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
ZEND_ARG_TYPE_INFO(0, database, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_options, 0, 0, 3)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, option, IS_LONG, 0)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_options_method, 0, 0, 2)
ZEND_ARG_TYPE_INFO(0, option, IS_LONG, 0)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_kill, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, process_id, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_kill_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, process_id, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_refresh, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_refresh_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_ssl_set, 0, 0, 6)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, certificate, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, ca_certificate, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, ca_path, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, cipher_algos, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_ssl_set_method, 0, 0, 5)
ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, certificate, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, ca_certificate, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, ca_path, IS_STRING, 1)
ZEND_ARG_TYPE_INFO(0, cipher_algos, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_poll, 0, 0, 4)
ZEND_ARG_TYPE_INFO(1, read, IS_ARRAY, 1)
ZEND_ARG_TYPE_INFO(1, error, IS_ARRAY, 1)
ZEND_ARG_TYPE_INFO(1, reject, IS_ARRAY, 0)
ZEND_ARG_TYPE_INFO(0, seconds, IS_LONG, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, microseconds, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_data_seek, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, result, mysqli_result, 0)
ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_data_seek_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_result_fetch_mode, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, result, mysqli_result, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mode, IS_LONG, 0, "MYSQLI_BOTH")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_result_fetch_mode_method, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mode, IS_LONG, 0, "MYSQLI_BOTH")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_result_only, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, result, mysqli_result, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_mysqli_result_get_iterator, 0, 0, Traversable, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_fetch_column, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, result, mysqli_result, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, column, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_fetch_column_method, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, column, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_field_direct, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, result, mysqli_result, 0)
ZEND_ARG_TYPE_INFO(0, index, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_field_direct_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, index, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_fetch_object, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, result, mysqli_result, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, class, IS_STRING, 0, "\"stdClass\"")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, constructor_args, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_fetch_object_method, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, class, IS_STRING, 0, "\"stdClass\"")
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, constructor_args, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_prepare, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_prepare_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_result_construct, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, result_mode, IS_LONG, 0, "MYSQLI_STORE_RESULT")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_construct, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, mysql, mysqli, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, query, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_execute, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_execute_method, 0, 0, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_bind_param, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_TYPE_INFO(0, types, IS_STRING, 0)
ZEND_ARG_VARIADIC_TYPE_INFO(1, vars, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_bind_param_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, types, IS_STRING, 0)
ZEND_ARG_VARIADIC_TYPE_INFO(1, vars, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_bind_result, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_VARIADIC_TYPE_INFO(1, vars, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_bind_result_method, 0, 0, 0)
ZEND_ARG_VARIADIC_TYPE_INFO(1, vars, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_string, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_string_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_only, 0, 0, 1)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_data_seek, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_data_seek_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_attr, 0, 0, 2)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_TYPE_INFO(0, attribute, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_attr_method, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, attribute, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_attr_set, 0, 0, 3)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_TYPE_INFO(0, attribute, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, value, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_attr_set_method, 0, 0, 2)
ZEND_ARG_TYPE_INFO(0, attribute, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, value, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_long_data, 0, 0, 3)
ZEND_ARG_OBJ_INFO(0, statement, mysqli_stmt, 0)
ZEND_ARG_TYPE_INFO(0, param_num, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_stmt_long_data_method, 0, 0, 2)
ZEND_ARG_TYPE_INFO(0, param_num, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_report, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqli_no_args, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(mysqli_connect);
PHP_FUNCTION(mysqli_init);
PHP_FUNCTION(mysqli_real_connect);
PHP_FUNCTION(mysqli_query);
PHP_FUNCTION(mysqli_execute_query);
PHP_FUNCTION(mysqli_real_query);
PHP_FUNCTION(mysqli_store_result);
PHP_FUNCTION(mysqli_use_result);
PHP_FUNCTION(mysqli_close);
PHP_FUNCTION(mysqli_connect_errno);
PHP_FUNCTION(mysqli_connect_error);
PHP_FUNCTION(mysqli_errno);
PHP_FUNCTION(mysqli_error);
PHP_FUNCTION(mysqli_error_list);
PHP_FUNCTION(mysqli_sqlstate);
PHP_FUNCTION(mysqli_field_count);
PHP_FUNCTION(mysqli_affected_rows);
PHP_FUNCTION(mysqli_insert_id);
PHP_FUNCTION(mysqli_info);
PHP_FUNCTION(mysqli_warning_count);
PHP_FUNCTION(mysqli_get_warnings);
PHP_FUNCTION(mysqli_get_client_info);
PHP_FUNCTION(mysqli_get_client_version);
PHP_FUNCTION(mysqli_get_host_info);
PHP_FUNCTION(mysqli_get_proto_info);
PHP_FUNCTION(mysqli_get_server_info);
PHP_FUNCTION(mysqli_get_server_version);
PHP_FUNCTION(mysqli_character_set_name);
PHP_FUNCTION(mysqli_get_charset);
PHP_FUNCTION(mysqli_set_charset);
PHP_FUNCTION(mysqli_select_db);
PHP_FUNCTION(mysqli_real_escape_string);
PHP_FUNCTION(mysqli_escape_string);
PHP_FUNCTION(mysqli_autocommit);
PHP_FUNCTION(mysqli_begin_transaction);
PHP_FUNCTION(mysqli_commit);
PHP_FUNCTION(mysqli_rollback);
PHP_FUNCTION(mysqli_savepoint);
PHP_FUNCTION(mysqli_release_savepoint);
PHP_FUNCTION(mysqli_change_user);
PHP_FUNCTION(mysqli_options);
PHP_FUNCTION(mysqli_set_opt);
PHP_FUNCTION(mysqli_ssl_set);
PHP_FUNCTION(mysqli_stat);
PHP_FUNCTION(mysqli_ping);
PHP_FUNCTION(mysqli_kill);
PHP_FUNCTION(mysqli_dump_debug_info);
PHP_FUNCTION(mysqli_debug);
PHP_FUNCTION(mysqli_refresh);
PHP_FUNCTION(mysqli_multi_query);
PHP_FUNCTION(mysqli_more_results);
PHP_FUNCTION(mysqli_next_result);
PHP_FUNCTION(mysqli_reap_async_query);
PHP_FUNCTION(mysqli_poll);
PHP_FUNCTION(mysqli_thread_id);
PHP_FUNCTION(mysqli_thread_safe);
PHP_FUNCTION(mysqli_get_connection_stats);
PHP_FUNCTION(mysqli_get_client_stats);
PHP_FUNCTION(mysqli_get_links_stats);
PHP_FUNCTION(mysqli_report);
PHP_FUNCTION(mysqli_prepare);
PHP_FUNCTION(mysqli_stmt_init);
PHP_FUNCTION(mysqli_fetch_assoc);
PHP_FUNCTION(mysqli_fetch_row);
PHP_FUNCTION(mysqli_fetch_array);
PHP_FUNCTION(mysqli_fetch_all);
PHP_FUNCTION(mysqli_fetch_object);
PHP_FUNCTION(mysqli_fetch_column);
PHP_FUNCTION(mysqli_fetch_field);
PHP_FUNCTION(mysqli_fetch_fields);
PHP_FUNCTION(mysqli_fetch_field_direct);
PHP_FUNCTION(mysqli_fetch_lengths);
PHP_FUNCTION(mysqli_data_seek);
PHP_FUNCTION(mysqli_field_seek);
PHP_FUNCTION(mysqli_field_tell);
PHP_FUNCTION(mysqli_free_result);
PHP_FUNCTION(mysqli_num_fields);
PHP_FUNCTION(mysqli_num_rows);
PHP_FUNCTION(mysqli_stmt_prepare);
PHP_FUNCTION(mysqli_stmt_execute);
PHP_FUNCTION(mysqli_execute);
PHP_FUNCTION(mysqli_stmt_bind_param);
PHP_FUNCTION(mysqli_stmt_bind_result);
PHP_FUNCTION(mysqli_stmt_fetch);
PHP_FUNCTION(mysqli_stmt_get_result);
PHP_FUNCTION(mysqli_stmt_result_metadata);
PHP_FUNCTION(mysqli_stmt_store_result);
PHP_FUNCTION(mysqli_stmt_free_result);
PHP_FUNCTION(mysqli_stmt_close);
PHP_FUNCTION(mysqli_stmt_data_seek);
PHP_FUNCTION(mysqli_stmt_reset);
PHP_FUNCTION(mysqli_stmt_attr_get);
PHP_FUNCTION(mysqli_stmt_attr_set);
PHP_FUNCTION(mysqli_stmt_send_long_data);
PHP_FUNCTION(mysqli_stmt_errno);
PHP_FUNCTION(mysqli_stmt_error);
PHP_FUNCTION(mysqli_stmt_error_list);
PHP_FUNCTION(mysqli_stmt_sqlstate);
PHP_FUNCTION(mysqli_stmt_field_count);
PHP_FUNCTION(mysqli_stmt_affected_rows);
PHP_FUNCTION(mysqli_stmt_insert_id);
PHP_FUNCTION(mysqli_stmt_num_rows);
PHP_FUNCTION(mysqli_stmt_param_count);
PHP_FUNCTION(mysqli_stmt_get_warnings);
PHP_FUNCTION(mysqli_stmt_more_results);
PHP_FUNCTION(mysqli_stmt_next_result);

PHP_METHOD(mysqli, __construct);
PHP_METHOD(mysqli, connect);
PHP_METHOD(mysqli, real_connect);
PHP_METHOD(mysqli, query);
PHP_METHOD(mysqli, execute_query);
PHP_METHOD(mysqli, real_query);
PHP_METHOD(mysqli, store_result);
PHP_METHOD(mysqli, use_result);
PHP_METHOD(mysqli, close);
PHP_METHOD(mysqli, prepare);
PHP_METHOD(mysqli, stmt_init);
PHP_METHOD(mysqli, autocommit);
PHP_METHOD(mysqli, begin_transaction);
PHP_METHOD(mysqli, commit);
PHP_METHOD(mysqli, rollback);
PHP_METHOD(mysqli, savepoint);
PHP_METHOD(mysqli, release_savepoint);
PHP_METHOD(mysqli, change_user);
PHP_METHOD(mysqli, character_set_name);
PHP_METHOD(mysqli, get_charset);
PHP_METHOD(mysqli, get_client_info);
PHP_METHOD(mysqli, get_connection_stats);
PHP_METHOD(mysqli, get_server_info);
PHP_METHOD(mysqli, get_warnings);
PHP_METHOD(mysqli, init);
PHP_METHOD(mysqli, kill);
PHP_METHOD(mysqli, multi_query);
PHP_METHOD(mysqli, more_results);
PHP_METHOD(mysqli, next_result);
PHP_METHOD(mysqli, ping);
PHP_METHOD(mysqli, poll);
PHP_METHOD(mysqli, real_escape_string);
PHP_METHOD(mysqli, reap_async_query);
PHP_METHOD(mysqli, escape_string);
PHP_METHOD(mysqli, select_db);
PHP_METHOD(mysqli, set_charset);
PHP_METHOD(mysqli, options);
PHP_METHOD(mysqli, set_opt);
PHP_METHOD(mysqli, ssl_set);
PHP_METHOD(mysqli, stat);
PHP_METHOD(mysqli, thread_safe);
PHP_METHOD(mysqli, dump_debug_info);
PHP_METHOD(mysqli, debug);
PHP_METHOD(mysqli, refresh);

PHP_METHOD(mysqli_result, __construct);
PHP_METHOD(mysqli_result, close);
PHP_METHOD(mysqli_result, free);
PHP_METHOD(mysqli_result, free_result);
PHP_METHOD(mysqli_result, data_seek);
PHP_METHOD(mysqli_result, fetch_assoc);
PHP_METHOD(mysqli_result, fetch_row);
PHP_METHOD(mysqli_result, fetch_array);
PHP_METHOD(mysqli_result, fetch_all);
PHP_METHOD(mysqli_result, fetch_object);
PHP_METHOD(mysqli_result, fetch_column);
PHP_METHOD(mysqli_result, fetch_field);
PHP_METHOD(mysqli_result, fetch_fields);
PHP_METHOD(mysqli_result, fetch_field_direct);
PHP_METHOD(mysqli_result, field_seek);
PHP_METHOD(mysqli_result, getIterator);

PHP_METHOD(mysqli_stmt, __construct);
PHP_METHOD(mysqli_stmt, prepare);
PHP_METHOD(mysqli_stmt, execute);
PHP_METHOD(mysqli_stmt, bind_param);
PHP_METHOD(mysqli_stmt, bind_result);
PHP_METHOD(mysqli_stmt, fetch);
PHP_METHOD(mysqli_stmt, get_result);
PHP_METHOD(mysqli_stmt, result_metadata);
PHP_METHOD(mysqli_stmt, store_result);
PHP_METHOD(mysqli_stmt, free_result);
PHP_METHOD(mysqli_stmt, close);
PHP_METHOD(mysqli_stmt, data_seek);
PHP_METHOD(mysqli_stmt, reset);
PHP_METHOD(mysqli_stmt, attr_get);
PHP_METHOD(mysqli_stmt, attr_set);
PHP_METHOD(mysqli_stmt, send_long_data);
PHP_METHOD(mysqli_stmt, get_warnings);
PHP_METHOD(mysqli_stmt, more_results);
PHP_METHOD(mysqli_stmt, next_result);
PHP_METHOD(mysqli_stmt, num_rows);

PHP_METHOD(mysqli_warning, __construct);
PHP_METHOD(mysqli_warning, next);
PHP_METHOD(mysqli_sql_exception, getSqlState);

/* clang-format off */
static const zend_function_entry mylite_mysqli_functions[] = {
    PHP_FE(mysqli_connect, arginfo_mysqli_connect)
    PHP_FE(mysqli_init, arginfo_mysqli_no_args)
    PHP_FE(mysqli_real_connect, arginfo_mysqli_real_connect)
    PHP_FE(mysqli_query, arginfo_mysqli_query)
    PHP_FE(mysqli_execute_query, arginfo_mysqli_execute_query)
    PHP_FE(mysqli_real_query, arginfo_mysqli_query_string)
    PHP_FE(mysqli_store_result, arginfo_mysqli_store_result)
    PHP_FE(mysqli_use_result, arginfo_mysqli_link_only)
    PHP_FE(mysqli_close, arginfo_mysqli_link_only)
    PHP_FE(mysqli_connect_errno, arginfo_mysqli_no_args)
    PHP_FE(mysqli_connect_error, arginfo_mysqli_no_args)
    PHP_FE(mysqli_errno, arginfo_mysqli_link_only)
    PHP_FE(mysqli_error, arginfo_mysqli_link_only)
    PHP_FE(mysqli_error_list, arginfo_mysqli_link_only)
    PHP_FE(mysqli_sqlstate, arginfo_mysqli_link_only)
    PHP_FE(mysqli_field_count, arginfo_mysqli_link_only)
    PHP_FE(mysqli_affected_rows, arginfo_mysqli_link_only)
    PHP_FE(mysqli_insert_id, arginfo_mysqli_link_only)
    PHP_FE(mysqli_info, arginfo_mysqli_link_only)
    PHP_FE(mysqli_warning_count, arginfo_mysqli_link_only)
    PHP_FE(mysqli_get_warnings, arginfo_mysqli_link_only)
    PHP_FE(mysqli_get_client_info, arginfo_mysqli_get_client_info)
    PHP_FE(mysqli_get_client_version, arginfo_mysqli_no_args)
    PHP_FE(mysqli_get_host_info, arginfo_mysqli_link_only)
    PHP_FE(mysqli_get_proto_info, arginfo_mysqli_link_only)
    PHP_FE(mysqli_get_server_info, arginfo_mysqli_link_only)
    PHP_FE(mysqli_get_server_version, arginfo_mysqli_link_only)
    PHP_FE(mysqli_character_set_name, arginfo_mysqli_link_only)
    PHP_FE(mysqli_get_charset, arginfo_mysqli_link_only)
    PHP_FE(mysqli_set_charset, arginfo_mysqli_string_link)
    PHP_FE(mysqli_select_db, arginfo_mysqli_string_link)
    PHP_FE(mysqli_real_escape_string, arginfo_mysqli_string_link)
    PHP_FE(mysqli_escape_string, arginfo_mysqli_string_link)
    PHP_FE(mysqli_autocommit, arginfo_mysqli_bool_link)
    PHP_FE(mysqli_begin_transaction, arginfo_mysqli_tx)
    PHP_FE(mysqli_commit, arginfo_mysqli_tx)
    PHP_FE(mysqli_rollback, arginfo_mysqli_tx)
    PHP_FE(mysqli_savepoint, arginfo_mysqli_string_link)
    PHP_FE(mysqli_release_savepoint, arginfo_mysqli_string_link)
    PHP_FE(mysqli_change_user, arginfo_mysqli_change_user)
    PHP_FE(mysqli_options, arginfo_mysqli_options)
    PHP_FE(mysqli_set_opt, arginfo_mysqli_options)
    PHP_FE(mysqli_ssl_set, arginfo_mysqli_ssl_set)
    PHP_FE(mysqli_stat, arginfo_mysqli_link_only)
    PHP_FE(mysqli_ping, arginfo_mysqli_link_only)
    PHP_FE(mysqli_kill, arginfo_mysqli_kill)
    PHP_FE(mysqli_dump_debug_info, arginfo_mysqli_link_only)
    PHP_FE(mysqli_debug, arginfo_mysqli_debug)
    PHP_FE(mysqli_refresh, arginfo_mysqli_refresh)
    PHP_FE(mysqli_multi_query, arginfo_mysqli_query_string)
    PHP_FE(mysqli_more_results, arginfo_mysqli_link_only)
    PHP_FE(mysqli_next_result, arginfo_mysqli_link_only)
    PHP_FE(mysqli_reap_async_query, arginfo_mysqli_link_only)
    PHP_FE(mysqli_poll, arginfo_mysqli_poll)
    PHP_FE(mysqli_thread_id, arginfo_mysqli_link_only)
    PHP_FE(mysqli_thread_safe, arginfo_mysqli_no_args)
    PHP_FE(mysqli_get_connection_stats, arginfo_mysqli_link_only)
    PHP_FE(mysqli_get_client_stats, arginfo_mysqli_no_args)
    PHP_FE(mysqli_get_links_stats, arginfo_mysqli_no_args)
    PHP_FE(mysqli_report, arginfo_mysqli_report)
    PHP_FE(mysqli_prepare, arginfo_mysqli_prepare)
    PHP_FE(mysqli_stmt_init, arginfo_mysqli_link_only)
    PHP_FE(mysqli_fetch_assoc, arginfo_mysqli_result_only)
    PHP_FE(mysqli_fetch_row, arginfo_mysqli_result_only)
    PHP_FE(mysqli_fetch_array, arginfo_mysqli_result_fetch_mode)
    PHP_FE(mysqli_fetch_all, arginfo_mysqli_result_fetch_mode)
    PHP_FE(mysqli_fetch_object, arginfo_mysqli_fetch_object)
    PHP_FE(mysqli_fetch_column, arginfo_mysqli_fetch_column)
    PHP_FE(mysqli_fetch_field, arginfo_mysqli_result_only)
    PHP_FE(mysqli_fetch_fields, arginfo_mysqli_result_only)
    PHP_FE(mysqli_fetch_field_direct, arginfo_mysqli_field_direct)
    PHP_FE(mysqli_fetch_lengths, arginfo_mysqli_result_only)
    PHP_FE(mysqli_data_seek, arginfo_mysqli_data_seek)
    PHP_FE(mysqli_field_seek, arginfo_mysqli_field_direct)
    PHP_FE(mysqli_field_tell, arginfo_mysqli_result_only)
    PHP_FE(mysqli_free_result, arginfo_mysqli_result_only)
    PHP_FE(mysqli_num_fields, arginfo_mysqli_result_only)
    PHP_FE(mysqli_num_rows, arginfo_mysqli_result_only)
    PHP_FE(mysqli_stmt_prepare, arginfo_mysqli_stmt_string)
    PHP_FE(mysqli_stmt_execute, arginfo_mysqli_stmt_execute)
    PHP_FALIAS(mysqli_execute, mysqli_stmt_execute, arginfo_mysqli_stmt_execute)
    PHP_FE(mysqli_stmt_bind_param, arginfo_mysqli_stmt_bind_param)
    PHP_FE(mysqli_stmt_bind_result, arginfo_mysqli_stmt_bind_result)
    PHP_FE(mysqli_stmt_fetch, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_get_result, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_result_metadata, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_store_result, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_free_result, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_close, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_data_seek, arginfo_mysqli_stmt_data_seek)
    PHP_FE(mysqli_stmt_reset, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_attr_get, arginfo_mysqli_stmt_attr)
    PHP_FE(mysqli_stmt_attr_set, arginfo_mysqli_stmt_attr_set)
    PHP_FE(mysqli_stmt_send_long_data, arginfo_mysqli_stmt_long_data)
    PHP_FE(mysqli_stmt_errno, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_error, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_error_list, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_sqlstate, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_field_count, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_affected_rows, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_insert_id, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_num_rows, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_param_count, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_get_warnings, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_more_results, arginfo_mysqli_stmt_only)
    PHP_FE(mysqli_stmt_next_result, arginfo_mysqli_stmt_only)
    PHP_FE_END
};

static const zend_function_entry mylite_mysqli_link_methods[] = {
    PHP_ME(mysqli, __construct, arginfo_mysqli_connect, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, autocommit, arginfo_mysqli_bool_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, begin_transaction, arginfo_mysqli_tx_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, change_user, arginfo_mysqli_change_user_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, character_set_name, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, close, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, commit, arginfo_mysqli_tx_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, connect, arginfo_mysqli_connect, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, dump_debug_info, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, debug, arginfo_mysqli_debug, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, get_charset, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, execute_query, arginfo_mysqli_execute_query_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, get_client_info, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, get_connection_stats, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, get_server_info, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, get_warnings, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, init, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, kill, arginfo_mysqli_kill_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, multi_query, arginfo_mysqli_query_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, more_results, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, next_result, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, ping, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, poll, arginfo_mysqli_poll, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(mysqli, prepare, arginfo_mysqli_prepare_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, query, arginfo_mysqli_query_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, real_connect, arginfo_mysqli_real_connect_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, real_escape_string, arginfo_mysqli_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, reap_async_query, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, escape_string, arginfo_mysqli_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, real_query, arginfo_mysqli_query_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, release_savepoint, arginfo_mysqli_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, rollback, arginfo_mysqli_tx_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, savepoint, arginfo_mysqli_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, select_db, arginfo_mysqli_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, set_charset, arginfo_mysqli_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, options, arginfo_mysqli_options_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, set_opt, arginfo_mysqli_options_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, ssl_set, arginfo_mysqli_ssl_set_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, stat, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, stmt_init, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, store_result, arginfo_mysqli_store_result_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, thread_safe, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, use_result, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli, refresh, arginfo_mysqli_refresh_method, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry mylite_mysqli_result_methods[] = {
    PHP_ME(mysqli_result, __construct, arginfo_mysqli_result_construct, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, close, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, free, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, data_seek, arginfo_mysqli_data_seek_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_field, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_fields, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_field_direct, arginfo_mysqli_field_direct_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_all, arginfo_mysqli_result_fetch_mode_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_array, arginfo_mysqli_result_fetch_mode_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_assoc, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_object, arginfo_mysqli_fetch_object_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_row, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, fetch_column, arginfo_mysqli_fetch_column_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, field_seek, arginfo_mysqli_field_direct_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, free_result, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_result, getIterator, arginfo_mysqli_result_get_iterator, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry mylite_mysqli_stmt_methods[] = {
    PHP_ME(mysqli_stmt, __construct, arginfo_mysqli_stmt_construct, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, attr_get, arginfo_mysqli_stmt_attr_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, attr_set, arginfo_mysqli_stmt_attr_set_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, bind_param, arginfo_mysqli_stmt_bind_param_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, bind_result, arginfo_mysqli_stmt_bind_result_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, close, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, data_seek, arginfo_mysqli_stmt_data_seek_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, execute, arginfo_mysqli_stmt_execute_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, fetch, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, get_warnings, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, result_metadata, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, more_results, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, next_result, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, num_rows, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, send_long_data, arginfo_mysqli_stmt_long_data_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, free_result, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, reset, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, prepare, arginfo_mysqli_stmt_string_method, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, store_result, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_stmt, get_result, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry mylite_mysqli_warning_methods[] = {
    PHP_ME(mysqli_warning, __construct, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_ME(mysqli_warning, next, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry mylite_mysqli_exception_methods[] = {
    PHP_ME(mysqli_sql_exception, getSqlState, arginfo_mysqli_no_args, ZEND_ACC_PUBLIC)
    PHP_FE_END
};
/* clang-format on */

PHP_FUNCTION(mysqli_connect)
{
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
    (void)port;
    (void)port_is_null;

    object_init_ex(return_value, mylite_mysqli_link_ce);
    link = mylite_mysqli_link_from_obj(Z_OBJ_P(return_value));
    if (!mylite_mysqli_connect_link(link, host, host_length, database, database_length, socket,
                                    socket_length)) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_init)
{
    ZEND_PARSE_PARAMETERS_NONE();
    object_init_ex(return_value, mylite_mysqli_link_ce);
}

PHP_FUNCTION(mysqli_real_connect)
{
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
    (void)port;
    (void)port_is_null;
    (void)flags;

    RETURN_BOOL(mylite_mysqli_connect_link(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), host,
                                           host_length, database, database_length, socket,
                                           socket_length));
}

PHP_FUNCTION(mysqli_query)
{
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

    (void)result_mode;
    if (!mylite_mysqli_link_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), query, query_length,
                                  return_value)) {
        RETURN_FALSE;
    }
}

PHP_FUNCTION(mysqli_execute_query)
{
    zval *mysql = NULL;
    char *query = NULL;
    size_t query_length = 0U;
    zval *params = NULL;
    zend_string *sql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(query, query_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    sql = params == NULL
              ? zend_string_init(query, query_length, false)
              : mylite_mysqli_interpolate_query(zend_string_init(query, query_length, false),
                                                params, zend_hash_num_elements(Z_ARRVAL_P(params)));
    if (sql == NULL) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        mylite_mysqli_report_link_error(link);
        RETURN_FALSE;
    }

    if (!mylite_mysqli_link_query(link, ZSTR_VAL(sql), ZSTR_LEN(sql), return_value)) {
        zend_string_release(sql);
        RETURN_FALSE;
    }
    zend_string_release(sql);
}

PHP_FUNCTION(mysqli_real_query)
{
    zval *mysql = NULL;
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), query,
                                              query_length));
}

PHP_FUNCTION(mysqli_store_result)
{
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    zend_long mode = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    (void)mode;
    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (Z_TYPE(link->last_result) == IS_OBJECT) {
        ZVAL_COPY(return_value, &link->last_result);
        return;
    }

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_use_result)
{
    ZEND_MN(mysqli_store_result)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_close)
{
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (link->database != NULL) {
        mylite_close(link->database);
        link->database = NULL;
    }
    link->connected = false;
    mylite_mysqli_update_link_properties(link);
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_connect_errno)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(MYLITE_MYSQLI_G(connect_errno));
}

PHP_FUNCTION(mysqli_connect_error)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (MYLITE_MYSQLI_G(connect_errno) == 0) {
        RETURN_NULL();
    }
    RETURN_STRING(MYLITE_MYSQLI_G(connect_error));
}

PHP_FUNCTION(mysqli_errno)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->error_code);
}

PHP_FUNCTION(mysqli_error)
{
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    RETURN_STR_COPY(link->error);
}

PHP_FUNCTION(mysqli_error_list)
{
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    zval entry;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    array_init(return_value);
    if (link->error_code == 0) {
        return;
    }

    array_init(&entry);
    add_assoc_long(&entry, "errno", link->error_code);
    add_assoc_string(&entry, "sqlstate", link->sqlstate);
    add_assoc_str(&entry, "error", zend_string_copy(link->error));
    add_next_index_zval(return_value, &entry);
}

PHP_FUNCTION(mysqli_sqlstate)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->sqlstate);
}

PHP_FUNCTION(mysqli_field_count)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->field_count);
}

PHP_FUNCTION(mysqli_affected_rows)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->affected_rows);
}

PHP_FUNCTION(mysqli_insert_id)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->insert_id);
}

PHP_FUNCTION(mysqli_info)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    (void)mysql;
    RETURN_NULL();
}

PHP_FUNCTION(mysqli_warning_count)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->warning_count);
}

PHP_FUNCTION(mysqli_get_warnings)
{
    zval *mysql = NULL;
    mylite_mysqli_link *link = NULL;
    const char *message = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    link = mylite_mysqli_link_from_obj(Z_OBJ_P(mysql));
    if (link->database == NULL || link->warning_count <= 0) {
        RETURN_FALSE;
    }

    object_init_ex(return_value, mylite_mysqli_warning_ce);
    message = "MyLite warning";
    zend_update_property_string(mylite_mysqli_warning_ce, Z_OBJ_P(return_value), "message",
                                strlen("message"), message == NULL ? "" : message);
    zend_update_property_string(mylite_mysqli_warning_ce, Z_OBJ_P(return_value), "sqlstate",
                                strlen("sqlstate"), "HY000");
    zend_update_property_long(mylite_mysqli_warning_ce, Z_OBJ_P(return_value), "errno",
                              strlen("errno"), 0);
}

PHP_FUNCTION(mysqli_get_client_info)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_OBJECT_OF_CLASS_OR_NULL(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    (void)mysql;
    RETURN_STRING("mylite mysqli " PHP_MYLITE_MYSQLI_VERSION);
}

PHP_FUNCTION(mysqli_get_client_version)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(100);
}

PHP_FUNCTION(mysqli_get_host_info)
{
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

PHP_FUNCTION(mysqli_get_proto_info)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(10);
}

PHP_FUNCTION(mysqli_get_server_info)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(MYLITE_MYSQLI_SERVER_INFO);
}

PHP_FUNCTION(mysqli_get_server_version)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(MYLITE_MYSQLI_SERVER_VERSION);
}

PHP_FUNCTION(mysqli_character_set_name)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING("utf8mb4");
}

PHP_FUNCTION(mysqli_get_charset)
{
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

PHP_FUNCTION(mysqli_set_charset)
{
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
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), ZSTR_VAL(sql),
                                       ZSTR_LEN(sql));
    zend_string_release(sql);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_select_db)
{
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
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), ZSTR_VAL(sql),
                                       ZSTR_LEN(sql));
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_real_escape_string)
{
    zval *mysql = NULL;
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *escaped = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    escaped = mylite_mysqli_escape_string(value, value_length);
    RETURN_STR(escaped);
}

PHP_FUNCTION(mysqli_escape_string)
{
    ZEND_MN(mysqli_real_escape_string)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_autocommit)
{
    zval *mysql = NULL;
    bool enable = false;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    (void)mysql;
    (void)enable;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_begin_transaction)
{
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

    (void)flags;
    (void)name;
    (void)name_length;
    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
                                              "START TRANSACTION", strlen("START TRANSACTION")));
}

PHP_FUNCTION(mysqli_commit)
{
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

    (void)flags;
    (void)name;
    (void)name_length;
    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), "COMMIT",
                                              strlen("COMMIT")));
}

PHP_FUNCTION(mysqli_rollback)
{
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

    (void)flags;
    (void)name;
    (void)name_length;
    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
                                              "ROLLBACK", strlen("ROLLBACK")));
}

PHP_FUNCTION(mysqli_savepoint)
{
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
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), ZSTR_VAL(sql),
                                       ZSTR_LEN(sql));
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_release_savepoint)
{
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
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)), ZSTR_VAL(sql),
                                       ZSTR_LEN(sql));
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_change_user)
{
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
    bool ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql)),
                                            ZSTR_VAL(sql), ZSTR_LEN(sql));

    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_FUNCTION(mysqli_options)
{
    zval *mysql = NULL;
    zval *value = NULL;
    zend_long option = 0;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_LONG(option)
    Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)mysql;
    (void)option;
    (void)value;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_set_opt)
{
    ZEND_MN(mysqli_options)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_ssl_set)
{
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

    (void)mysql;
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
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stat)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING("MyLite embedded mysqli connection");
}

PHP_FUNCTION(mysqli_ping)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_from_obj(Z_OBJ_P(mysql))->connected);
}

PHP_FUNCTION(mysqli_kill)
{
    zval *mysql = NULL;
    zend_long process_id = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_LONG(process_id)
    ZEND_PARSE_PARAMETERS_END();

    (void)mysql;
    (void)process_id;
    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_dump_debug_info)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_debug)
{
    char *options = NULL;
    size_t options_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(options, options_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)options;
    (void)options_length;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_refresh)
{
    zval *mysql = NULL;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    (void)mysql;
    (void)flags;
    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_multi_query)
{
    ZEND_MN(mysqli_real_query)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_more_results)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_next_result)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_reap_async_query)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_poll)
{
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

PHP_FUNCTION(mysqli_thread_id)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(1);
}

PHP_FUNCTION(mysqli_thread_safe)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_get_connection_stats)
{
    zval *mysql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(mysql, mylite_mysqli_link_ce)
    ZEND_PARSE_PARAMETERS_END();

    array_init(return_value);
}

PHP_FUNCTION(mysqli_get_client_stats)
{
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
}

PHP_FUNCTION(mysqli_get_links_stats)
{
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
}

PHP_FUNCTION(mysqli_report)
{
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    MYLITE_MYSQLI_G(report_mode) = (int)flags;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_prepare)
{
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

PHP_FUNCTION(mysqli_stmt_init)
{
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

PHP_FUNCTION(mysqli_fetch_assoc)
{
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
                               MYLITE_MYSQLI_ASSOC, return_value);
}

PHP_FUNCTION(mysqli_fetch_row)
{
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
                               MYLITE_MYSQLI_NUM, return_value);
}

PHP_FUNCTION(mysqli_fetch_array)
{
    zval *result_zval = NULL;
    zend_long mode = MYLITE_MYSQLI_BOTH;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)), (int)mode,
                               return_value);
}

PHP_FUNCTION(mysqli_fetch_all)
{
    zval *result_zval = NULL;
    zend_long mode = MYLITE_MYSQLI_NUM;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    array_init(return_value);
    while (result->cursor < result->row_count) {
        zval row;

        mylite_mysqli_result_fetch(result, (int)mode, &row);
        add_next_index_zval(return_value, &row);
    }
}

PHP_FUNCTION(mysqli_fetch_object)
{
    zval *result_zval = NULL;
    char *class_name = NULL;
    size_t class_name_length = 0U;
    zval *constructor_args = NULL;
    zval row;
    zend_class_entry *class_entry = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING(class_name, class_name_length)
    Z_PARAM_ARRAY(constructor_args)
    ZEND_PARSE_PARAMETERS_END();

    (void)constructor_args;
    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)),
                               MYLITE_MYSQLI_ASSOC, &row);
    if (Z_TYPE(row) != IS_ARRAY) {
        ZVAL_COPY_VALUE(return_value, &row);
        return;
    }

    class_entry = class_name == NULL
                      ? zend_standard_class_def
                      : zend_lookup_class(zend_string_init(class_name, class_name_length, false));
    if (class_entry == NULL) {
        zval_ptr_dtor(&row);
        RETURN_FALSE;
    }
    object_init_ex(return_value, class_entry);
    zend_hash_copy(Z_OBJPROP_P(return_value), Z_ARRVAL(row), zval_add_ref);
    zval_ptr_dtor(&row);
}

PHP_FUNCTION(mysqli_fetch_column)
{
    zval *result_zval = NULL;
    zend_long column = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(column)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch_column(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval)), column,
                                      return_value);
}

PHP_FUNCTION(mysqli_fetch_field)
{
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

PHP_FUNCTION(mysqli_fetch_fields)
{
    zval *result_zval = NULL;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    array_init(return_value);
    for (uint32_t index = 0; index < result->column_count; index++) {
        zval field;

        mylite_mysqli_result_fetch_field(result, index, &field);
        add_next_index_zval(return_value, &field);
    }
}

PHP_FUNCTION(mysqli_fetch_field_direct)
{
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

PHP_FUNCTION(mysqli_fetch_lengths)
{
    zval *result_zval = NULL;
    mylite_mysqli_result *result = NULL;
    uint32_t row_index = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    if (result->cursor == 0 || result->cursor > result->row_count) {
        RETURN_FALSE;
    }

    row_index = result->cursor - 1U;
    array_init(return_value);
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval *value = &result->values[row_index * result->column_count + column];

        add_next_index_long(return_value,
                            Z_TYPE_P(value) == IS_STRING ? (zend_long)Z_STRLEN_P(value) : 0);
    }
}

PHP_FUNCTION(mysqli_data_seek)
{
    zval *result_zval = NULL;
    zend_long offset = 0;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    Z_PARAM_LONG(offset)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval));
    if (offset < 0 || (uint32_t)offset > result->row_count) {
        RETURN_FALSE;
    }
    result->cursor = (uint32_t)offset;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_field_seek)
{
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

PHP_FUNCTION(mysqli_field_tell)
{
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval))->field_cursor);
}

PHP_FUNCTION(mysqli_free_result)
{
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();
}

PHP_FUNCTION(mysqli_num_fields)
{
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval))->column_count);
}

PHP_FUNCTION(mysqli_num_rows)
{
    zval *result_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(result_zval, mylite_mysqli_result_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_result_from_obj(Z_OBJ_P(result_zval))->row_count);
}

PHP_FUNCTION(mysqli_stmt_prepare)
{
    zval *stmt_zval = NULL;
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_prepare_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval)),
                                                    query, query_length));
}

PHP_FUNCTION(mysqli_stmt_execute)
{
    zval *stmt_zval = NULL;
    zval *params = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval)),
                                                    params));
}

PHP_FUNCTION(mysqli_stmt_bind_param)
{
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
        mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_CLIENT, "HY000",
                                     "parameter count mismatch");
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

PHP_FUNCTION(mysqli_stmt_bind_result)
{
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

PHP_FUNCTION(mysqli_stmt_fetch)
{
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (Z_TYPE(stmt->result) != IS_OBJECT) {
        RETURN_FALSE;
    }
    result = mylite_mysqli_result_from_obj(Z_OBJ(stmt->result));
    if (result->cursor >= result->row_count) {
        RETURN_NULL();
    }
    if (stmt->bound_result_count != result->column_count) {
        mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_CLIENT, "HY000",
                                     "result variable count mismatch");
        mylite_mysqli_report_stmt_error(stmt);
        RETURN_FALSE;
    }

    for (uint32_t column = 0; column < result->column_count; column++) {
        zval *target = &stmt->bound_results[column];
        zval *target_value = target;
        zval *source = &result->values[result->cursor * result->column_count + column];
        zval copy;

        ZVAL_COPY(&copy, source);
        ZVAL_DEREF(target_value);
        zval_ptr_dtor(target_value);
        ZVAL_COPY_VALUE(target_value, &copy);
    }
    result->cursor++;
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stmt_get_result)
{
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (Z_TYPE(stmt->result) != IS_OBJECT) {
        RETURN_FALSE;
    }
    ZVAL_COPY(return_value, &stmt->result);
}

PHP_FUNCTION(mysqli_stmt_result_metadata)
{
    ZEND_MN(mysqli_stmt_get_result)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_FUNCTION(mysqli_stmt_store_result)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stmt_free_result)
{
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
        ZVAL_UNDEF(&stmt->result);
    }
}

PHP_FUNCTION(mysqli_stmt_close)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stmt_data_seek)
{
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
        if (offset >= 0 && (uint32_t)offset <= result->row_count) {
            result->cursor = (uint32_t)offset;
        }
    }
}

PHP_FUNCTION(mysqli_stmt_reset)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stmt_attr_get)
{
    zval *stmt_zval = NULL;
    zend_long attribute = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_LONG(attribute)
    ZEND_PARSE_PARAMETERS_END();

    (void)attribute;
    RETURN_LONG(0);
}

PHP_FUNCTION(mysqli_stmt_attr_set)
{
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
    RETURN_TRUE;
}

PHP_FUNCTION(mysqli_stmt_send_long_data)
{
    zval *stmt_zval = NULL;
    char *data = NULL;
    size_t data_length = 0U;
    zend_long param_num = 0;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    Z_PARAM_LONG(param_num)
    Z_PARAM_STRING(data, data_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)param_num;
    (void)data;
    (void)data_length;
    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_stmt_errno)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->error_code);
}

PHP_FUNCTION(mysqli_stmt_error)
{
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    RETURN_STR_COPY(stmt->error);
}

PHP_FUNCTION(mysqli_stmt_error_list)
{
    zval *stmt_zval = NULL;
    mylite_mysqli_stmt *stmt = NULL;
    zval entry;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval));
    array_init(return_value);
    if (stmt->error_code == 0) {
        return;
    }

    array_init(&entry);
    add_assoc_long(&entry, "errno", stmt->error_code);
    add_assoc_string(&entry, "sqlstate", stmt->sqlstate);
    add_assoc_str(&entry, "error", zend_string_copy(stmt->error));
    add_next_index_zval(return_value, &entry);
}

PHP_FUNCTION(mysqli_stmt_sqlstate)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STRING(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->sqlstate);
}

PHP_FUNCTION(mysqli_stmt_field_count)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->field_count);
}

PHP_FUNCTION(mysqli_stmt_affected_rows)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->affected_rows);
}

PHP_FUNCTION(mysqli_stmt_insert_id)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->insert_id);
}

PHP_FUNCTION(mysqli_stmt_num_rows)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->num_rows);
}

PHP_FUNCTION(mysqli_stmt_param_count)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(stmt_zval))->param_count);
}

PHP_FUNCTION(mysqli_stmt_get_warnings)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_stmt_more_results)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_FUNCTION(mysqli_stmt_next_result)
{
    zval *stmt_zval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_OBJECT_OF_CLASS(stmt_zval, mylite_mysqli_stmt_ce)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_FALSE;
}

PHP_METHOD(mysqli, __construct)
{
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
        (void)port;
        (void)port_is_null;

        (void)mylite_mysqli_connect_link(mylite_mysqli_link_from_obj(Z_OBJ_P(object)), host,
                                         host_length, database, database_length, socket,
                                         socket_length);
    }
}

PHP_METHOD(mysqli, connect)
{
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
    (void)port;
    (void)port_is_null;

    RETURN_BOOL(mylite_mysqli_connect_link(mylite_mysqli_link_from_obj(Z_OBJ_P(object)), host,
                                           host_length, database, database_length, socket,
                                           socket_length));
}

PHP_METHOD(mysqli, real_connect)
{
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
    (void)port;
    (void)port_is_null;
    (void)flags;
    RETURN_BOOL(mylite_mysqli_connect_link(mylite_mysqli_link_from_obj(Z_OBJ_P(object)), host,
                                           host_length, database, database_length, socket,
                                           socket_length));
}

PHP_METHOD(mysqli, query)
{
    char *query = NULL;
    size_t query_length = 0U;
    zend_long result_mode = MYLITE_MYSQLI_STORE_RESULT;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(query, query_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(result_mode)
    ZEND_PARSE_PARAMETERS_END();

    (void)result_mode;
    if (!mylite_mysqli_link_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())), query,
                                  query_length, return_value)) {
        RETURN_FALSE;
    }
}

PHP_METHOD(mysqli, execute_query)
{
    char *query = NULL;
    size_t query_length = 0U;
    zval *params = NULL;
    zval *object = getThis();
    zend_string *query_string = NULL;
    zend_string *sql = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(query, query_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    query_string = zend_string_init(query, query_length, false);
    sql = params == NULL ? query_string
                         : mylite_mysqli_interpolate_query(
                               query_string, params, zend_hash_num_elements(Z_ARRVAL_P(params)));
    if (sql == NULL) {
        RETURN_FALSE;
    }
    if (object == NULL) {
        zend_string_release(sql);
        RETURN_FALSE;
    }
    if (!mylite_mysqli_link_query(mylite_mysqli_link_from_obj(Z_OBJ_P(object)), ZSTR_VAL(sql),
                                  ZSTR_LEN(sql), return_value)) {
        zend_string_release(sql);
        RETURN_FALSE;
    }
    zend_string_release(sql);
}

PHP_METHOD(mysqli, real_query)
{
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                              query, query_length));
}

PHP_METHOD(mysqli, store_result)
{
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (Z_TYPE(link->last_result) == IS_OBJECT) {
        ZVAL_COPY(return_value, &link->last_result);
        return;
    }
    RETURN_FALSE;
}

PHP_METHOD(mysqli, use_result)
{
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (Z_TYPE(link->last_result) == IS_OBJECT) {
        ZVAL_COPY(return_value, &link->last_result);
        return;
    }
    RETURN_FALSE;
}

PHP_METHOD(mysqli, close)
{
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (link->database != NULL) {
        mylite_close(link->database);
        link->database = NULL;
    }
    link->connected = false;
    mylite_mysqli_update_link_properties(link);
    RETURN_TRUE;
}

PHP_METHOD(mysqli, prepare)
{
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

PHP_METHOD(mysqli, stmt_init)
{
    mylite_mysqli_stmt *stmt = NULL;

    ZEND_PARSE_PARAMETERS_NONE();
    object_init_ex(return_value, mylite_mysqli_stmt_ce);
    stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(return_value));
    ZVAL_OBJ(&stmt->link, Z_OBJ_P(getThis()));
    Z_ADDREF(stmt->link);
    mylite_mysqli_update_stmt_properties(stmt);
}

PHP_METHOD(mysqli, autocommit)
{
    bool enable = false;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    (void)enable;
    RETURN_TRUE;
}

PHP_METHOD(mysqli, begin_transaction)
{
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)flags;
    (void)name;
    (void)name_length;
    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                              "START TRANSACTION", strlen("START TRANSACTION")));
}

PHP_METHOD(mysqli, commit)
{
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)flags;
    (void)name;
    (void)name_length;
    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                              "COMMIT", strlen("COMMIT")));
}

PHP_METHOD(mysqli, rollback)
{
    zend_long flags = 0;
    char *name = NULL;
    size_t name_length = 0U;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(flags)
    Z_PARAM_STRING_OR_NULL(name, name_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)flags;
    (void)name;
    (void)name_length;
    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                              "ROLLBACK", strlen("ROLLBACK")));
}

PHP_METHOD(mysqli, savepoint)
{
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
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                       ZSTR_VAL(sql), ZSTR_LEN(sql));
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, release_savepoint)
{
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
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                       ZSTR_VAL(sql), ZSTR_LEN(sql));
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, change_user)
{
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
    bool ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(object)),
                                            ZSTR_VAL(sql), ZSTR_LEN(sql));

    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, character_set_name)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING("utf8mb4");
}

PHP_METHOD(mysqli, get_charset)
{
    ZEND_PARSE_PARAMETERS_NONE();
    object_init(return_value);
    add_property_string(return_value, "charset", "utf8mb4");
    add_property_string(return_value, "collation", "utf8mb4_0900_ai_ci");
    add_property_long(return_value, "number", 255);
}

PHP_METHOD(mysqli, get_client_info)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING("mylite mysqli " PHP_MYLITE_MYSQLI_VERSION);
}

PHP_METHOD(mysqli, get_connection_stats)
{
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
}

PHP_METHOD(mysqli, get_server_info)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(MYLITE_MYSQLI_SERVER_INFO);
}

PHP_METHOD(mysqli, get_warnings)
{
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()));
    const char *message = NULL;

    ZEND_PARSE_PARAMETERS_NONE();
    if (link->database == NULL || link->warning_count <= 0) {
        RETURN_FALSE;
    }
    object_init_ex(return_value, mylite_mysqli_warning_ce);
    message = "MyLite warning";
    zend_update_property_string(mylite_mysqli_warning_ce, Z_OBJ_P(return_value), "message",
                                strlen("message"), message == NULL ? "" : message);
    zend_update_property_string(mylite_mysqli_warning_ce, Z_OBJ_P(return_value), "sqlstate",
                                strlen("sqlstate"), "HY000");
    zend_update_property_long(mylite_mysqli_warning_ce, Z_OBJ_P(return_value), "errno",
                              strlen("errno"), 0);
}

PHP_METHOD(mysqli, init)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(mysqli, kill)
{
    zend_long process_id = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(process_id)
    ZEND_PARSE_PARAMETERS_END();

    (void)process_id;
    RETURN_FALSE;
}

PHP_METHOD(mysqli, multi_query)
{
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                              query, query_length));
}

PHP_METHOD(mysqli, more_results)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli, next_result)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli, ping)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis()))->connected);
}

PHP_METHOD(mysqli, poll)
{
    zif_mysqli_poll(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(mysqli, real_escape_string)
{
    char *value = NULL;
    size_t value_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STR(mylite_mysqli_escape_string(value, value_length));
}

PHP_METHOD(mysqli, reap_async_query)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli, escape_string)
{
    char *value = NULL;
    size_t value_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STR(mylite_mysqli_escape_string(value, value_length));
}

PHP_METHOD(mysqli, select_db)
{
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
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                       ZSTR_VAL(sql), ZSTR_LEN(sql));
    zend_string_release(sql);
    zend_string_release(quoted);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, set_charset)
{
    char *value = NULL;
    size_t value_length = 0U;
    zend_string *sql = NULL;
    bool ok = false;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(value, value_length)
    ZEND_PARSE_PARAMETERS_END();

    sql = zend_strpprintf(0, "SET NAMES %.*s", (int)value_length, value);
    ok = mylite_mysqli_link_real_query(mylite_mysqli_link_from_obj(Z_OBJ_P(getThis())),
                                       ZSTR_VAL(sql), ZSTR_LEN(sql));
    zend_string_release(sql);
    RETURN_BOOL(ok);
}

PHP_METHOD(mysqli, options)
{
    zend_long option = 0;
    zval *value = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(option)
    Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)option;
    (void)value;
    RETURN_TRUE;
}

PHP_METHOD(mysqli, set_opt)
{
    zend_long option = 0;
    zval *value = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(option)
    Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)option;
    (void)value;
    RETURN_TRUE;
}

PHP_METHOD(mysqli, ssl_set)
{
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
    RETURN_TRUE;
}

PHP_METHOD(mysqli, stat)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING("MyLite embedded mysqli connection");
}

PHP_METHOD(mysqli, thread_safe)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_METHOD(mysqli, dump_debug_info)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_METHOD(mysqli, debug)
{
    char *options = NULL;
    size_t options_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(options, options_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)options;
    (void)options_length;
    RETURN_TRUE;
}

PHP_METHOD(mysqli, refresh)
{
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    (void)flags;
    RETURN_FALSE;
}

PHP_METHOD(mysqli_result, __construct)
{
    zend_throw_error(NULL, "mysqli_result objects are created by query execution");
}

PHP_METHOD(mysqli_result, close)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(mysqli_result, free)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(mysqli_result, free_result)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(mysqli_result, data_seek)
{
    zend_long offset = 0;
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(offset)
    ZEND_PARSE_PARAMETERS_END();

    if (offset < 0 || (uint32_t)offset > result->row_count) {
        RETURN_FALSE;
    }
    result->cursor = (uint32_t)offset;
    RETURN_TRUE;
}

PHP_METHOD(mysqli_result, fetch_assoc)
{
    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())),
                               MYLITE_MYSQLI_ASSOC, return_value);
}

PHP_METHOD(mysqli_result, fetch_row)
{
    ZEND_PARSE_PARAMETERS_NONE();
    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())), MYLITE_MYSQLI_NUM,
                               return_value);
}

PHP_METHOD(mysqli_result, fetch_array)
{
    zend_long mode = MYLITE_MYSQLI_BOTH;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())), (int)mode,
                               return_value);
}

PHP_METHOD(mysqli_result, fetch_all)
{
    zend_long mode = MYLITE_MYSQLI_NUM;
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));
    array_init(return_value);
    while (result->cursor < result->row_count) {
        zval row;

        mylite_mysqli_result_fetch(result, (int)mode, &row);
        add_next_index_zval(return_value, &row);
    }
}

PHP_METHOD(mysqli_result, fetch_object)
{
    char *class_name = NULL;
    size_t class_name_length = 0U;
    zval *constructor_args = NULL;
    zval row;
    zend_class_entry *class_entry = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING(class_name, class_name_length)
    Z_PARAM_ARRAY(constructor_args)
    ZEND_PARSE_PARAMETERS_END();

    (void)constructor_args;
    mylite_mysqli_result_fetch(mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())),
                               MYLITE_MYSQLI_ASSOC, &row);
    if (Z_TYPE(row) != IS_ARRAY) {
        ZVAL_COPY_VALUE(return_value, &row);
        return;
    }

    if (class_name == NULL) {
        class_entry = zend_standard_class_def;
    } else {
        zend_string *lookup_name = zend_string_init(class_name, class_name_length, false);

        class_entry = zend_lookup_class(lookup_name);
        zend_string_release(lookup_name);
    }
    if (class_entry == NULL) {
        zval_ptr_dtor(&row);
        RETURN_FALSE;
    }
    object_init_ex(return_value, class_entry);
    zend_hash_copy(Z_OBJPROP_P(return_value), Z_ARRVAL(row), zval_add_ref);
    zval_ptr_dtor(&row);
}

PHP_METHOD(mysqli_result, fetch_column)
{
    zend_long column = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(column)
    ZEND_PARSE_PARAMETERS_END();

    mylite_mysqli_result_fetch_column(mylite_mysqli_result_from_obj(Z_OBJ_P(getThis())), column,
                                      return_value);
}

PHP_METHOD(mysqli_result, fetch_field)
{
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (result->field_cursor >= result->column_count) {
        RETURN_FALSE;
    }
    mylite_mysqli_result_fetch_field(result, result->field_cursor, return_value);
    result->field_cursor++;
    mylite_mysqli_update_result_properties(result);
}

PHP_METHOD(mysqli_result, fetch_fields)
{
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
    for (uint32_t index = 0; index < result->column_count; index++) {
        zval field;

        mylite_mysqli_result_fetch_field(result, index, &field);
        add_next_index_zval(return_value, &field);
    }
}

PHP_METHOD(mysqli_result, fetch_field_direct)
{
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

PHP_METHOD(mysqli_result, field_seek)
{
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

PHP_METHOD(mysqli_result, getIterator)
{
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    result->cursor = 0;
    array_init(return_value);
    while (result->cursor < result->row_count) {
        zval row;

        mylite_mysqli_result_fetch(result, MYLITE_MYSQLI_ASSOC, &row);
        add_next_index_zval(return_value, &row);
    }
    zend_create_internal_iterator_zval(return_value, return_value);
}

PHP_METHOD(mysqli_stmt, __construct)
{
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

PHP_METHOD(mysqli_stmt, prepare)
{
    char *query = NULL;
    size_t query_length = 0U;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(query, query_length)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_prepare_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis())),
                                                    query, query_length));
}

PHP_METHOD(mysqli_stmt, execute)
{
    zval *params = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(params)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis())),
                                                    params));
}

PHP_METHOD(mysqli_stmt, bind_param)
{
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
        mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_CLIENT, "HY000",
                                     "parameter count mismatch");
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

PHP_METHOD(mysqli_stmt, bind_result)
{
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

PHP_METHOD(mysqli_stmt, fetch)
{
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));
    mylite_mysqli_result *result = NULL;

    ZEND_PARSE_PARAMETERS_NONE();
    if (Z_TYPE(stmt->result) != IS_OBJECT) {
        RETURN_FALSE;
    }
    result = mylite_mysqli_result_from_obj(Z_OBJ(stmt->result));
    if (result->cursor >= result->row_count) {
        RETURN_NULL();
    }
    if (stmt->bound_result_count != result->column_count) {
        mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_CLIENT, "HY000",
                                     "result variable count mismatch");
        mylite_mysqli_report_stmt_error(stmt);
        RETURN_FALSE;
    }
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval *target = &stmt->bound_results[column];
        zval *target_value = target;
        zval *source = &result->values[result->cursor * result->column_count + column];
        zval copy;

        ZVAL_COPY(&copy, source);
        ZVAL_DEREF(target_value);
        zval_ptr_dtor(target_value);
        ZVAL_COPY_VALUE(target_value, &copy);
    }
    result->cursor++;
    RETURN_TRUE;
}

PHP_METHOD(mysqli_stmt, get_result)
{
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (Z_TYPE(stmt->result) != IS_OBJECT) {
        RETURN_FALSE;
    }
    ZVAL_COPY(return_value, &stmt->result);
}

PHP_METHOD(mysqli_stmt, result_metadata)
{
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (Z_TYPE(stmt->result) != IS_OBJECT) {
        RETURN_FALSE;
    }
    ZVAL_COPY(return_value, &stmt->result);
}

PHP_METHOD(mysqli_stmt, store_result)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_METHOD(mysqli_stmt, free_result)
{
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_NONE();
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
        ZVAL_UNDEF(&stmt->result);
    }
}

PHP_METHOD(mysqli_stmt, close)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_METHOD(mysqli_stmt, data_seek)
{
    zend_long offset = 0;
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()));

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(offset)
    ZEND_PARSE_PARAMETERS_END();

    if (Z_TYPE(stmt->result) == IS_OBJECT) {
        mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ(stmt->result));

        if (offset >= 0 && (uint32_t)offset <= result->row_count) {
            result->cursor = (uint32_t)offset;
        }
    }
}

PHP_METHOD(mysqli_stmt, reset)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_METHOD(mysqli_stmt, attr_get)
{
    zend_long attribute = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(attribute)
    ZEND_PARSE_PARAMETERS_END();

    (void)attribute;
    RETURN_LONG(0);
}

PHP_METHOD(mysqli_stmt, attr_set)
{
    zend_long attribute = 0;
    zend_long value = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(attribute)
    Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    (void)attribute;
    (void)value;
    RETURN_TRUE;
}

PHP_METHOD(mysqli_stmt, send_long_data)
{
    zend_long param_num = 0;
    char *data = NULL;
    size_t data_length = 0U;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(param_num)
    Z_PARAM_STRING(data, data_length)
    ZEND_PARSE_PARAMETERS_END();

    (void)param_num;
    (void)data;
    (void)data_length;
    RETURN_FALSE;
}

PHP_METHOD(mysqli_stmt, get_warnings)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli_stmt, more_results)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli_stmt, next_result)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli_stmt, num_rows)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(mylite_mysqli_stmt_from_obj(Z_OBJ_P(getThis()))->num_rows);
}

PHP_METHOD(mysqli_warning, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(mysqli_warning, next)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_FALSE;
}

PHP_METHOD(mysqli_sql_exception, getSqlState)
{
    zval *property = zend_read_property(mylite_mysqli_exception_ce, Z_OBJ_P(getThis()), "sqlstate",
                                        strlen("sqlstate"), true, NULL);

    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_ZVAL(property, true, false);
}

static zend_object *mylite_mysqli_link_create(zend_class_entry *class_entry)
{
    mylite_mysqli_link *link = zend_object_alloc(sizeof(*link), class_entry);

    zend_object_std_init(&link->std, class_entry);
    object_properties_init(&link->std, class_entry);
    link->std.handlers = &mylite_mysqli_link_handlers;
    link->error = zend_empty_string;
    zend_string_addref(link->error);
    memcpy(link->sqlstate, "00000", sizeof(link->sqlstate));
    link->affected_rows = -1;
    ZVAL_UNDEF(&link->last_result);
    mylite_mysqli_update_link_properties(link);
    return &link->std;
}

static void mylite_mysqli_link_free(zend_object *object)
{
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(object);

    if (link->database != NULL) {
        mylite_close(link->database);
    }
    if (link->path != NULL) {
        zend_string_release(link->path);
    }
    zend_string_release(link->error);
    if (!Z_ISUNDEF(link->last_result)) {
        zval_ptr_dtor(&link->last_result);
    }
    zend_object_std_dtor(&link->std);
}

static zend_object *mylite_mysqli_result_create(zend_class_entry *class_entry)
{
    mylite_mysqli_result *result = zend_object_alloc(sizeof(*result), class_entry);

    zend_object_std_init(&result->std, class_entry);
    object_properties_init(&result->std, class_entry);
    result->std.handlers = &mylite_mysqli_result_handlers;
    mylite_mysqli_update_result_properties(result);
    return &result->std;
}

static void mylite_mysqli_result_free(zend_object *object)
{
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(object);

    for (uint32_t column = 0; column < result->column_count; column++) {
        if (result->names != NULL && result->names[column] != NULL) {
            zend_string_release(result->names[column]);
        }
        if (result->schemas != NULL && result->schemas[column] != NULL) {
            zend_string_release(result->schemas[column]);
        }
        if (result->tables != NULL && result->tables[column] != NULL) {
            zend_string_release(result->tables[column]);
        }
        if (result->origin_tables != NULL && result->origin_tables[column] != NULL) {
            zend_string_release(result->origin_tables[column]);
        }
        if (result->origin_names != NULL && result->origin_names[column] != NULL) {
            zend_string_release(result->origin_names[column]);
        }
    }
    for (uint32_t index = 0; index < result->row_count * result->column_count; index++) {
        zval_ptr_dtor(&result->values[index]);
    }
    efree(result->names);
    efree(result->schemas);
    efree(result->tables);
    efree(result->origin_tables);
    efree(result->origin_names);
    efree(result->types);
    efree(result->flags);
    efree(result->lengths);
    efree(result->max_lengths);
    efree(result->decimals);
    efree(result->charsets);
    efree(result->nullable);
    efree(result->values);
    zend_object_std_dtor(&result->std);
}

static zend_object *mylite_mysqli_stmt_create(zend_class_entry *class_entry)
{
    mylite_mysqli_stmt *stmt = zend_object_alloc(sizeof(*stmt), class_entry);

    zend_object_std_init(&stmt->std, class_entry);
    object_properties_init(&stmt->std, class_entry);
    stmt->std.handlers = &mylite_mysqli_stmt_handlers;
    stmt->error = zend_empty_string;
    zend_string_addref(stmt->error);
    stmt->affected_rows = -1;
    memcpy(stmt->sqlstate, "00000", sizeof(stmt->sqlstate));
    ZVAL_UNDEF(&stmt->link);
    ZVAL_UNDEF(&stmt->result);
    mylite_mysqli_update_stmt_properties(stmt);
    return &stmt->std;
}

static void mylite_mysqli_stmt_free(zend_object *object)
{
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(object);

    if (!Z_ISUNDEF(stmt->link)) {
        zval_ptr_dtor(&stmt->link);
    }
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
    }
    if (stmt->bound_params != NULL) {
        for (uint32_t index = 0; index < stmt->bound_param_count; index++) {
            zval_ptr_dtor(&stmt->bound_params[index]);
        }
        efree(stmt->bound_params);
    }
    if (stmt->bound_results != NULL) {
        for (uint32_t index = 0; index < stmt->bound_result_count; index++) {
            zval_ptr_dtor(&stmt->bound_results[index]);
        }
        efree(stmt->bound_results);
    }
    if (stmt->query != NULL) {
        zend_string_release(stmt->query);
    }
    if (stmt->types != NULL) {
        zend_string_release(stmt->types);
    }
    zend_string_release(stmt->error);
    zend_object_std_dtor(&stmt->std);
}

static zend_object *mylite_mysqli_warning_create(zend_class_entry *class_entry)
{
    mylite_mysqli_warning *warning = zend_object_alloc(sizeof(*warning), class_entry);

    zend_object_std_init(&warning->std, class_entry);
    object_properties_init(&warning->std, class_entry);
    warning->std.handlers = &mylite_mysqli_warning_handlers;
    return &warning->std;
}

static void mylite_mysqli_warning_free(zend_object *object)
{
    zend_object_std_dtor(object);
}

static mylite_mysqli_link *mylite_mysqli_link_from_obj(zend_object *object)
{
    return (mylite_mysqli_link *)((char *)object - XtOffsetOf(mylite_mysqli_link, std));
}

static mylite_mysqli_result *mylite_mysqli_result_from_obj(zend_object *object)
{
    return (mylite_mysqli_result *)((char *)object - XtOffsetOf(mylite_mysqli_result, std));
}

static mylite_mysqli_stmt *mylite_mysqli_stmt_from_obj(zend_object *object)
{
    return (mylite_mysqli_stmt *)((char *)object - XtOffsetOf(mylite_mysqli_stmt, std));
}

static bool mylite_mysqli_connect_link(mylite_mysqli_link *link, const char *host,
                                       size_t host_length, const char *database,
                                       size_t database_length, const char *socket,
                                       size_t socket_length)
{
    bool memory = false;
    bool use_database = false;
    zend_string *path = mylite_mysqli_resolve_path(host, host_length, database, database_length,
                                                   socket, socket_length, &memory, &use_database);
    int status = MYLITE_OK;

    mylite_mysqli_clear_error(link);
    if (path == NULL) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        mylite_mysqli_set_global_connect_error(link->error_code, ZSTR_VAL(link->error));
        mylite_mysqli_report_link_error(link);
        return false;
    }

    if (link->database != NULL) {
        mylite_close(link->database);
        link->database = NULL;
    }
    link->connected = false;
    if (link->path != NULL) {
        zend_string_release(link->path);
    }
    link->path = zend_string_copy(path);

    status =
        memory ? mylite_open_memory(&link->database) : mylite_open(ZSTR_VAL(path), &link->database);
    zend_string_release(path);
    if (status != MYLITE_OK) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CONNECTION, "HY000",
                                "failed to open MyLite database");
        mylite_mysqli_set_global_connect_error(link->error_code, ZSTR_VAL(link->error));
        mylite_mysqli_report_link_error(link);
        mylite_mysqli_update_link_properties(link);
        return false;
    }

    link->connected = true;
    mylite_mysqli_set_global_connect_error(0, "");
    if (use_database && database != NULL) {
        zend_string *quoted = mylite_mysqli_quote_identifier(database, database_length);
        zend_string *sql = zend_strpprintf(0, "USE %s", ZSTR_VAL(quoted));
        bool ok = mylite_mysqli_link_real_query(link, ZSTR_VAL(sql), ZSTR_LEN(sql));

        zend_string_release(sql);
        zend_string_release(quoted);
        if (!ok) {
            mylite_mysqli_set_global_connect_error(link->error_code, ZSTR_VAL(link->error));
            mylite_close(link->database);
            link->database = NULL;
            link->connected = false;
            mylite_mysqli_update_link_properties(link);
            return false;
        }
    }

    mylite_mysqli_update_link_properties(link);
    return true;
}

static bool mylite_mysqli_link_query(mylite_mysqli_link *link, const char *sql, size_t sql_length,
                                     zval *out_result)
{
    if (!mylite_mysqli_execute_sql(link, sql, sql_length, out_result)) {
        return false;
    }

    if (Z_TYPE_P(out_result) == IS_OBJECT) {
        if (!Z_ISUNDEF(link->last_result)) {
            zval_ptr_dtor(&link->last_result);
        }
        ZVAL_COPY(&link->last_result, out_result);
    }
    return true;
}

static bool mylite_mysqli_link_real_query(mylite_mysqli_link *link, const char *sql,
                                          size_t sql_length)
{
    zval result;
    bool ok = mylite_mysqli_execute_sql(link, sql, sql_length, &result);

    if (!ok) {
        return false;
    }
    if (!Z_ISUNDEF(link->last_result)) {
        zval_ptr_dtor(&link->last_result);
        ZVAL_UNDEF(&link->last_result);
    }
    if (Z_TYPE(result) == IS_OBJECT) {
        ZVAL_COPY_VALUE(&link->last_result, &result);
    } else {
        zval_ptr_dtor(&result);
    }
    return true;
}

static bool mylite_mysqli_stmt_prepare_internal(mylite_mysqli_stmt *stmt, const char *sql,
                                                size_t sql_length)
{
    if (stmt->query != NULL) {
        zend_string_release(stmt->query);
    }
    stmt->query = zend_string_init(sql, sql_length, false);
    stmt->param_count = mylite_mysqli_count_markers(sql, sql_length);
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

static bool mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt *stmt, zval *params)
{
    mylite_mysqli_link *link = NULL;
    zend_string *sql = NULL;
    zval result;

    if (Z_ISUNDEF(stmt->link) || Z_TYPE(stmt->link) != IS_OBJECT || stmt->query == NULL) {
        mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_CLIENT, "HY000",
                                     "statement is not prepared");
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }

    link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));
    if (params != NULL) {
        sql = mylite_mysqli_interpolate_query(zend_string_copy(stmt->query), params,
                                              zend_hash_num_elements(Z_ARRVAL_P(params)));
    } else if (stmt->bound_param_count > 0U) {
        zval params_array;

        array_init(&params_array);
        for (uint32_t index = 0; index < stmt->bound_param_count; index++) {
            zval value;

            ZVAL_COPY(&value, &stmt->bound_params[index]);
            add_next_index_zval(&params_array, &value);
        }
        sql = mylite_mysqli_interpolate_query(zend_string_copy(stmt->query), &params_array,
                                              stmt->bound_param_count);
        zval_ptr_dtor(&params_array);
    } else {
        sql = zend_string_copy(stmt->query);
    }

    if (sql == NULL) {
        mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_CLIENT, "HY000",
                                     "parameter count mismatch");
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }

    if (!mylite_mysqli_execute_sql(link, ZSTR_VAL(sql), ZSTR_LEN(sql), &result)) {
        mylite_mysqli_set_stmt_error(stmt, link->error_code, link->sqlstate, ZSTR_VAL(link->error));
        zend_string_release(sql);
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }
    zend_string_release(sql);

    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
        ZVAL_UNDEF(&stmt->result);
    }
    if (Z_TYPE(result) == IS_OBJECT) {
        mylite_mysqli_result *result_object = mylite_mysqli_result_from_obj(Z_OBJ(result));

        stmt->num_rows = result_object->row_count;
        stmt->field_count = result_object->column_count;
        ZVAL_COPY_VALUE(&stmt->result, &result);
    } else {
        zval_ptr_dtor(&result);
        stmt->num_rows = 0;
        stmt->field_count = 0;
    }
    stmt->affected_rows = link->affected_rows;
    stmt->insert_id = link->insert_id;
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

static bool mylite_mysqli_execute_sql(mylite_mysqli_link *link, const char *sql, size_t sql_length,
                                      zval *out_result)
{
    mylite_result *source = NULL;
    const char *sqlstate = "HY000";
    int status = MYLITE_OK;

    ZVAL_UNDEF(out_result);
    if (link->database == NULL) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CONNECTION, "HY000",
                                "mysqli object is not connected");
        mylite_mysqli_report_link_error(link);
        return false;
    }

    mylite_mysqli_clear_error(link);
    status = mylite_execute(link->database, sql, sql_length, &source);
    if (status != MYLITE_OK) {
        int error_code = mylite_errcode(link->database);

        if (error_code == 0) {
            error_code = mylite_mysqli_error_from_status(status, &sqlstate);
        } else {
            sqlstate = mylite_sqlstate(link->database);
        }
        mylite_mysqli_set_error(link, error_code, sqlstate, mylite_errmsg(link->database));
        mylite_mysqli_report_link_error(link);
        return false;
    }

    if (!mylite_mysqli_buffer_result(link, source, out_result)) {
        mylite_result_free(source);
        return false;
    }
    mylite_result_free(source);
    mylite_mysqli_update_link_properties(link);
    return true;
}

static bool mylite_mysqli_buffer_result(mylite_mysqli_link *link, const mylite_result *source,
                                        zval *out_result)
{
    size_t column_count = mylite_result_column_count(source);
    size_t row_count = mylite_result_row_count(source);

    if (column_count > (size_t)UINT32_MAX || row_count > (size_t)UINT32_MAX ||
        (column_count != 0U && row_count > SIZE_MAX / column_count)) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000",
                                "result set is too large");
        mylite_mysqli_report_link_error(link);
        return false;
    }

    link->field_count = (zend_long)column_count;
    link->warning_count = (zend_long)mylite_result_warning_count(source);
    if (column_count == 0U) {
        link->affected_rows = (zend_long)mylite_result_affected_rows(source);
        link->insert_id = 0;
        ZVAL_TRUE(out_result);
        return true;
    }

    object_init_ex(out_result, mylite_mysqli_result_ce);
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(out_result));
    result->column_count = (uint32_t)column_count;
    result->row_count = (uint32_t)row_count;
    result->row_capacity = result->row_count;
    result->names = ecalloc(result->column_count, sizeof(zend_string *));
    result->schemas = ecalloc(result->column_count, sizeof(zend_string *));
    result->tables = ecalloc(result->column_count, sizeof(zend_string *));
    result->origin_tables = ecalloc(result->column_count, sizeof(zend_string *));
    result->origin_names = ecalloc(result->column_count, sizeof(zend_string *));
    result->types = ecalloc(result->column_count, sizeof(int));
    result->flags = ecalloc(result->column_count, sizeof(unsigned int));
    result->lengths = ecalloc(result->column_count, sizeof(uint64_t));
    result->max_lengths = ecalloc(result->column_count, sizeof(uint64_t));
    result->decimals = ecalloc(result->column_count, sizeof(unsigned int));
    result->charsets = ecalloc(result->column_count, sizeof(unsigned int));
    result->nullable = ecalloc(result->column_count, sizeof(bool));
    if (row_count > 0U) {
        result->values = ecalloc(row_count * column_count, sizeof(zval));
    }

    for (uint32_t column = 0; column < result->column_count; column++) {
        const char *name = mylite_result_column_name(source, column);

        result->names[column] =
            zend_string_init(name == NULL ? "" : name, name == NULL ? 0U : strlen(name), false);
        result->types[column] = MYLITE_MYSQLI_FIELD_TYPE_VAR_STRING;
        result->charsets[column] = 255U;
        result->nullable[column] = true;
    }

    for (uint32_t row = 0; row < result->row_count; row++) {
        for (uint32_t column = 0; column < result->column_count; column++) {
            zval *value = &result->values[(size_t)row * result->column_count + column];
            const char *text = mylite_result_value_text(source, row, column);

            if (text == NULL) {
                ZVAL_NULL(value);
            } else {
                size_t text_length = strlen(text);

                if (text_length > result->max_lengths[column]) {
                    result->max_lengths[column] = text_length;
                }
                ZVAL_STRING(value, text);
            }
        }
    }

    link->affected_rows = -1;
    link->insert_id = 0;
    mylite_mysqli_update_result_properties(result);
    return true;
}

static void mylite_mysqli_result_fetch(mylite_mysqli_result *result, int mode, zval *return_value)
{
    if (result->cursor >= result->row_count) {
        RETURN_NULL();
    }

    array_init(return_value);
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval *source = &result->values[result->cursor * result->column_count + column];
        zval value;

        if ((mode & MYLITE_MYSQLI_NUM) != 0) {
            ZVAL_COPY(&value, source);
            add_next_index_zval(return_value, &value);
        }
        if ((mode & MYLITE_MYSQLI_ASSOC) != 0) {
            ZVAL_COPY(&value, source);
            add_assoc_zval_ex(return_value, ZSTR_VAL(result->names[column]),
                              ZSTR_LEN(result->names[column]), &value);
        }
    }
    result->cursor++;
}

static void mylite_mysqli_result_fetch_column(mylite_mysqli_result *result, zend_long column,
                                              zval *return_value)
{
    if (column < 0 || (uint32_t)column >= result->column_count) {
        RETURN_FALSE;
    }
    if (result->cursor >= result->row_count) {
        RETURN_NULL();
    }

    ZVAL_COPY(return_value,
              &result->values[result->cursor * result->column_count + (uint32_t)column]);
    result->cursor++;
}

static void mylite_mysqli_result_fetch_field(mylite_mysqli_result *result, uint32_t index,
                                             zval *return_value)
{
    object_init(return_value);
    add_property_str(return_value, "name", zend_string_copy(result->names[index]));
    add_property_str(return_value, "orgname",
                     result->origin_names[index] == NULL
                         ? ZSTR_EMPTY_ALLOC()
                         : zend_string_copy(result->origin_names[index]));
    add_property_str(return_value, "table",
                     result->tables[index] == NULL ? ZSTR_EMPTY_ALLOC()
                                                   : zend_string_copy(result->tables[index]));
    add_property_str(return_value, "orgtable",
                     result->origin_tables[index] == NULL
                         ? ZSTR_EMPTY_ALLOC()
                         : zend_string_copy(result->origin_tables[index]));
    add_property_str(return_value, "db",
                     result->schemas[index] == NULL ? ZSTR_EMPTY_ALLOC()
                                                    : zend_string_copy(result->schemas[index]));
    add_property_string(return_value, "catalog", "def");
    add_property_string(return_value, "def", "");
    add_property_long(return_value, "flags", (zend_long)result->flags[index]);
    add_property_long(return_value, "type", result->types[index]);
    add_property_long(return_value, "length", (zend_long)result->lengths[index]);
    add_property_long(return_value, "max_length", (zend_long)result->max_lengths[index]);
    add_property_long(return_value, "decimals", (zend_long)result->decimals[index]);
    add_property_long(return_value, "charsetnr", (zend_long)result->charsets[index]);
}

static zend_string *mylite_mysqli_interpolate_query(zend_string *query, zval *params,
                                                    uint32_t param_count)
{
    smart_str sql = {0};
    uint32_t param_index = 0U;
    const char *text = ZSTR_VAL(query);
    size_t length = ZSTR_LEN(query);
    char quote = '\0';
    bool line_comment = false;
    bool block_comment = false;

    for (size_t index = 0U; index < length; index++) {
        char ch = text[index];

        if (line_comment) {
            smart_str_appendc(&sql, ch);
            if (mylite_mysqli_is_line_comment_terminator(ch)) {
                line_comment = false;
            }
            continue;
        }
        if (block_comment) {
            smart_str_appendc(&sql, ch);
            if (ch == '*' && index + 1U < length && text[index + 1U] == '/') {
                index++;
                smart_str_appendc(&sql, text[index]);
                block_comment = false;
            }
            continue;
        }
        if (quote != '\0') {
            smart_str_appendc(&sql, ch);
            if (ch == '\\' && index + 1U < length) {
                index++;
                smart_str_appendc(&sql, text[index]);
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"' || ch == '`') {
            quote = ch;
            smart_str_appendc(&sql, ch);
            continue;
        }
        if (ch == '#' || mylite_mysqli_is_dash_comment_start(text, length, index)) {
            line_comment = true;
            smart_str_appendc(&sql, ch);
            if (ch == '-') {
                index++;
                smart_str_appendc(&sql, text[index]);
            }
            continue;
        }
        if (ch == '/' && index + 1U < length && text[index + 1U] == '*') {
            block_comment = true;
            smart_str_appendc(&sql, ch);
            index++;
            smart_str_appendc(&sql, text[index]);
            continue;
        }
        if (ch == '?' && param_index < param_count) {
            zval *parameter = zend_hash_index_find(Z_ARRVAL_P(params), param_index);
            zend_string *literal = NULL;

            if (parameter == NULL) {
                smart_str_free(&sql);
                zend_string_release(query);
                return NULL;
            }
            literal = mylite_mysqli_param_to_sql(parameter);
            smart_str_append(&sql, literal);
            zend_string_release(literal);
            param_index++;
            continue;
        }
        smart_str_appendc(&sql, ch);
    }

    zend_string_release(query);
    if (param_index != param_count) {
        smart_str_free(&sql);
        return NULL;
    }

    smart_str_0(&sql);
    return sql.s;
}

static zend_string *mylite_mysqli_param_to_sql(zval *value)
{
    zval copy;
    zval *copy_value = &copy;

    ZVAL_COPY(&copy, value);
    ZVAL_DEREF(copy_value);

    switch (Z_TYPE_P(copy_value)) {
    case IS_NULL:
        zval_ptr_dtor(&copy);
        return zend_string_init("NULL", strlen("NULL"), false);
    case IS_FALSE:
        zval_ptr_dtor(&copy);
        return zend_string_init("0", strlen("0"), false);
    case IS_TRUE:
        zval_ptr_dtor(&copy);
        return zend_string_init("1", strlen("1"), false);
    case IS_LONG: {
        zend_string *result = zend_strpprintf(0, ZEND_LONG_FMT, Z_LVAL_P(copy_value));
        zval_ptr_dtor(&copy);
        return result;
    }
    case IS_DOUBLE: {
        zend_string *result = zend_strpprintf(0, "%.*G", (int)EG(precision), Z_DVAL_P(copy_value));
        zval_ptr_dtor(&copy);
        return result;
    }
    default: {
        zend_string *string_value = zval_get_string(copy_value);
        zend_string *escaped =
            mylite_mysqli_escape_string(ZSTR_VAL(string_value), ZSTR_LEN(string_value));
        zend_string *result = zend_strpprintf(0, "'%s'", ZSTR_VAL(escaped));

        zend_string_release(escaped);
        zend_string_release(string_value);
        zval_ptr_dtor(&copy);
        return result;
    }
    }
}

static uint32_t mylite_mysqli_count_markers(const char *sql, size_t sql_length)
{
    uint32_t count = 0U;
    char quote = '\0';
    bool line_comment = false;
    bool block_comment = false;

    for (size_t index = 0U; index < sql_length; index++) {
        char ch = sql[index];

        if (line_comment) {
            if (mylite_mysqli_is_line_comment_terminator(ch)) {
                line_comment = false;
            }
            continue;
        }
        if (block_comment) {
            if (ch == '*' && index + 1U < sql_length && sql[index + 1U] == '/') {
                index++;
                block_comment = false;
            }
            continue;
        }
        if (quote != '\0') {
            if (ch == '\\' && index + 1U < sql_length) {
                index++;
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '\'' || ch == '"' || ch == '`') {
            quote = ch;
        } else if (ch == '#' || mylite_mysqli_is_dash_comment_start(sql, sql_length, index)) {
            if (ch == '-') {
                index++;
            }
            line_comment = true;
        } else if (ch == '/' && index + 1U < sql_length && sql[index + 1U] == '*') {
            index++;
            block_comment = true;
        } else if (ch == '?') {
            count++;
        }
    }
    return count;
}

static bool mylite_mysqli_is_line_comment_terminator(char ch)
{
    return ch == '\n' || ch == '\r';
}

static bool mylite_mysqli_is_dash_comment_start(const char *sql, size_t sql_length, size_t index)
{
    if (index + 2U >= sql_length || sql[index] != '-' || sql[index + 1U] != '-') {
        return false;
    }

    unsigned char following = (unsigned char)sql[index + 2U];
    return following <= (unsigned char)' ';
}

static bool mylite_mysqli_is_local_path(const char *value, size_t length)
{
    return length == strlen(":memory:") && memcmp(value, ":memory:", length) == 0;
}

static zend_string *mylite_mysqli_resolve_path(const char *host, size_t host_length,
                                               const char *database, size_t database_length,
                                               const char *socket, size_t socket_length,
                                               bool *out_memory, bool *out_use_database)
{
    static const char prefix[] = "mylite:";

    *out_memory = true;
    *out_use_database = false;
    if (host != NULL && host_length >= sizeof(prefix) - 1U &&
        memcmp(host, prefix, sizeof(prefix) - 1U) == 0) {
        const char *path = host + sizeof(prefix) - 1U;
        size_t path_length = host_length - (sizeof(prefix) - 1U);

        *out_memory =
            path_length == strlen(":memory:") && memcmp(path, ":memory:", path_length) == 0;
        *out_use_database =
            database != NULL && !mylite_mysqli_is_local_path(database, database_length);
        return zend_string_init(path_length == 0U ? ":memory:" : path,
                                path_length == 0U ? strlen(":memory:") : path_length, false);
    }
    if (socket != NULL && socket_length > 0U) {
        *out_memory = mylite_mysqli_is_local_path(socket, socket_length);
        *out_use_database =
            database != NULL && !mylite_mysqli_is_local_path(database, database_length);
        return zend_string_init(socket, socket_length, false);
    }
    if (database != NULL && (mylite_mysqli_is_local_path(database, database_length) ||
                             memchr(database, '/', database_length) != NULL ||
                             (database_length > strlen(".mylite") &&
                              memcmp(database + database_length - strlen(".mylite"), ".mylite",
                                     strlen(".mylite")) == 0))) {
        *out_memory = mylite_mysqli_is_local_path(database, database_length);
        return zend_string_init(database, database_length, false);
    }

    return zend_string_init(":memory:", strlen(":memory:"), false);
}

static zend_string *mylite_mysqli_quote_identifier(const char *value, size_t length)
{
    smart_str quoted = {0};

    smart_str_appendc(&quoted, '`');
    for (size_t index = 0U; index < length; index++) {
        if (value[index] == '`') {
            smart_str_appendc(&quoted, '`');
        }
        smart_str_appendc(&quoted, value[index]);
    }
    smart_str_appendc(&quoted, '`');
    smart_str_0(&quoted);
    return quoted.s;
}

static zend_string *mylite_mysqli_escape_string(const char *value, size_t length)
{
    smart_str escaped = {0};

    for (size_t index = 0U; index < length; index++) {
        switch (value[index]) {
        case '\0':
            smart_str_appendl(&escaped, "\\0", 2);
            break;
        case '\n':
            smart_str_appendl(&escaped, "\\n", 2);
            break;
        case '\r':
            smart_str_appendl(&escaped, "\\r", 2);
            break;
        case '\\':
        case '\'':
        case '"':
            smart_str_appendc(&escaped, '\\');
            smart_str_appendc(&escaped, value[index]);
            break;
        case '\032':
            smart_str_appendl(&escaped, "\\Z", 2);
            break;
        default:
            smart_str_appendc(&escaped, value[index]);
            break;
        }
    }
    smart_str_0(&escaped);
    return escaped.s == NULL ? zend_string_init("", 0, false) : escaped.s;
}

static void mylite_mysqli_set_error(mylite_mysqli_link *link, int error_code, const char *sqlstate,
                                    const char *message)
{
    if (message == NULL || message[0] == '\0') {
        message = "MyLite mysqli error";
    }
    zend_string_release(link->error);
    link->error = zend_string_init(message, strlen(message), false);
    link->error_code = error_code;
    memcpy(link->sqlstate, sqlstate, 5U);
    link->sqlstate[5] = '\0';
    mylite_mysqli_update_link_properties(link);
}

static void mylite_mysqli_set_stmt_error(mylite_mysqli_stmt *stmt, int error_code,
                                         const char *sqlstate, const char *message)
{
    if (message == NULL || message[0] == '\0') {
        message = "MyLite mysqli statement error";
    }
    zend_string_release(stmt->error);
    stmt->error = zend_string_init(message, strlen(message), false);
    stmt->error_code = error_code;
    memcpy(stmt->sqlstate, sqlstate, 5U);
    stmt->sqlstate[5] = '\0';
    mylite_mysqli_update_stmt_properties(stmt);
}

static void mylite_mysqli_clear_error(mylite_mysqli_link *link)
{
    zend_string_release(link->error);
    link->error = zend_empty_string;
    zend_string_addref(link->error);
    link->error_code = 0;
    memcpy(link->sqlstate, "00000", sizeof(link->sqlstate));
    mylite_mysqli_update_link_properties(link);
}

static void mylite_mysqli_clear_stmt_error(mylite_mysqli_stmt *stmt)
{
    zend_string_release(stmt->error);
    stmt->error = zend_empty_string;
    zend_string_addref(stmt->error);
    stmt->error_code = 0;
    memcpy(stmt->sqlstate, "00000", sizeof(stmt->sqlstate));
    mylite_mysqli_update_stmt_properties(stmt);
}

static void mylite_mysqli_report_link_error(mylite_mysqli_link *link)
{
    if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_STRICT) != 0) {
        zend_object *exception = zend_throw_exception(mylite_mysqli_exception_ce,
                                                      ZSTR_VAL(link->error), link->error_code);

        if (exception != NULL) {
            zend_update_property_string(mylite_mysqli_exception_ce, exception, "sqlstate",
                                        strlen("sqlstate"), link->sqlstate);
        }
    } else if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_ERROR) != 0) {
        php_error_docref(NULL, E_WARNING, "%s", ZSTR_VAL(link->error));
    }
}

static void mylite_mysqli_report_stmt_error(mylite_mysqli_stmt *stmt)
{
    if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_STRICT) != 0) {
        zend_object *exception = zend_throw_exception(mylite_mysqli_exception_ce,
                                                      ZSTR_VAL(stmt->error), stmt->error_code);

        if (exception != NULL) {
            zend_update_property_string(mylite_mysqli_exception_ce, exception, "sqlstate",
                                        strlen("sqlstate"), stmt->sqlstate);
        }
    } else if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_ERROR) != 0) {
        php_error_docref(NULL, E_WARNING, "%s", ZSTR_VAL(stmt->error));
    }
}

static int mylite_mysqli_error_from_status(int status, const char **out_sqlstate)
{
    switch (status) {
    case MYLITE_NOMEM:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_CLIENT;
    case MYLITE_MISUSE:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_CLIENT;
    case MYLITE_ERROR:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_EXEC;
    default:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_CLIENT;
    }
}

static void mylite_mysqli_update_link_properties(mylite_mysqli_link *link)
{
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "affected_rows",
                              strlen("affected_rows"), link->affected_rows);
    zend_update_property_string(mylite_mysqli_link_ce, &link->std, "client_info",
                                strlen("client_info"), "mylite mysqli " PHP_MYLITE_MYSQLI_VERSION);
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "client_version",
                              strlen("client_version"), 100);
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "connect_errno",
                              strlen("connect_errno"), MYLITE_MYSQLI_G(connect_errno));
    zend_update_property_string(mylite_mysqli_link_ce, &link->std, "connect_error",
                                strlen("connect_error"), MYLITE_MYSQLI_G(connect_error));
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "errno", strlen("errno"),
                              link->error_code);
    zend_update_property_str(mylite_mysqli_link_ce, &link->std, "error", strlen("error"),
                             zend_string_copy(link->error));
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "field_count",
                              strlen("field_count"), link->field_count);
    zend_update_property_string(mylite_mysqli_link_ce, &link->std, "host_info", strlen("host_info"),
                                "MyLite embedded");
    zend_update_property_null(mylite_mysqli_link_ce, &link->std, "info", strlen("info"));
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "insert_id", strlen("insert_id"),
                              link->insert_id);
    zend_update_property_string(mylite_mysqli_link_ce, &link->std, "server_info",
                                strlen("server_info"), MYLITE_MYSQLI_SERVER_INFO);
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "server_version",
                              strlen("server_version"), MYLITE_MYSQLI_SERVER_VERSION);
    zend_update_property_string(mylite_mysqli_link_ce, &link->std, "sqlstate", strlen("sqlstate"),
                                link->sqlstate);
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "protocol_version",
                              strlen("protocol_version"), 10);
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "thread_id", strlen("thread_id"),
                              1);
    zend_update_property_long(mylite_mysqli_link_ce, &link->std, "warning_count",
                              strlen("warning_count"), link->warning_count);
}

static void mylite_mysqli_update_result_properties(mylite_mysqli_result *result)
{
    zend_update_property_long(mylite_mysqli_result_ce, &result->std, "current_field",
                              strlen("current_field"), result->field_cursor);
    zend_update_property_long(mylite_mysqli_result_ce, &result->std, "field_count",
                              strlen("field_count"), result->column_count);
    zend_update_property_null(mylite_mysqli_result_ce, &result->std, "lengths", strlen("lengths"));
    zend_update_property_long(mylite_mysqli_result_ce, &result->std, "num_rows", strlen("num_rows"),
                              result->row_count);
    zend_update_property_long(mylite_mysqli_result_ce, &result->std, "type", strlen("type"),
                              MYLITE_MYSQLI_STORE_RESULT);
}

static void mylite_mysqli_update_stmt_properties(mylite_mysqli_stmt *stmt)
{
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "affected_rows",
                              strlen("affected_rows"), stmt->affected_rows);
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "insert_id", strlen("insert_id"),
                              stmt->insert_id);
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "num_rows", strlen("num_rows"),
                              stmt->num_rows);
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "param_count",
                              strlen("param_count"), stmt->param_count);
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "field_count",
                              strlen("field_count"), stmt->field_count);
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "errno", strlen("errno"),
                              stmt->error_code);
    zend_update_property_str(mylite_mysqli_stmt_ce, &stmt->std, "error", strlen("error"),
                             zend_string_copy(stmt->error));
    zend_update_property_string(mylite_mysqli_stmt_ce, &stmt->std, "sqlstate", strlen("sqlstate"),
                                stmt->sqlstate);
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "id", strlen("id"), 0);
}

static void mylite_mysqli_set_global_connect_error(int error_code, const char *message)
{
    MYLITE_MYSQLI_G(connect_errno) = error_code;
    snprintf(MYLITE_MYSQLI_G(connect_error), sizeof(MYLITE_MYSQLI_G(connect_error)), "%s",
             message == NULL ? "" : message);
}

static void mylite_mysqli_init_globals(zend_mylite_mysqli_globals *globals)
{
    globals->report_mode = MYLITE_MYSQLI_REPORT_ERROR | MYLITE_MYSQLI_REPORT_STRICT;
    globals->connect_errno = 0;
    globals->connect_error[0] = '\0';
}

static void mylite_mysqli_register_constants(int module_number)
{
    REGISTER_LONG_CONSTANT("MYSQLI_READ_DEFAULT_GROUP", 5, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_READ_DEFAULT_FILE", 4, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_CONNECT_TIMEOUT", 0, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_LOCAL_INFILE", 8, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_LOAD_DATA_LOCAL_DIR", 43, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_INIT_COMMAND", 3, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_READ_TIMEOUT", 11, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_NET_CMD_BUFFER_SIZE", 202, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_NET_READ_BUFFER_SIZE", 203, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_INT_AND_FLOAT_NATIVE", 201, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_SSL_VERIFY_SERVER_CERT", 21, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SERVER_PUBLIC_KEY", 35, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_SSL", 2048, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_COMPRESS", 32, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_INTERACTIVE", 1024, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_IGNORE_SPACE", 256, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_NO_SCHEMA", 16, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_FOUND_ROWS", 2, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_SSL_VERIFY_SERVER_CERT", 1073741824, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_SSL_DONT_VERIFY_SERVER_CERT", 64, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS", 4194304, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_OPT_CAN_HANDLE_EXPIRED_PASSWORDS", 37, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_STORE_RESULT", MYLITE_MYSQLI_STORE_RESULT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_USE_RESULT", MYLITE_MYSQLI_USE_RESULT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_ASYNC", 8, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_STORE_RESULT_COPY_DATA", 16, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_ASSOC", MYLITE_MYSQLI_ASSOC, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_NUM", MYLITE_MYSQLI_NUM, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_BOTH", MYLITE_MYSQLI_BOTH, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_STMT_ATTR_UPDATE_MAX_LENGTH", 0, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_STMT_ATTR_CURSOR_TYPE", 1, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CURSOR_TYPE_NO_CURSOR", 0, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_CURSOR_TYPE_READ_ONLY", 1, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_NOT_NULL_FLAG", MYLITE_MYSQLI_FIELD_FLAG_NOT_NULL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_PRI_KEY_FLAG", MYLITE_MYSQLI_FIELD_FLAG_PRI_KEY, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_UNIQUE_KEY_FLAG", MYLITE_MYSQLI_FIELD_FLAG_UNIQUE_KEY,
                           CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_MULTIPLE_KEY_FLAG", MYLITE_MYSQLI_FIELD_FLAG_MULTIPLE_KEY,
                           CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_BLOB_FLAG", MYLITE_MYSQLI_FIELD_FLAG_BLOB, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_UNSIGNED_FLAG", MYLITE_MYSQLI_FIELD_FLAG_UNSIGNED, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_ZEROFILL_FLAG", MYLITE_MYSQLI_FIELD_FLAG_ZEROFILL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_BINARY_FLAG", MYLITE_MYSQLI_FIELD_FLAG_BINARY, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TIMESTAMP_FLAG", 1024, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SET_FLAG", 2048, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_PART_KEY_FLAG", 16384, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_GROUP_FLAG", 32768, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_ENUM_FLAG", 256, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_NO_DEFAULT_VALUE_FLAG", 4096, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_ON_UPDATE_NOW_FLAG", 8192, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_AUTO_INCREMENT_FLAG", MYLITE_MYSQLI_FIELD_FLAG_AUTO_INCREMENT,
                           CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_NUM_FLAG", MYLITE_MYSQLI_FIELD_FLAG_NUM, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_DECIMAL", MYLITE_MYSQLI_FIELD_TYPE_DECIMAL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_TINY", MYLITE_MYSQLI_FIELD_TYPE_TINY, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_SHORT", MYLITE_MYSQLI_FIELD_TYPE_SHORT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_LONG", MYLITE_MYSQLI_FIELD_TYPE_LONG, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_FLOAT", MYLITE_MYSQLI_FIELD_TYPE_FLOAT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_DOUBLE", MYLITE_MYSQLI_FIELD_TYPE_DOUBLE, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_NULL", MYLITE_MYSQLI_FIELD_TYPE_NULL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_TIMESTAMP", MYLITE_MYSQLI_FIELD_TYPE_TIMESTAMP, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_LONGLONG", MYLITE_MYSQLI_FIELD_TYPE_LONGLONG, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_INT24", MYLITE_MYSQLI_FIELD_TYPE_INT24, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_DATE", MYLITE_MYSQLI_FIELD_TYPE_DATE, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_TIME", MYLITE_MYSQLI_FIELD_TYPE_TIME, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_DATETIME", MYLITE_MYSQLI_FIELD_TYPE_DATETIME, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_YEAR", MYLITE_MYSQLI_FIELD_TYPE_YEAR, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_NEWDATE", MYLITE_MYSQLI_FIELD_TYPE_NEWDATE, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_CHAR", MYLITE_MYSQLI_FIELD_TYPE_TINY, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_TINY_BLOB", 249, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_MEDIUM_BLOB", 250, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_LONG_BLOB", 251, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_NEWDECIMAL", MYLITE_MYSQLI_FIELD_TYPE_NEWDECIMAL,
                           CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_JSON", 245, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_VECTOR", 242, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_ENUM", MYLITE_MYSQLI_FIELD_TYPE_ENUM, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_SET", MYLITE_MYSQLI_FIELD_TYPE_SET, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_BLOB", MYLITE_MYSQLI_FIELD_TYPE_BLOB, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_VAR_STRING", MYLITE_MYSQLI_FIELD_TYPE_VAR_STRING,
                           CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_STRING", MYLITE_MYSQLI_FIELD_TYPE_STRING, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_GEOMETRY", 255, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_BIT", MYLITE_MYSQLI_FIELD_TYPE_BIT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SET_CHARSET_NAME", 7, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_NO_DATA", 100, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_DATA_TRUNCATED", 101, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REPORT_INDEX", MYLITE_MYSQLI_REPORT_INDEX, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REPORT_ERROR", MYLITE_MYSQLI_REPORT_ERROR, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REPORT_STRICT", MYLITE_MYSQLI_REPORT_STRICT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REPORT_ALL", MYLITE_MYSQLI_REPORT_ALL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REPORT_OFF", MYLITE_MYSQLI_REPORT_OFF, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_DEBUG_TRACE_ENABLED", 0, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SERVER_QUERY_NO_GOOD_INDEX_USED", 16, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SERVER_QUERY_NO_INDEX_USED", 32, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SERVER_QUERY_WAS_SLOW", 2048, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SERVER_PS_OUT_PARAMS", 4096, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_GRANT", 1, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_LOG", 2, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_TABLES", 4, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_HOSTS", 8, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_STATUS", 16, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_THREADS", 32, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_REPLICA", 64, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_SLAVE", 64, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_MASTER", 128, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_REFRESH_BACKUP_LOG", 2097152, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TRANS_START_WITH_CONSISTENT_SNAPSHOT", 1, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TRANS_START_READ_WRITE", 2, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TRANS_START_READ_ONLY", 4, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TRANS_COR_AND_CHAIN", 1, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TRANS_COR_AND_NO_CHAIN", 2, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TRANS_COR_RELEASE", 4, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TRANS_COR_NO_RELEASE", 8, CONST_PERSISTENT);
    REGISTER_BOOL_CONSTANT("MYSQLI_IS_MARIADB", false, CONST_PERSISTENT);
    (void)module_number;
}

static void mylite_mysqli_register_classes(void)
{
    zend_class_entry class_entry;

    INIT_CLASS_ENTRY(class_entry, "mysqli", mylite_mysqli_link_methods);
    mylite_mysqli_link_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_link_ce->create_object = mylite_mysqli_link_create;
    memcpy(&mylite_mysqli_link_handlers, zend_get_std_object_handlers(),
           sizeof(mylite_mysqli_link_handlers));
    mylite_mysqli_link_handlers.offset = XtOffsetOf(mylite_mysqli_link, std);
    mylite_mysqli_link_handlers.free_obj = mylite_mysqli_link_free;

    zend_declare_property_long(mylite_mysqli_link_ce, "affected_rows", strlen("affected_rows"), -1,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_link_ce, "client_info", strlen("client_info"), "",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "client_version", strlen("client_version"),
                               100, ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "connect_errno", strlen("connect_errno"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_link_ce, "connect_error", strlen("connect_error"),
                                 "", ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "errno", strlen("errno"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_link_ce, "error", strlen("error"), "",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_null(mylite_mysqli_link_ce, "error_list", strlen("error_list"),
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "field_count", strlen("field_count"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_link_ce, "host_info", strlen("host_info"), "",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_null(mylite_mysqli_link_ce, "info", strlen("info"), ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "insert_id", strlen("insert_id"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_link_ce, "server_info", strlen("server_info"), "",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "server_version", strlen("server_version"),
                               MYLITE_MYSQLI_SERVER_VERSION, ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_link_ce, "sqlstate", strlen("sqlstate"), "00000",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "protocol_version",
                               strlen("protocol_version"), 10, ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "thread_id", strlen("thread_id"), 1,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_link_ce, "warning_count", strlen("warning_count"), 0,
                               ZEND_ACC_PUBLIC);

    INIT_CLASS_ENTRY(class_entry, "mysqli_result", mylite_mysqli_result_methods);
    mylite_mysqli_result_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_result_ce->create_object = mylite_mysqli_result_create;
    memcpy(&mylite_mysqli_result_handlers, zend_get_std_object_handlers(),
           sizeof(mylite_mysqli_result_handlers));
    mylite_mysqli_result_handlers.offset = XtOffsetOf(mylite_mysqli_result, std);
    mylite_mysqli_result_handlers.free_obj = mylite_mysqli_result_free;
    zend_declare_property_long(mylite_mysqli_result_ce, "current_field", strlen("current_field"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_result_ce, "field_count", strlen("field_count"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_null(mylite_mysqli_result_ce, "lengths", strlen("lengths"),
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_result_ce, "num_rows", strlen("num_rows"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_result_ce, "type", strlen("type"), 0, ZEND_ACC_PUBLIC);
    zend_class_implements(mylite_mysqli_result_ce, 1, zend_ce_aggregate);

    INIT_CLASS_ENTRY(class_entry, "mysqli_stmt", mylite_mysqli_stmt_methods);
    mylite_mysqli_stmt_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_stmt_ce->create_object = mylite_mysqli_stmt_create;
    memcpy(&mylite_mysqli_stmt_handlers, zend_get_std_object_handlers(),
           sizeof(mylite_mysqli_stmt_handlers));
    mylite_mysqli_stmt_handlers.offset = XtOffsetOf(mylite_mysqli_stmt, std);
    mylite_mysqli_stmt_handlers.free_obj = mylite_mysqli_stmt_free;
    zend_declare_property_long(mylite_mysqli_stmt_ce, "affected_rows", strlen("affected_rows"), -1,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_stmt_ce, "insert_id", strlen("insert_id"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_stmt_ce, "num_rows", strlen("num_rows"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_stmt_ce, "param_count", strlen("param_count"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_stmt_ce, "field_count", strlen("field_count"), 0,
                               ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_stmt_ce, "errno", strlen("errno"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_stmt_ce, "error", strlen("error"), "",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_null(mylite_mysqli_stmt_ce, "error_list", strlen("error_list"),
                               ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_stmt_ce, "sqlstate", strlen("sqlstate"), "00000",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_stmt_ce, "id", strlen("id"), 0, ZEND_ACC_PUBLIC);

    INIT_CLASS_ENTRY(class_entry, "mysqli_driver", NULL);
    mylite_mysqli_driver_ce = zend_register_internal_class(&class_entry);
    zend_declare_property_string(mylite_mysqli_driver_ce, "client_info", strlen("client_info"),
                                 "mylite mysqli " PHP_MYLITE_MYSQLI_VERSION, ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_driver_ce, "client_version", strlen("client_version"),
                               100, ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_driver_ce, "driver_version", strlen("driver_version"),
                               100, ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_driver_ce, "report_mode", strlen("report_mode"),
                               MYLITE_MYSQLI_REPORT_ERROR | MYLITE_MYSQLI_REPORT_STRICT,
                               ZEND_ACC_PUBLIC);

    INIT_CLASS_ENTRY(class_entry, "mysqli_warning", mylite_mysqli_warning_methods);
    mylite_mysqli_warning_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_warning_ce->create_object = mylite_mysqli_warning_create;
    memcpy(&mylite_mysqli_warning_handlers, zend_get_std_object_handlers(),
           sizeof(mylite_mysqli_warning_handlers));
    mylite_mysqli_warning_handlers.offset = XtOffsetOf(mylite_mysqli_warning, std);
    mylite_mysqli_warning_handlers.free_obj = mylite_mysqli_warning_free;
    zend_declare_property_string(mylite_mysqli_warning_ce, "message", strlen("message"), "",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_string(mylite_mysqli_warning_ce, "sqlstate", strlen("sqlstate"), "HY000",
                                 ZEND_ACC_PUBLIC);
    zend_declare_property_long(mylite_mysqli_warning_ce, "errno", strlen("errno"), 0,
                               ZEND_ACC_PUBLIC);

    INIT_CLASS_ENTRY(class_entry, "mysqli_sql_exception", mylite_mysqli_exception_methods);
    mylite_mysqli_exception_ce =
        zend_register_internal_class_ex(&class_entry, spl_ce_RuntimeException);
    zend_declare_property_string(mylite_mysqli_exception_ce, "sqlstate", strlen("sqlstate"),
                                 "HY000", ZEND_ACC_PROTECTED);
}

PHP_MINIT_FUNCTION(mysqli)
{
    mylite_mysqli_register_classes();
    mylite_mysqli_register_constants(module_number);
    return SUCCESS;
}

PHP_MINFO_FUNCTION(mysqli)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "MyLite mysqli support", "enabled");
    php_info_print_table_row(2, "MyLite version", mylite_version());
    php_info_print_table_end();
}

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
PHP_GINIT_FUNCTION(mylite_mysqli)
{
#if defined(COMPILE_DL_MYSQLI) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    mylite_mysqli_init_globals(mylite_mysqli_globals);
}
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

zend_module_entry mysqli_module_entry = {
    STANDARD_MODULE_HEADER,
    "mysqli",
    mylite_mysqli_functions,
    PHP_MINIT(mysqli),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(mysqli),
    PHP_MYLITE_MYSQLI_VERSION,
    PHP_MODULE_GLOBALS(mylite_mysqli),
    PHP_GINIT(mylite_mysqli),
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX,
};

#ifdef COMPILE_DL_MYSQLI
ZEND_GET_MODULE(mysqli)
#endif
