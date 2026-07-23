#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_connection.h"
#include "mylite_execution_completion.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_select_order_support.h"
#include "mylite_execution_values.h"
#include "mylite_execution_values_support.h"
#include "mylite_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_values_internal.h"

int mylite_execution_execute_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_values_statement(database, statement, out_result);
}

#include "mylite_execution_values_statement.inc"
