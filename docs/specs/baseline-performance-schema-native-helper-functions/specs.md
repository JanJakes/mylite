# Baseline Performance Schema Native Helper Functions

## Scope

This slice implements the MySQL 8.4.9 native Performance Schema helper
functions that supersede selected deprecated `sys` schema stored functions:

- `FORMAT_BYTES(count)`
- `FORMAT_PICO_TIME(time_val)`
- `PS_CURRENT_THREAD_ID()`
- `PS_THREAD_ID(connection_id)`

The functions are accepted as unqualified built-ins in scalar expressions, both
without a row source and in row-backed expressions. They are not valid with a
schema qualifier. Existing `sys.format_bytes()`, `sys.format_time()`, and
`sys.ps_thread_id()` behavior remains documented separately because MySQL keeps
minor semantic differences for deprecated sys helpers.

Out of scope:

- Live Performance Schema event collection.
- Background thread inventory.
- Performance Schema disabled-mode error behavior.
- Mutable `performance_schema` setup tables or server-wide instrumentation
  configuration.
- The deprecated sys-helper name-conflict note emitted by MySQL for
  `sys.ps_thread_id()`.

## MySQL Authority

Compatibility is based on the MySQL 8.4 Reference Manual, section
`14.21 Performance Schema Functions`, and MySQL 8.4.9 runtime probes against
`mylite-mysql-849`.

The manual defines these helpers as unqualified built-in functions usable from
any schema. It also records the key differences from the deprecated `sys`
functions: `FORMAT_PICO_TIME()` uses `min` rather than `m` for minutes and does
not use a week unit, while `PS_THREAD_ID(NULL)` returns `NULL` even though
`sys.ps_thread_id(NULL)` returns the current connection thread id.

Observed MySQL 8.4.9 behavior:

```sql
SELECT FORMAT_BYTES(NULL), FORMAT_BYTES(0), FORMAT_BYTES(1),
       FORMAT_BYTES(512), FORMAT_BYTES(1024), FORMAT_BYTES(1536),
       FORMAT_BYTES(18446644073709551615), FORMAT_BYTES('abc');
```

returns:

```text
NULL |    0 bytes |    1 bytes |  512 bytes | 1.00 KiB | 1.50 KiB | 16.00 EiB |    0 bytes
Warning 1292: Truncated incorrect DOUBLE value: 'abc'
```

```sql
SELECT FORMAT_PICO_TIME(NULL), FORMAT_PICO_TIME(0),
       FORMAT_PICO_TIME(999), FORMAT_PICO_TIME(1000),
       FORMAT_PICO_TIME(3501), FORMAT_PICO_TIME(1000000000),
       FORMAT_PICO_TIME(60000000000000),
       FORMAT_PICO_TIME(188732396662000), FORMAT_PICO_TIME('abc');
```

returns:

```text
NULL |   0 ps | 999 ps | 1.00 ns | 3.50 ns | 1.00 ms | 1.00 min | 3.15 min |   0 ps
Warning 1292: Truncated incorrect DOUBLE value: 'abc'
```

```sql
SELECT PS_CURRENT_THREAD_ID() IS NULL,
       PS_THREAD_ID(CONNECTION_ID()) = PS_CURRENT_THREAD_ID(),
       PS_THREAD_ID(NULL), PS_THREAD_ID(999999), PS_THREAD_ID(-1);
```

returns:

```text
0 | 1 | NULL | NULL | NULL
```

Invalid string thread ids return `NULL` and produce truncation warnings:

```sql
SELECT PS_THREAD_ID('abc'), PS_THREAD_ID('1abc');
SHOW WARNINGS;
```

returns two `NULL` values and warnings `1292` for truncated incorrect integer
values.

Parameter-count diagnostics use `1582 / 42000` with the native function name.
Schema-qualified forms such as `sys.PS_CURRENT_THREAD_ID()` and
`sys.FORMAT_PICO_TIME(1)` fail with `1305 / 42000` because the built-ins are not
sys schema routines.

## Runtime Semantics

The functions are implemented in MyLite's sys-helper runtime module but exposed
through explicit native descriptors. They are registered as SQLite scalar UDFs
through SQLite's public extension API. No targeted SQLite fork hook is needed:
the current UDF path gives SQLite enough information to execute row-backed
calls and lets MyLite keep MySQL-specific coercion, diagnostics, and metadata
in the compatibility layer.

MyLite uses its session `connection_id` as the synthetic Performance Schema
`THREAD_ID`. This matches the current `sys.processlist`, `sys.session`,
`sys.x$session`, and deprecated sys Performance Schema helper rows without
introducing a second thread-id allocator.

### Function Details

- `FORMAT_BYTES(count)` returns `NULL` for `NULL`, formats byte values through
  `EiB`, rounds non-byte units to two decimal places, and formats invalid
  numeric text as zero while emitting a truncation warning where MyLite's
  warning surface supports it.
- `FORMAT_PICO_TIME(time_val)` returns `NULL` for `NULL`, formats picoseconds
  through days, uses `min` for minutes, and formats invalid numeric text as
  zero while emitting a truncation warning where supported.
- `PS_CURRENT_THREAD_ID()` returns the current MyLite synthetic thread id as an
  unsigned `BIGINT` scalar result.
- `PS_THREAD_ID(connection_id)` returns `NULL` for `NULL`, negative values,
  invalid values, or unknown connection ids. It returns the same synthetic
  thread id for known MyLite connection ids.

The implementation intentionally does not emit the MySQL note for deprecated
`sys.ps_thread_id()`, because that note belongs to the broader sys stored
function warning surface and existing MyLite sys helper docs already classify
it as a known gap.

## Syntax

The existing generic-function path handles the calls:

```lemon
expr ::= generic_function_call.
generic_function_call ::= ident LP opt_expr_list RP.
```

The sys-helper lookup boundary distinguishes native unqualified names from
qualified `sys.<name>(...)` calls:

```lemon
native_performance_schema_function ::= FORMAT_BYTES LP expr RP.
native_performance_schema_function ::= FORMAT_PICO_TIME LP expr RP.
native_performance_schema_function ::= PS_CURRENT_THREAD_ID LP RP.
native_performance_schema_function ::= PS_THREAD_ID LP expr RP.
```

No parser grammar change is required because the generic-function AST already
captures these calls and the runtime lookup provides arity and qualification
validation.

## Diagnostics

- Incorrect parameter counts raise `1582 / 42000` using the native function
  name.
- Schema-qualified native calls raise the normal unknown-function diagnostic.
- Invalid numeric text for formatting functions or `PS_THREAD_ID()` should
  produce MySQL truncation warnings when MyLite has warning plumbing for the
  executing context; the value result remains the MySQL-compatible zero or
  `NULL`.

## Metadata

- `FORMAT_BYTES()` and `FORMAT_PICO_TIME()` return nullable character strings.
- `PS_CURRENT_THREAD_ID()` returns non-null unsigned `BIGINT`.
- `PS_THREAD_ID()` returns nullable unsigned `BIGINT`.

## Tests

Focused C tests cover:

- direct unqualified native calls;
- row-backed native calls lowered to SQLite UDFs;
- the native/sys `PS_THREAD_ID(NULL)` semantic split;
- invalid argument coercion for formatting and thread id helpers;
- arity diagnostics;
- schema-qualified rejection through the normal unknown-function path;
- scalar result metadata for unsigned thread-id values where existing metadata
  accessors expose it.

The MySQL expectation script records the MySQL 8.4.9 values and diagnostics used
for this slice.
