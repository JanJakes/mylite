# Baseline Loadable Function Diagnostics

This slice covers MySQL 8.4.9 loadable function DDL:

- `CREATE [AGGREGATE] FUNCTION [IF NOT EXISTS] name RETURNS type SONAME 'library'`
- `DROP FUNCTION [IF EXISTS] name`

MySQL uses these statements to register and unregister native shared-library
UDF entry points in the server. MyLite is an embedded single-file database
library and must not load arbitrary MySQL server plugins or shared objects.
The compatible embedded behavior is therefore explicit parser acceptance and a
predictable unsupported diagnostic.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-function-loadable.html
- https://dev.mysql.com/doc/refman/8.4/en/drop-function-loadable.html
- https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html
- https://dev.mysql.com/doc/refman/8.4/en/drop-procedure.html

## MySQL 8.4.9 Observations

Runtime probes were executed against the local `mysql:8.4.9` container
`mylite-mysql-849`.

```sql
CREATE FUNCTION mylite_missing_udf RETURNS INTEGER SONAME 'missing_mylite_udf.so';
CREATE FUNCTION IF NOT EXISTS mylite_missing_udf RETURNS STRING SONAME 'missing_mylite_udf.so';
CREATE AGGREGATE FUNCTION mylite_missing_aggr RETURNS REAL SONAME 'missing_mylite_udf.so';
CREATE FUNCTION mylite_bad_udf RETURNS BOGUS SONAME 'missing_mylite_udf.so';
DROP FUNCTION mylite_missing_udf;
DROP FUNCTION IF EXISTS mylite_missing_udf;
```

Observed behavior:

- `CREATE FUNCTION ... SONAME ...`, `CREATE FUNCTION IF NOT EXISTS ...`, and
  `CREATE AGGREGATE FUNCTION ... SONAME ...` are accepted by the MySQL grammar
  and fail with `1126 / HY000` when the shared library is missing.
- Unsupported return types such as `RETURNS BOGUS` are syntax errors.
- `DROP FUNCTION missing_name` fails with `1305 / 42000`.
- `DROP FUNCTION IF EXISTS missing_name` succeeds, appends note `1305`, leaves
  `@@warning_count = 1`, and makes `ROW_COUNT()` report `-1`.

## Scope

MyLite accepts representative loadable function DDL through the existing
post-failure placeholder classifier and maps it to the unsupported stored
program/loadable-function diagnostic:

- `CREATE FUNCTION name RETURNS STRING SONAME 'library'`
- `CREATE FUNCTION IF NOT EXISTS name RETURNS INTEGER SONAME 'library'`
- `CREATE AGGREGATE FUNCTION name RETURNS REAL SONAME 'library'`
- `DROP FUNCTION name`
- `DROP FUNCTION IF EXISTS name`

Runtime behavior:

- execution fails with `MYLITE_ERROR`;
- diagnostics are `1064 / 42000`;
- the message contains `stored program or loadable function statement is not
  supported`;
- no result rows, warnings, user data, catalogs, function registry, plugin
  state, files, variables, or transactions are modified.

MyLite intentionally does not attempt to mimic MySQL's `1126` shared-library
load failure or `1305` missing-UDF drop behavior. Doing so would imply a
server plugin directory and loadable function registry that MyLite does not
have. The embedded contract is to reject the feature before any native-code
loading or registration is attempted.

## Out Of Scope

This slice does not implement:

- native shared-library loading;
- UDF entry-point discovery, initialization, deinitialization, aggregate state,
  or result typing;
- `mysql.func` mutation or persisted loadable function registry entries;
- privilege checks;
- plugin directory handling;
- distinguishing a missing stored function from a missing loadable function at
  runtime.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned placeholder shape. The
normal Lemon grammar remains the authority for implemented SQL; these forms are
recognized by the post-failure classifier.

```text
unsupported_routine_or_loadable_function_statement:
    CREATE FUNCTION ... SONAME ...
  | CREATE AGGREGATE FUNCTION ... SONAME ...
  | DROP FUNCTION ...
```

`DROP FUNCTION` is syntactically shared by stored and loadable functions. Until
MyLite implements stored functions or a loadable-function registry, both paths
use the same unsupported diagnostic.

## Runtime Architecture

No SQLite extension API, MyLite catalog change, dynamic loader integration, or
SQLite fork hook is needed. The statement is represented as the existing raw
unsupported stored-program AST node and routed to the existing runtime
unsupported diagnostic executor.

## Tests

Focused coverage includes:

- MySQL 8.4.9 expectation probes for representative loadable function DDL
  acceptance and diagnostics;
- parser classification for `CREATE FUNCTION ... SONAME`,
  `CREATE AGGREGATE FUNCTION ... SONAME`, `CREATE FUNCTION IF NOT EXISTS ...`,
  `DROP FUNCTION`, and `DROP FUNCTION IF EXISTS`;
- runtime unsupported diagnostics for the same forms;
- compatibility documentation updates for SQL routines and embedded design
  decisions.

## Compatibility Status

This slice turns loadable function DDL diagnostics into a covered embedded
baseline. Loadable functions remain unsupported by design until MyLite has an
explicit native-extension registration policy and implementation.
