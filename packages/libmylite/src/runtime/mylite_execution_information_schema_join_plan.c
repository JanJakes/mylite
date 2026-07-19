#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_information_schema_join_plan.h"
#include "mylite_execution_scalar.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum { information_schema_join_predicate_stack_capacity = 8 };

#include "mylite_execution_information_schema_join_plan.inc"
