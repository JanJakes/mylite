# Baseline Sys Procedure Placeholders

## Scope

This slice covers the MySQL 8.4.9 `sys` stored-procedure names that were still
listed as unsupported in the sys-schema compatibility table. The goal is to make
routine calls predictable for embedded applications without claiming that MyLite
has live Performance Schema setup or tracing state.

Implemented behavior:

- `CALL sys.table_exists(in_db, in_table, out_var)` resolves MyLite descriptor
  tables, views, and session temporary tables and stores one of `BASE TABLE`,
  `VIEW`, `TEMPORARY`, or the empty string in the OUT user variable.
- `CALL sys.execute_prepared_stmt(in_query)` executes one SQL string through
  MyLite's prepared-statement execution path. It temporarily uses `sys` as the
  default schema, matching MySQL routine-context behavior observed with
  `DATABASE()`, and restores the caller's selected schema afterward.
- Performance Schema setup mutator procedures are accepted with deterministic
  no-op summary result sets because MyLite does not maintain mutable
  `performance_schema.setup_*` tables.
- Performance Schema setup listing procedures return MySQL-shaped empty
  placeholder result sets where a stable embedded rowset is meaningful.
- Multi-result tracing/diagnostics procedures return a clear unsupported
  diagnostic after MySQL-shaped argument validation.

Out of scope:

- Persisted or mutable Performance Schema setup table state.
- Live instrumentation, trace files, histograms, diagnostics capture, and
  statement analyzer snapshots.
- `sys.create_synonym_db()` physical view creation.
- Exact internal routine implementation details, including debug-output
  configuration from `sys.sys_config`.
- Stored-program execution beyond this built-in sys procedure dispatch.

## Compatibility Authority

Primary references:

- MySQL 8.4 Reference Manual, `sys.table_exists()`:
  `https://dev.mysql.com/doc/refman/8.4/en/sys-table-exists.html`
- MySQL 8.4 Reference Manual, `sys.execute_prepared_stmt()`:
  `https://dev.mysql.com/doc/refman/8.4/en/sys-execute-prepared-stmt.html`
- MySQL 8.4 Reference Manual, sys stored procedures index:
  `https://dev.mysql.com/doc/refman/8.4/en/sys-schema-stored-procedures.html`

Runtime expectations are verified against the `mysql:8.4.9` container using
`packages/libmylite/tests/mysql_baseline_sys_procedure_placeholders_expectations.sh`.
Observed MySQL behavior used by this slice:

- `sys.table_exists()` returns `TEMPORARY` before a same-named persistent table,
  `BASE TABLE` for persistent base tables, `VIEW` for views, and `''` for
  missing objects.
- `sys.table_exists()` and `sys.execute_prepared_stmt()` use MySQL error 1318 /
  SQLSTATE `42000` for incorrect argument counts.
- `sys.execute_prepared_stmt('SELECT DATABASE()')` runs with `DATABASE()` equal
  to `sys`; schema-qualified DDL still affects the qualified schema.

## Parser And AST

No grammar expansion is required. MyLite already parses:

```lemon
call_statement ::= CALL table_name.
call_statement ::= CALL table_name LP function_argument_list RP.
function_argument ::= expression.
```

This feature changes runtime dispatch after parsing. Qualified or unqualified
routine names are resolved normally except that the built-in `sys` schema is a
valid routine namespace even when it is not a persisted user schema.

## Runtime Semantics

`CALL sys.table_exists(in_db, in_table, out_var)`:

- Requires exactly three arguments.
- Evaluates `in_db` and `in_table` as session scalar expressions.
- Requires the third argument to be a user variable.
- Checks the session temporary catalog first, then the persistent catalog.
- Sets the OUT variable to:
  - `TEMPORARY` for a matching session temporary table.
  - `VIEW` for a persistent view descriptor.
  - `BASE TABLE` for a persistent non-view table descriptor.
  - `''` for missing schema, missing object, `NULL`, or empty names.
- Produces an OK result with no columns and affected rows `0`.

`CALL sys.execute_prepared_stmt(in_query)`:

- Requires exactly one argument.
- Evaluates the argument as a session scalar string.
- Runs the resulting SQL through MyLite's existing expanded prepared-statement
  execution helper.
- Temporarily sets the selected schema to `sys` for the inner execution and
  restores the caller schema after success or failure.
- Inherits MyLite's existing prepared-statement limitations and diagnostics for
  unsupported SQL inside the supplied string.

Setup no-op procedures:

- Validate exact MySQL argument count.
- Return a single `summary` column with one deterministic row.
- Do not mutate any global, connection, Performance Schema, or catalog state.

Setup listing procedures:

- Validate exact MySQL argument count.
- Return the expected column names with zero rows.

Unsupported sys procedures:

- Validate exact MySQL argument count.
- Return MySQL error 1235 / SQLSTATE `42000` with a MyLite diagnostic explaining
  that live Performance Schema instrumentation is unavailable.

## Procedure Surface

Real descriptor-backed behavior:

- `sys.table_exists()`
- `sys.execute_prepared_stmt()`

Accepted embedded no-op or empty placeholder behavior:

- `sys.ps_setup_disable_background_threads()`
- `sys.ps_setup_disable_consumer()`
- `sys.ps_setup_disable_instrument()`
- `sys.ps_setup_disable_thread()`
- `sys.ps_setup_enable_background_threads()`
- `sys.ps_setup_enable_consumer()`
- `sys.ps_setup_enable_instrument()`
- `sys.ps_setup_enable_thread()`
- `sys.ps_setup_reload_saved()`
- `sys.ps_setup_reset_to_default()`
- `sys.ps_setup_save()`
- `sys.ps_setup_show_disabled()`
- `sys.ps_setup_show_disabled_consumers()`
- `sys.ps_setup_show_disabled_instruments()`
- `sys.ps_setup_show_enabled()`
- `sys.ps_setup_show_enabled_consumers()`
- `sys.ps_setup_show_enabled_instruments()`
- `sys.ps_truncate_all_tables()`

Accepted with unsupported diagnostics:

- `sys.create_synonym_db()`
- `sys.diagnostics()`
- `sys.ps_statement_avg_latency_histogram()`
- `sys.ps_trace_statement_digest()`
- `sys.ps_trace_thread()`
- `sys.statement_performance_analyzer()`

## Storage, Transactions, And Performance

This slice does not add storage. `table_exists()` reads existing in-memory
temporary metadata and durable catalog descriptors. `execute_prepared_stmt()`
delegates to the normal SQL execution path, so transaction and catalog behavior
comes from the inner statement exactly as if MyLite executed that statement
directly under the temporary `sys` default schema.

The dispatch table is static and per-call work is bounded by argument
evaluation, a small routine-name lookup, and existing catalog indexes. No
SQLite fork hook is needed.

## Tests

- MySQL expectation script:
  `packages/libmylite/tests/mysql_baseline_sys_procedure_placeholders_expectations.sh`
- MyLite runtime test:
  `packages/libmylite/tests/runtime_sys_procedure_placeholders_test.c`

The tests cover descriptor-backed table existence, routine-context dynamic SQL,
schema restoration, schema-qualified DDL through `execute_prepared_stmt()`,
representative no-op and empty placeholder procedures, unsupported diagnostics,
and MySQL-shaped arity errors.
