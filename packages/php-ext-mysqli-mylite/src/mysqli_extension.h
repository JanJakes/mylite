#ifndef MYLITE_PHP_MYSQLI_EXTENSION_H
#define MYLITE_PHP_MYSQLI_EXTENSION_H

#include <Zend/zend_smart_str.h>
#include <ext/spl/spl_exceptions.h>
#include <ext/standard/info.h>
#include <mylite/mylite.h>
#include <php.h>
#include <zend_exceptions.h>
#include <zend_interfaces.h>

#include <ctype.h>
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
#define MYLITE_MYSQLI_MAX_ALLOWED_PACKET 67108864U
#define MYLITE_MYSQLI_PROFILE_SLOT_COUNT 64U
#define MYLITE_MYSQLI_PROFILE_SQL_LENGTH 160U

typedef struct {
    char sql[MYLITE_MYSQLI_PROFILE_SQL_LENGTH];
    uint64_t calls;
    uint64_t errors;
    uint64_t bridge_calls;
    uint64_t result_sets;
    uint64_t rows;
    uint64_t cells;
    uint64_t execute_ns;
    uint64_t buffer_ns;
} mylite_mysqli_profile_slot;

enum mylite_mysqli_error_code {
    MYLITE_MYSQLI_ERROR_NONE = 0,
    MYLITE_MYSQLI_ERROR_CLIENT = 2000,
    MYLITE_MYSQLI_ERROR_CONNECTION = 2002,
    MYLITE_MYSQLI_ERROR_PARSE = 1064,
    MYLITE_MYSQLI_ERROR_PACKET_TOO_LARGE = 1153,
    MYLITE_MYSQLI_ERROR_UNSUPPORTED = 1235,
    MYLITE_MYSQLI_ERROR_EXEC = 1105,
};

typedef struct {
    mylite_db *database;
    zend_string *path;
    zend_string *error;
    zend_string *info;
    char sqlstate[6];
    zend_long affected_rows;
    zend_long insert_id;
    zend_long field_count;
    zend_long warning_count;
    int error_code;
    bool connected;
    zval last_result;
    mylite_stmt *pending_stmt;
    zend_string *pending_sql;
    uint64_t pending_execute_ns;
    zend_object std;
} mylite_mysqli_link;

typedef struct {
    zend_string *name;
    zend_string *schema;
    zend_string *table;
    zend_string *origin_table;
    zend_string *origin_name;
    int type;
    unsigned int flags;
    uint64_t length;
    uint64_t max_length;
    unsigned int decimals;
    unsigned int charset;
    bool nullable;
} mylite_mysqli_field;

typedef struct {
    mylite_mysqli_field *fields;
    zval *values;
    mylite_stmt *native_stmt;
    uint32_t column_count;
    uint32_t row_count;
    uint32_t row_capacity;
    uint32_t cursor;
    uint32_t field_cursor;
    bool unbuffered;
    bool current_row_valid;
    bool unbuffered_finished;
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
bool profile_enabled;
bool profile_stderr;
char profile_path[512];
uint32_t profile_limit;
uint64_t profile_calls;
uint64_t profile_errors;
uint64_t profile_bridge_calls;
uint64_t profile_result_sets;
uint64_t profile_rows;
uint64_t profile_cells;
uint64_t profile_execute_ns;
uint64_t profile_buffer_ns;
mylite_mysqli_profile_slot profile_slots[MYLITE_MYSQLI_PROFILE_SLOT_COUNT];
ZEND_END_MODULE_GLOBALS(mylite_mysqli)

ZEND_EXTERN_MODULE_GLOBALS(mylite_mysqli)

#ifdef ZTS
#  define MYLITE_MYSQLI_G(v) ZEND_TSRMG(mylite_mysqli_globals_id, zend_mylite_mysqli_globals *, v)
#else
#  define MYLITE_MYSQLI_G(v) (mylite_mysqli_globals.v)
#endif

extern zend_class_entry *mylite_mysqli_link_ce;
extern zend_class_entry *mylite_mysqli_result_ce;
extern zend_class_entry *mylite_mysqli_stmt_ce;
extern zend_class_entry *mylite_mysqli_driver_ce;
extern zend_class_entry *mylite_mysqli_warning_ce;
extern zend_class_entry *mylite_mysqli_exception_ce;
extern zend_object_handlers mylite_mysqli_link_handlers;
extern zend_object_handlers mylite_mysqli_result_handlers;
extern zend_object_handlers mylite_mysqli_stmt_handlers;
extern zend_object_handlers mylite_mysqli_warning_handlers;
extern const zend_function_entry mylite_mysqli_functions[];

zend_object *mylite_mysqli_link_create(zend_class_entry *class_entry);
void mylite_mysqli_link_free(zend_object *object);
zend_object *mylite_mysqli_result_create(zend_class_entry *class_entry);
void mylite_mysqli_result_free(zend_object *object);
zend_object *mylite_mysqli_stmt_create(zend_class_entry *class_entry);
void mylite_mysqli_stmt_free(zend_object *object);
zend_object *mylite_mysqli_warning_create(zend_class_entry *class_entry);
void mylite_mysqli_warning_free(zend_object *object);

mylite_mysqli_link *mylite_mysqli_link_from_obj(zend_object *object);
mylite_mysqli_result *mylite_mysqli_result_from_obj(zend_object *object);
mylite_mysqli_stmt *mylite_mysqli_stmt_from_obj(zend_object *object);

bool mylite_mysqli_connect_link(
    mylite_mysqli_link *link,
    const char *host,
    size_t host_length,
    const char *database,
    size_t database_length,
    const char *socket,
    size_t socket_length
);
void mylite_mysqli_close_link(mylite_mysqli_link *link);
bool mylite_mysqli_link_query(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zend_long result_mode,
    zval *out_result
);
bool mylite_mysqli_link_real_query(mylite_mysqli_link *link, const char *sql, size_t sql_length);
bool mylite_mysqli_link_store_result(mylite_mysqli_link *link, zval *out_result);
bool mylite_mysqli_link_use_result(mylite_mysqli_link *link, zval *out_result);
bool mylite_mysqli_stmt_prepare_internal(
    mylite_mysqli_stmt *stmt,
    const char *sql,
    size_t sql_length
);
bool mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt *stmt, zval *params);
void mylite_mysqli_result_fetch(mylite_mysqli_result *result, int mode, zval *return_value);
void mylite_mysqli_result_fetch_column(
    mylite_mysqli_result *result,
    zend_long column,
    zval *return_value
);
void mylite_mysqli_result_fetch_field(
    mylite_mysqli_result *result,
    uint32_t index,
    zval *return_value
);
zend_long mylite_mysqli_result_num_rows(const mylite_mysqli_result *result);
void mylite_mysqli_result_discard(mylite_mysqli_result *result);
zend_string *mylite_mysqli_interpolate_query(
    zend_string *query,
    zval *params,
    uint32_t param_count
);
zend_string *mylite_mysqli_interpolate_bound_params(
    zend_string *query,
    zval *params,
    uint32_t param_count
);
zend_string *mylite_mysqli_quote_identifier(const char *value, size_t length);
zend_string *mylite_mysqli_escape_string(const char *value, size_t length);
void mylite_mysqli_set_error(
    mylite_mysqli_link *link,
    int error_code,
    const char *sqlstate,
    const char *message
);
void mylite_mysqli_set_stmt_error(
    mylite_mysqli_stmt *stmt,
    int error_code,
    const char *sqlstate,
    const char *message
);
void mylite_mysqli_report_link_error(mylite_mysqli_link *link);
void mylite_mysqli_report_stmt_error(mylite_mysqli_stmt *stmt);
void mylite_mysqli_update_link_properties(mylite_mysqli_link *link);
void mylite_mysqli_update_link_status_properties(mylite_mysqli_link *link);
void mylite_mysqli_update_result_properties(mylite_mysqli_result *result);
void mylite_mysqli_update_stmt_properties(mylite_mysqli_stmt *stmt);
void mylite_mysqli_init_globals(zend_mylite_mysqli_globals *globals);
void mylite_mysqli_flush_profile(void);
void mylite_mysqli_register_constants(int module_number);
void mylite_mysqli_register_classes(void);

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

#endif
