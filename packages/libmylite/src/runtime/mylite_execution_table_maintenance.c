#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_completion.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_table_maintenance.h"
#include "mylite_execution_table_maintenance_support.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_result.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_table_maintenance_internal.h"

int mylite_execution_execute_checksum_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_checksum_table_statement(database, statement, out_result);
}

int mylite_execution_execute_table_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_table_maintenance_statement(database, statement, out_result);
}

#include "mylite_execution_table_maintenance_queries.inc"
