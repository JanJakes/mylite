#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "mysqli_extension.h"

zend_class_entry *mylite_mysqli_link_ce;
zend_class_entry *mylite_mysqli_result_ce;
zend_class_entry *mylite_mysqli_stmt_ce;
zend_class_entry *mylite_mysqli_driver_ce;
zend_class_entry *mylite_mysqli_warning_ce;
zend_class_entry *mylite_mysqli_exception_ce;
zend_object_handlers mylite_mysqli_link_handlers;
zend_object_handlers mylite_mysqli_result_handlers;
zend_object_handlers mylite_mysqli_stmt_handlers;
zend_object_handlers mylite_mysqli_warning_handlers;

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

/* clang-format off */
const zend_function_entry mylite_mysqli_functions[] = {
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

void mylite_mysqli_register_constants(int module_number) {
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
    REGISTER_LONG_CONSTANT(
        "MYSQLI_NOT_NULL_FLAG",
        MYLITE_MYSQLI_FIELD_FLAG_NOT_NULL,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT(
        "MYSQLI_PRI_KEY_FLAG",
        MYLITE_MYSQLI_FIELD_FLAG_PRI_KEY,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT(
        "MYSQLI_UNIQUE_KEY_FLAG",
        MYLITE_MYSQLI_FIELD_FLAG_UNIQUE_KEY,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT(
        "MYSQLI_MULTIPLE_KEY_FLAG",
        MYLITE_MYSQLI_FIELD_FLAG_MULTIPLE_KEY,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_BLOB_FLAG", MYLITE_MYSQLI_FIELD_FLAG_BLOB, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_UNSIGNED_FLAG",
        MYLITE_MYSQLI_FIELD_FLAG_UNSIGNED,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT(
        "MYSQLI_ZEROFILL_FLAG",
        MYLITE_MYSQLI_FIELD_FLAG_ZEROFILL,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_BINARY_FLAG", MYLITE_MYSQLI_FIELD_FLAG_BINARY, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TIMESTAMP_FLAG", 1024, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_SET_FLAG", 2048, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_PART_KEY_FLAG", 16384, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_GROUP_FLAG", 32768, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_ENUM_FLAG", 256, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_NO_DEFAULT_VALUE_FLAG", 4096, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_ON_UPDATE_NOW_FLAG", 8192, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_AUTO_INCREMENT_FLAG",
        MYLITE_MYSQLI_FIELD_FLAG_AUTO_INCREMENT,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_NUM_FLAG", MYLITE_MYSQLI_FIELD_FLAG_NUM, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_TYPE_DECIMAL",
        MYLITE_MYSQLI_FIELD_TYPE_DECIMAL,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_TINY", MYLITE_MYSQLI_FIELD_TYPE_TINY, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_SHORT", MYLITE_MYSQLI_FIELD_TYPE_SHORT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_LONG", MYLITE_MYSQLI_FIELD_TYPE_LONG, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_FLOAT", MYLITE_MYSQLI_FIELD_TYPE_FLOAT, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_DOUBLE", MYLITE_MYSQLI_FIELD_TYPE_DOUBLE, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_NULL", MYLITE_MYSQLI_FIELD_TYPE_NULL, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_TYPE_TIMESTAMP",
        MYLITE_MYSQLI_FIELD_TYPE_TIMESTAMP,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT(
        "MYSQLI_TYPE_LONGLONG",
        MYLITE_MYSQLI_FIELD_TYPE_LONGLONG,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_INT24", MYLITE_MYSQLI_FIELD_TYPE_INT24, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_DATE", MYLITE_MYSQLI_FIELD_TYPE_DATE, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_TIME", MYLITE_MYSQLI_FIELD_TYPE_TIME, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_TYPE_DATETIME",
        MYLITE_MYSQLI_FIELD_TYPE_DATETIME,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_YEAR", MYLITE_MYSQLI_FIELD_TYPE_YEAR, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_TYPE_NEWDATE",
        MYLITE_MYSQLI_FIELD_TYPE_NEWDATE,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_CHAR", MYLITE_MYSQLI_FIELD_TYPE_TINY, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_TINY_BLOB", 249, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_MEDIUM_BLOB", 250, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_LONG_BLOB", 251, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_TYPE_NEWDECIMAL",
        MYLITE_MYSQLI_FIELD_TYPE_NEWDECIMAL,
        CONST_PERSISTENT
    );
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_JSON", 245, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_VECTOR", 242, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_ENUM", MYLITE_MYSQLI_FIELD_TYPE_ENUM, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_SET", MYLITE_MYSQLI_FIELD_TYPE_SET, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MYSQLI_TYPE_BLOB", MYLITE_MYSQLI_FIELD_TYPE_BLOB, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT(
        "MYSQLI_TYPE_VAR_STRING",
        MYLITE_MYSQLI_FIELD_TYPE_VAR_STRING,
        CONST_PERSISTENT
    );
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

void mylite_mysqli_register_classes(void) {
    zend_class_entry class_entry;

    INIT_CLASS_ENTRY(class_entry, "mysqli", mylite_mysqli_link_methods);
    mylite_mysqli_link_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_link_ce->create_object = mylite_mysqli_link_create;
    memcpy(
        &mylite_mysqli_link_handlers,
        zend_get_std_object_handlers(),
        sizeof(mylite_mysqli_link_handlers)
    );
    mylite_mysqli_link_handlers.offset = XtOffsetOf(mylite_mysqli_link, std);
    mylite_mysqli_link_handlers.free_obj = mylite_mysqli_link_free;

    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "affected_rows",
        strlen("affected_rows"),
        -1,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_string(
        mylite_mysqli_link_ce,
        "client_info",
        strlen("client_info"),
        "",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "client_version",
        strlen("client_version"),
        100,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "connect_errno",
        strlen("connect_errno"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_string(
        mylite_mysqli_link_ce,
        "connect_error",
        strlen("connect_error"),
        "",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(mylite_mysqli_link_ce, "errno", strlen("errno"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(
        mylite_mysqli_link_ce,
        "error",
        strlen("error"),
        "",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_null(
        mylite_mysqli_link_ce,
        "error_list",
        strlen("error_list"),
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "field_count",
        strlen("field_count"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_string(
        mylite_mysqli_link_ce,
        "host_info",
        strlen("host_info"),
        "",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_null(mylite_mysqli_link_ce, "info", strlen("info"), ZEND_ACC_PUBLIC);
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "insert_id",
        strlen("insert_id"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_string(
        mylite_mysqli_link_ce,
        "server_info",
        strlen("server_info"),
        "",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "server_version",
        strlen("server_version"),
        MYLITE_MYSQLI_SERVER_VERSION,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_string(
        mylite_mysqli_link_ce,
        "sqlstate",
        strlen("sqlstate"),
        "00000",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "protocol_version",
        strlen("protocol_version"),
        10,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "thread_id",
        strlen("thread_id"),
        1,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_link_ce,
        "warning_count",
        strlen("warning_count"),
        0,
        ZEND_ACC_PUBLIC
    );

    INIT_CLASS_ENTRY(class_entry, "mysqli_result", mylite_mysqli_result_methods);
    mylite_mysqli_result_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_result_ce->create_object = mylite_mysqli_result_create;
    memcpy(
        &mylite_mysqli_result_handlers,
        zend_get_std_object_handlers(),
        sizeof(mylite_mysqli_result_handlers)
    );
    mylite_mysqli_result_handlers.offset = XtOffsetOf(mylite_mysqli_result, std);
    mylite_mysqli_result_handlers.free_obj = mylite_mysqli_result_free;
    zend_declare_property_long(
        mylite_mysqli_result_ce,
        "current_field",
        strlen("current_field"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_result_ce,
        "field_count",
        strlen("field_count"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_null(
        mylite_mysqli_result_ce,
        "lengths",
        strlen("lengths"),
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_result_ce,
        "num_rows",
        strlen("num_rows"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(mylite_mysqli_result_ce, "type", strlen("type"), 0, ZEND_ACC_PUBLIC);
    zend_class_implements(mylite_mysqli_result_ce, 1, zend_ce_aggregate);

    INIT_CLASS_ENTRY(class_entry, "mysqli_stmt", mylite_mysqli_stmt_methods);
    mylite_mysqli_stmt_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_stmt_ce->create_object = mylite_mysqli_stmt_create;
    memcpy(
        &mylite_mysqli_stmt_handlers,
        zend_get_std_object_handlers(),
        sizeof(mylite_mysqli_stmt_handlers)
    );
    mylite_mysqli_stmt_handlers.offset = XtOffsetOf(mylite_mysqli_stmt, std);
    mylite_mysqli_stmt_handlers.free_obj = mylite_mysqli_stmt_free;
    zend_declare_property_long(
        mylite_mysqli_stmt_ce,
        "affected_rows",
        strlen("affected_rows"),
        -1,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_stmt_ce,
        "insert_id",
        strlen("insert_id"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_stmt_ce,
        "num_rows",
        strlen("num_rows"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_stmt_ce,
        "param_count",
        strlen("param_count"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_stmt_ce,
        "field_count",
        strlen("field_count"),
        0,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(mylite_mysqli_stmt_ce, "errno", strlen("errno"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(
        mylite_mysqli_stmt_ce,
        "error",
        strlen("error"),
        "",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_null(
        mylite_mysqli_stmt_ce,
        "error_list",
        strlen("error_list"),
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_string(
        mylite_mysqli_stmt_ce,
        "sqlstate",
        strlen("sqlstate"),
        "00000",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(mylite_mysqli_stmt_ce, "id", strlen("id"), 0, ZEND_ACC_PUBLIC);

    INIT_CLASS_ENTRY(class_entry, "mysqli_driver", NULL);
    mylite_mysqli_driver_ce = zend_register_internal_class(&class_entry);
    zend_declare_property_string(
        mylite_mysqli_driver_ce,
        "client_info",
        strlen("client_info"),
        "mylite mysqli " PHP_MYLITE_MYSQLI_VERSION,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_driver_ce,
        "client_version",
        strlen("client_version"),
        100,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_driver_ce,
        "driver_version",
        strlen("driver_version"),
        100,
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_driver_ce,
        "report_mode",
        strlen("report_mode"),
        MYLITE_MYSQLI_REPORT_ERROR | MYLITE_MYSQLI_REPORT_STRICT,
        ZEND_ACC_PUBLIC
    );

    INIT_CLASS_ENTRY(class_entry, "mysqli_warning", mylite_mysqli_warning_methods);
    mylite_mysqli_warning_ce = zend_register_internal_class(&class_entry);
    mylite_mysqli_warning_ce->create_object = mylite_mysqli_warning_create;
    memcpy(
        &mylite_mysqli_warning_handlers,
        zend_get_std_object_handlers(),
        sizeof(mylite_mysqli_warning_handlers)
    );
    mylite_mysqli_warning_handlers.offset = XtOffsetOf(mylite_mysqli_warning, std);
    mylite_mysqli_warning_handlers.free_obj = mylite_mysqli_warning_free;
    zend_declare_property_string(
        mylite_mysqli_warning_ce,
        "message",
        strlen("message"),
        "",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_string(
        mylite_mysqli_warning_ce,
        "sqlstate",
        strlen("sqlstate"),
        "HY000",
        ZEND_ACC_PUBLIC
    );
    zend_declare_property_long(
        mylite_mysqli_warning_ce,
        "errno",
        strlen("errno"),
        0,
        ZEND_ACC_PUBLIC
    );

    INIT_CLASS_ENTRY(class_entry, "mysqli_sql_exception", mylite_mysqli_exception_methods);
    mylite_mysqli_exception_ce =
        zend_register_internal_class_ex(&class_entry, spl_ce_RuntimeException);
    zend_declare_property_string(
        mylite_mysqli_exception_ce,
        "sqlstate",
        strlen("sqlstate"),
        "HY000",
        ZEND_ACC_PROTECTED
    );
}
