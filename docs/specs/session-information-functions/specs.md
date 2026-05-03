# Session information functions

## Scope

This slice implements MySQL-compatible scalar session and information functions
that expose handle-owned state rather than table data:

- `DATABASE()`
- `SCHEMA()`
- `VERSION()`
- `LAST_INSERT_ID()`
- `LAST_INSERT_ID(expr)`
- `ROW_COUNT()`

The functions are available anywhere MyLite already evaluates supported scalar
function calls, including no-table `SELECT` and table-backed `SELECT`
projection expressions. Predicate and DML expression contexts may accept these
calls when the surrounding expression evaluator already supports function calls,
but the primary compatibility target for this slice is result expressions and
the handle state that client libraries observe after statements complete.
Session-dependent no-table `SELECT` expressions are evaluated when the statement
is stepped, not when it is prepared, so prepared statements observe execution
time schema, row-count, and insert-id state.

Out of scope:

- `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_USER()`,
  `CONNECTION_ID()`, `FOUND_ROWS()`, `CHARSET()`, `COLLATION()`,
  `CURRENT_ROLE()`, and other information functions.
- Stored routine default-database semantics. MyLite does not yet implement
  stored routines, so `DATABASE()` and `SCHEMA()` read only the connection's
  selected schema.
- Replication warnings for `VERSION()`.
- Exact MySQL numeric diagnostic text for every unsupported arity. MyLite should
  report deterministic prepare-time failure for unsupported counts and can
  refine numeric error codes when the diagnostic subsystem exposes them.

## Sources

- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4.9 runtime probes supplied with this task.

This specification is independently authored from official MySQL documentation
and observed MySQL 8.4.9 behavior. It does not copy MySQL grammar text,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

The official MySQL information-functions documentation defines these functions
as session or server information accessors. The supplied MySQL 8.4.9 probes
pin the details that matter for this MyLite slice:

- `DATABASE()` returns the selected default schema name. If no schema is
  selected, or if the selected schema is dropped, it returns `NULL`.
- `SCHEMA()` is a synonym for `DATABASE()`.
- `VERSION()` returns the server version string. MyLite returns
  `mylite_version()` so client code sees the embedded runtime version.
- `LAST_INSERT_ID()` returns the session's remembered auto-increment id.
- `LAST_INSERT_ID(expr)` evaluates `expr`, stores its unsigned-integer value in
  the session, and returns that value.
- `LAST_INSERT_ID(NULL)` returns `NULL` and resets the session value observed by
  later no-argument calls to `0`.
- `LAST_INSERT_ID(-5)` stores and returns `18446744073709551611`, matching
  MySQL's unsigned 64-bit interpretation.
- `ROW_COUNT()` returns the affected-row count for the previous completed
  statement. Successful result-set statements become the latest completed
  statement with value `-1`.
- DDL and session-control statements completed by this slice report `0` unless
  their existing statement implementation already reports a more specific value.
- `INSERT`, `UPDATE`, and `DELETE` report their MyLite affected-row value.
- Explicit inserts into an auto-increment column do not change
  `LAST_INSERT_ID()` unless MySQL would report a generated id.

Arity behavior from the probes:

- `DATABASE(1)`, `SCHEMA(1)`, and `ROW_COUNT(1)` are MySQL syntax errors.
- `VERSION(1)` and `LAST_INSERT_ID(1, 2)` are MySQL error 1582, incorrect
  parameter count.
- MyLite's generic function-call parser may initially parse those calls, but
  binding must reject unsupported arities before execution. Future parser
  refinement can move the zero-argument-only functions to exact syntax errors.

## MyLite Lemon grammar snippets

MyLite already has generic function-call grammar. The intended accepted surface
for this slice can be represented as:

```lemon
scalar_expr(A) ::= session_info_function(A).

session_info_function(A) ::= DATABASE LP RP.
session_info_function(A) ::= SCHEMA LP RP.
session_info_function(A) ::= VERSION LP RP.
session_info_function(A) ::= LAST_INSERT_ID LP RP.
session_info_function(A) ::= LAST_INSERT_ID LP expr(X) RP.
session_info_function(A) ::= ROW_COUNT LP RP.
```

The snippets are descriptive for MyLite's own grammar and are not copied from
MySQL. If these functions remain under generic function-call parsing, the binder
must enforce the same arity set.

## Runtime semantics

### `DATABASE()` and `SCHEMA()`

These functions read `mylite_db.selected_schema`.

- If `selected_schema` is non-`NULL`, return it as text.
- If no schema is selected, return SQL `NULL`.
- `USE schema_name` updates the selected schema through the existing schema
  lifecycle implementation.
- `DROP DATABASE selected_schema` clears the selected schema through the
  existing schema lifecycle implementation; later `DATABASE()` returns `NULL`.
- The functions do not inspect SQLite attached databases or filesystem names.
  MyLite schemas are catalog rows inside the single `.mylite` file.

### `VERSION()`

`VERSION()` returns the result of `mylite_version()` as a non-`NULL` text value.
It has no side effects and does not read or mutate session state.

### `LAST_INSERT_ID()`

The no-argument form returns `mylite_db.last_insert_id`.

Existing insert execution already updates this handle-owned state when MyLite
generates an auto-increment id for an insert path. This public state remains
observable through `mylite_last_insert_id(database)`.

### `LAST_INSERT_ID(expr)`

The one-argument form evaluates `expr` exactly once for each function
evaluation. Preparing a statement that contains `LAST_INSERT_ID(expr)` must not
change `mylite_db.last_insert_id`; the side effect belongs to statement
execution.

- If `expr` evaluates to `NULL`, return `NULL` and set
  `mylite_db.last_insert_id` to `0`.
- Otherwise convert `expr` using MyLite's unsigned integer conversion rules,
  store the resulting `uint64_t` in `mylite_db.last_insert_id`, and return it as
  an unsigned integer expression value.
- Negative signed integers are interpreted as unsigned 64-bit values by
  two's-complement wrap. The verified `-5` case stores and returns
  `18446744073709551611`.
- Evaluation must be handle-owned; no process-global last-insert state is
  introduced.

### `ROW_COUNT()`

MyLite must add handle-owned previous-statement row-count state separate from
`mylite_stmt.affected_rows`.

- `mylite_stmt.affected_rows` continues to describe the current statement
  handle for the public `mylite_affected_rows(stmt)` API.
- `mylite_db.previous_row_count` stores the value returned by `ROW_COUNT()`.
- A statement updates `previous_row_count` when it successfully completes.
- Result-set statements update the value to `-1` only when the result set is
  complete, not when the first row is produced. This lets `SELECT ROW_COUNT()`
  read the value from the previous statement; after that result statement is
  drained, a later `ROW_COUNT()` returns `-1`.
- Failed statements do not claim support for exact MySQL diagnostics-state
  behavior in this slice; they should leave the previous successful value
  unchanged where practical.

## Result metadata

With `SET NAMES utf8mb4`, MySQL 8.4.9 reports:

| Expression | Type | Charset/collation | Length | Decimals | Nullability | Flags |
| --- | --- | --- | --- | --- | --- | --- |
| `DATABASE()` | `VAR_STRING` | connection result charset, `utf8mb4_0900_ai_ci` in the probe | 256 | 31 | nullable | none |
| `SCHEMA()` | `VAR_STRING` | connection result charset, `utf8mb4_0900_ai_ci` in the probe | 256 | 31 | nullable | none |
| `VERSION()` | `VAR_STRING` | connection result charset | 20 | 31 | not null | `NOT_NULL` |
| `LAST_INSERT_ID()` | `LONGLONG` | binary numeric | 21 | 0 | not null | `NOT_NULL`, `UNSIGNED`, `BINARY`, `NUM` |
| `LAST_INSERT_ID(expr)` | `LONGLONG` | binary numeric | 21 | 0 | not null | `NOT_NULL`, `UNSIGNED`, `BINARY`, `NUM` |
| `ROW_COUNT()` | `LONGLONG` | binary numeric | 21 | 0 | not null | `NOT_NULL`, `BINARY`, `NUM` |

MyLite should use the active connection result charset id for text-returning
functions. For `SET NAMES utf8mb4`, that is charset id `255`.

`LAST_INSERT_ID(expr)` is metadata-not-null even though a `NULL` argument returns
`NULL` at runtime in the verified probe. This follows MySQL's reported metadata
and is intentionally different from ordinary NULL-propagating functions.

## Errors and warnings

- Unsupported arity is rejected during prepare/bind. MyLite's current public
  status may be `MYLITE_UNSUPPORTED`; the error-condition code can be refined
  to 1582 later when function-arity diagnostics are represented directly.
- The functions do not produce warnings for normal successful evaluation.
- Unsigned conversion for `LAST_INSERT_ID(expr)` should follow the existing
  expression conversion warning behavior. This slice's required probes do not
  require warning assertions for negative integer input.

## Storage and performance implications

No `.mylite` file-format or SQLite storage change is required.

The implementation adds one small field to `mylite_db` for previous
row-count state. Function evaluation should remain direct and O(1), except for
the normal cost of evaluating the optional `LAST_INSERT_ID(expr)` argument.

No third-party dependencies are introduced.

## Test plan

Fast C tests must cover:

- Parser acceptance of the supported function forms in no-table and table-backed
  `SELECT` expressions.
- Prepare-time rejection of unsupported arities for the implemented functions.
- `DATABASE()` and `SCHEMA()` with no selected schema, after `USE`, and after
  dropping the selected schema.
- `VERSION()` matching `mylite_version()`.
- `LAST_INSERT_ID()` after generated auto-increment inserts, explicit
  auto-increment inserts, `UPDATE`, and `DELETE`.
- `LAST_INSERT_ID(expr)` with positive integer, `NULL`, and negative integer
  arguments, including the public `mylite_last_insert_id()` side effect.
- `ROW_COUNT()` after `INSERT`, table-backed `SELECT`, no-table `SELECT`,
  `UPDATE`, and `DELETE`, including the transition where a drained result-set
  statement makes the next `ROW_COUNT()` return `-1`.
- Table-backed projection of these functions where the existing evaluator
  supports it.
- Result metadata for all functions under `SET NAMES utf8mb4`.

## Implementation handoff

Implementation should:

1. Extend the scalar function registry to recognize the six function names and
   enforce supported arities.
2. Add a context callback for session information functions so the expression
   module does not own `mylite_db` internals.
3. Mark session functions as non-cacheable no-table expressions so
   `LAST_INSERT_ID(expr)` and statement-state functions are evaluated at the
   correct time in table-backed expressions.
4. Add handle-owned `previous_row_count` state and update it once when a
   statement successfully completes.
5. Infer function metadata explicitly instead of deriving these descriptors from
   runtime values.
6. Keep `mylite_affected_rows()` and `mylite_last_insert_id()` ABI behavior
   unchanged.
