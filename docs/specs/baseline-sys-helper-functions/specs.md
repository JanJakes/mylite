# Baseline sys Helper Functions

## Scope

This slice adds executable MySQL 8.4.9-compatible behavior for these `sys`
schema scalar helper functions:

- `sys.extract_schema_from_file_name(path)`
- `sys.extract_table_from_file_name(path)`
- `sys.format_bytes(bytes)`
- `sys.format_path(path)`
- `sys.format_statement(statement)`
- `sys.format_time(picoseconds)`
- `sys.list_add(list, value)`
- `sys.list_drop(list, value)`
- `sys.quote_identifier(identifier)`
- `sys.sys_get_config(variable_name, default_value)`
- `sys.version_major()`
- `sys.version_minor()`
- `sys.version_patch()`

The implementation also accepts the unqualified helper names where MySQL exposes
them in sys view definitions, such as `format_bytes(...)`.

Out of scope:

- Performance Schema-backed helper predicates such as `ps_thread_account()`.
- Stored sys procedures.
- User-managed `sys_config` writes and triggers.
- Exact stored-routine metadata rows for `INFORMATION_SCHEMA.ROUTINES` and
  `INFORMATION_SCHEMA.PARAMETERS` beyond existing routine dependency metadata.

## MySQL Authority

Compatibility is based on the MySQL 8.4 Reference Manual sys stored-function
pages and MySQL 8.4.9 runtime probes against `mylite-mysql-849`.

Observed probe highlights:

- `sys.version_major()`, `sys.version_minor()`, and `sys.version_patch()` return
  `8`, `4`, and `9`.
- `sys.format_bytes(NULL)` returns `NULL`; `0`, `1`, and `999` render as
  right-aligned byte strings; `1024` renders as `1.00 KiB`.
- `sys.format_time(NULL)` returns `NULL`; sub-nanosecond values render as
  picoseconds, and larger values use `ns`, `us`, `ms`, `s`, `m`, `h`, or `d`.
- `sys.format_path(path)` replaces known global path prefixes with
  `@@datadir`, `@@tmpdir`, and `@@basedir` names and otherwise returns the
  input unchanged.
- `sys.format_statement(statement)` truncates long statements using
  `@sys.statement_truncate_len` when present and otherwise the sys config
  default, while preserving short statements.
- `sys.list_add(NULL, 'x')` and `sys.list_add('', 'x')` return `x`; a `NULL`
  second argument is an invalid use of `NULL`.
- `sys.list_drop(NULL, 'x')` returns `NULL`; removal matches comma-prefixed
  values with no space or one space after the comma and preserves the remaining
  bytes except for final leading/trailing comma trimming.
- `sys.quote_identifier()` wraps the input in backticks and doubles embedded
  backticks.
- `sys.sys_get_config(name, default)` returns MySQL sys table defaults for
  known non-`NULL` variables and the supplied fallback for unknown names,
  `NULL` names, and known variables with `NULL` sys config values.

## Syntax

MyLite accepts unqualified helper calls as ordinary generic scalar functions.
For this slice, schema-qualified calls to the selected sys helpers are accepted
by the SQL normalization layer, which rewrites `sys.<helper>(...)` and
`` `sys`.`<helper>`(...) `` to the corresponding unqualified helper call before
parsing. The rewrite is limited to the selected helper allow-list and skips
string literals, comments, and top-level `SET` statements. `SET` expression
support remains governed by the existing SET compatibility slices.

General schema-qualified generic function grammar is left out of this slice
because the compatibility need here is only the MySQL `sys` helper surface, and
the broader grammar route needs separate parser design and performance review.

## Runtime Semantics

The helper functions are deterministic and side-effect free except for
diagnostics. They are available through:

- MyLite scalar evaluation for no-source and `DUAL` projections.
- SQLite UDF callbacks for row-backed SELECT, WHERE, ORDER BY, INSERT, and
  UPDATE expression lowering.

The SQLite path uses public SQLite scalar function registration. No targeted
SQLite fork hook is needed.

### Function Details

- `extract_schema_from_file_name(path)` returns the path segment immediately
  before the file name. With no slash it returns the input string.
- `extract_table_from_file_name(path)` returns the file name without the last
  extension. A trailing slash returns the empty string.
- `format_bytes(bytes)` coerces numeric text using MySQL-style leading numeric
  conversion, uses base-1024 units, and returns `NULL` for `NULL`.
- `format_path(path)` rewrites known prefixes for MyLite's `datadir`, `tmpdir`,
  and `basedir` values. It returns `NULL` for `NULL`.
- `format_statement(statement)` uses `@sys.statement_truncate_len` when present,
  otherwise the sys config default (`64`). Short statements are returned
  unchanged; long statements keep a prefix and suffix separated by ` ... `.
- `format_time(picoseconds)` formats numeric picosecond input into the largest
  useful unit with trimmed fractional zeros. Non-numeric text keeps the input
  and appends ` ps`, matching MySQL's stored-function coercion behavior.
- `list_add(list, value)` appends `value` to a comma-separated list. A `NULL`
  list is treated as empty. A `NULL` value raises error 1138 / SQLSTATE `02200`.
- `list_drop(list, value)` removes MySQL's comma-prefixed value patterns and
  preserves spacing in the remaining list. A `NULL` list returns `NULL`; a
  `NULL` value raises error 1138 / SQLSTATE `02200`.
- `quote_identifier(identifier)` returns a backtick-quoted identifier or `NULL`.
- `sys_get_config(variable_name, default_value)` returns built-in sys table
  defaults. Unknown names, `NULL` names, and known `NULL` values return the
  supplied default; session `@sys.*` user variables do not override
  `sys_get_config()` itself.
- Version helpers return the three numeric components from
  `MYLITE_MYSQL_SERVER_VERSION_STRING`.

## Diagnostics

`list_add()` and `list_drop()` must emit MySQL's invalid-null diagnostic for a
`NULL` value argument:

- code: `1138`
- SQLSTATE: `02200`
- message:
  - `Function sys.list_add: in_add_value input variable should not be NULL`
  - `Function sys.list_drop: in_drop_value input variable should not be NULL`

Wrong argument counts should be rejected before execution.

## Tests

The focused runtime test covers:

- no-source and `DUAL` projections;
- unqualified and schema-qualified helper names;
- row-backed SELECT, WHERE, ORDER BY, INSERT, and UPDATE contexts through
  SQLite UDF lowering;
- `NULL`, empty string, quoting, byte/time formatting, path extraction, path
  rewriting, list add/drop, sys config fallback, and version helpers;
- invalid `NULL` diagnostics for `list_add()` and `list_drop()`.

The expectation script records the MySQL 8.4.9 results used to shape the test
cases.
