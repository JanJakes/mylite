# Baseline GROUP_CONCAT Max Length System Variable

## Status

This feature extends the existing limited `GROUP_CONCAT()` aggregate with the
smallest MySQL-compatible `@@group_concat_max_len` surface that makes the
current aggregate slice observable and useful:

- scalar reads for session and global scopes;
- `SHOW VARIABLES` rows;
- session-local `SET` assignments;
- byte-capped `GROUP_CONCAT()` accumulation for the already supported
  descriptor-driven aggregate grammar;
- MySQL-compatible truncation warnings for capped aggregate output.

MyLite still does not implement mutable server-global system-variable state.
Global reads expose the fixed default value, and `SET GLOBAL` is rejected with
the same embedded-design boundary used by the other mutable session variables.

## Sources

- MyLite README architecture: `README.md`
- Engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline GROUP_CONCAT aggregate:
  `docs/specs/baseline-group-concat-aggregate/specs.md`
- Baseline SHOW VARIABLES:
  `docs/specs/baseline-show-variables/specs.md`
- Baseline mutable SQL select limit:
  `docs/specs/baseline-mutable-sql-select-limit/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Runtime system-variable compatibility:
  `docs/compatibility/runtime-system-variables.md`
- Aggregate function compatibility:
  `docs/compatibility/functions-aggregate.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, aggregate function descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, `SET` variable assignment:
  https://dev.mysql.com/doc/refman/8.4/en/set-variable.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_group_concat_max_len_system_variable_expectations.sh`
records the MySQL 8.4.9 probes for this feature. Observed behavior:

- `@@group_concat_max_len`, `@@SESSION.group_concat_max_len`, and
  `@@GLOBAL.group_concat_max_len` read `1024` by default.
- `SHOW VARIABLES LIKE 'group_concat_max_len'` returns one row with value
  `1024`.
- The official variable metadata reports global/session scope, dynamic status,
  integer type, default `1024`, minimum `4`, and maximum
  `18446744073709551615` on 64-bit platforms.
- Unscoped, `SESSION`, `LOCAL`, direct `@@variable`, `@@SESSION.variable`, and
  `@@LOCAL.variable` assignments mutate the current session value.
- `SET SESSION group_concat_max_len = DEFAULT` restores the session value to
  the current global value. MyLite's global value remains fixed at `1024`, so
  `DEFAULT` restores `1024`.
- Integer assignments below `4`, including `0`, `1`, negative integers,
  `TRUE`, and `FALSE`, store `4` and emit warning `1292 HY000` with message
  `Truncated incorrect group_concat_max_len value: '<text>'`.
- `+integer` stores the positive integer when it is in range. Integer-typed
  user variables use the same conversion; string-typed user variables are
  rejected even when their text is numeric.
- String, decimal, `NULL`, and integer literals above
  `18446744073709551615` fail with error `1232 42000` and message
  `Incorrect argument type to variable 'group_concat_max_len'`.
- `GROUP_CONCAT()` output is capped in bytes. When the cap truncates a
  nonbinary string value, MySQL returns a valid character prefix rather than an
  invalid partial UTF-8 sequence.
- A truncation emits warning `1260 HY000` with message
  `Row N was cut by GROUP_CONCAT()`. The observed `N` is the one-based ordinal
  of the non-`NULL` aggregate value that first exceeded the cap in the
  statement's aggregate input order.
- Each truncated aggregate group emits at most one warning. Later values in
  the same already-truncated group do not add more warnings.

## Scope

Supported SQL examples:

```sql
SELECT @@group_concat_max_len
SELECT @@GLOBAL.group_concat_max_len, @@SESSION.group_concat_max_len
SHOW VARIABLES LIKE 'group_concat_max_len'
SET group_concat_max_len = 8
SET SESSION group_concat_max_len = DEFAULT
SET LOCAL group_concat_max_len = +16
SET @@group_concat_max_len = TRUE
SET @n = 12
SET group_concat_max_len = @n
SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM t
SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM t GROUP BY g
```

The implementation must:

- store `group_concat_max_len` in handle-local session state and initialize it
  to `1024`;
- expose scalar reads for no scope, `SESSION`, `LOCAL`, and fixed `GLOBAL`;
- include `group_concat_max_len` in `SHOW VARIABLES` and
  `SHOW GLOBAL VARIABLES`;
- snapshot and restore it across multi-assignment `SET` failure rollback;
- support `DEFAULT`, unsigned integer literals, parenthesized integer
  literals, `+integer`, `-integer`, `TRUE`, `FALSE`, and integer-typed user
  variables;
- clamp values below `4` to `4` and append the MySQL-compatible warning;
- reject unsupported value forms with MySQL-compatible diagnostics;
- use a MyLite-owned SQLite aggregate function for the existing supported
  `GROUP_CONCAT()` grammar so accumulation stops at the configured byte cap;
- preserve valid UTF-8 prefixes for current nonbinary string results;
- append truncation warning `1260 HY000` for each group whose output is cut;
- leave catalog rows, descriptor versions, descriptor caches, catalog
  generation, SQLite schema generation, and `.mylite` preamble bytes
  unchanged.

## Non-Goals

This feature does not implement:

- mutable server-global `group_concat_max_len`;
- startup options, persisted variables, Performance Schema variable tables,
  privilege semantics, or `SET_VAR` optimizer hints;
- result metadata changes around MySQL's documented binary/nonbinary and
  `<= 512` threshold behavior;
- `GROUP_CONCAT(DISTINCT ...)`, unsupported expression arguments, expression or
  ordinal ordering, nullable or string aggregate-local order keys, multiple
  aggregate-local order keys, window functions, joins, or any other
  `GROUP_CONCAT()` form deferred by
  `baseline-group-concat-aggregate`;
- binary string truncation, invalid UTF-8 output, or collation-sensitive
  aggregate-local ordering;
- arbitrary SQLite pass-through or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement
  orchestration, public result ownership, diagnostics snapshots, and cleanup on
  failure.
- Statement context owns live and previous diagnostics. `GROUP_CONCAT()` cap
  warnings are appended to the current statement diagnostics before the
  successful result is finalized.
- Lexer/parser/AST already admit `SYSTEM_VARIABLE` expressions and `SET`
  system-variable targets. This feature adds no new tokens or grammar beyond
  the existing `GROUP_CONCAT()` grammar.
- Runtime system-variable handling owns value parsing, clamping warnings,
  session-state mutation, scalar readback, fixed global readback, and
  `SHOW VARIABLES` display.
- Analyzer/planner remains descriptor-driven for aggregate value columns,
  aggregate-local order columns, predicates, grouped columns, aliases, and
  generated SQL. It switches the generated aggregate function name from
  SQLite's built-in `group_concat` to a MyLite-owned aggregate with the same
  supported argument shape.
- The catalog module remains the logical authority for user tables and columns.
  This feature reads descriptors but does not mutate catalog metadata.
- The result builder owns copied result rows. The aggregate returns `NULL` when
  all input values are `NULL`; otherwise it returns a capped UTF-8 byte string
  copied by the existing result API.
- SQLite physical execution still owns scans, filtering, grouping, and
  aggregate-local ordering. MyLite uses public SQLite aggregate registration
  and aggregate `ORDER BY` invocation syntax. No SQLite fork patch is required.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Supported reads and session-variable assignments must not write through byte
  range `[0, 4096)`.

## Grammar

The existing MyLite grammar shape is sufficient for system-variable reads and
assignments:

```lemon
expression ::= SYSTEM_VARIABLE.

set_assignment ::= set_system_variable_target EQ expr_or_default.
set_assignment ::= scope_keyword system_variable_name EQ expr_or_default.

expr_or_default ::= expression.
expr_or_default ::= DEFAULT.
expression ::= INTEGER.
expression ::= PLUS INTEGER.
expression ::= MINUS INTEGER.
expression ::= TRUE.
expression ::= FALSE.
expression ::= USER_VARIABLE.
expression ::= LP expression RP.
```

The existing limited aggregate grammar remains:

```lemon
expression ::=
    GROUP_CONCAT LPAREN qualified_identifier
        group_concat_order_opt group_concat_separator_opt RPAREN.

group_concat_order_opt ::= .
group_concat_order_opt ::= ORDER BY qualified_identifier order_direction_opt.

group_concat_separator_opt ::= .
group_concat_separator_opt ::= SEPARATOR STRING.
```

These snippets describe MyLite's admitted subset and are independently
authored for this project.

## System Variable Semantics

Session storage:

- New handles start with `group_concat_max_len = 1024`.
- Close/reopen starts a new session with the default value; the value is not
  stored in `.mylite` files.
- Independent handles have independent session values.
- Global reads always return `1024`.

Assignment:

- `DEFAULT` stores `1024`.
- `TRUE` is converted as integer `1`, clamps to `4`, and warns.
- `FALSE` is converted as integer `0`, clamps to `4`, and warns.
- A nonnegative integer literal stores the exact parsed `uint64_t` value when
  it is at least `4`.
- A positive sign is ignored.
- A negative integer literal stores `4` and appends warning `1292 HY000`.
- An integer-typed user variable stores its current integer value using the
  same range and minimum-clamp behavior.
- Unsupported literal kinds and unsigned overflow fail before mutation.
- Multi-assignment `SET` remains atomic: if any assignment fails, earlier
  session mutations in the same `SET` statement are rolled back.

`SET GLOBAL group_concat_max_len = ...` fails with MyLite's existing
unsupported global-assignment diagnostic. This is an explicit embedded-design
limitation, not a MySQL behavior claim.

## GROUP_CONCAT Runtime Semantics

The current `GROUP_CONCAT()` source, grouping, value-column, order-column, and
separator limits stay unchanged.

The generated SQLite SQL calls MyLite's private aggregate function:

```sql
_mylite_group_concat(value_column [ , ?separator ] ORDER BY order_column ASC)
```

or the corresponding `DESC` order. Identifiers remain descriptor-derived and
quoted. Separator, predicate, and limit values remain bound parameters.

The aggregate step:

- ignores `NULL` value arguments;
- increments a statement-local non-`NULL` aggregate value ordinal for each
  non-`NULL` value;
- converts integer inputs through SQLite's text representation, matching the
  current descriptor envelope for integer value columns;
- reads nonbinary string inputs as UTF-8 bytes;
- appends the separator before each value after the first non-`NULL` value in
  the group;
- stops appending once the session cap is reached;
- if separator or value bytes would exceed the cap, appends only the valid
  prefix that fits, marks that group truncated, records the first cut ordinal,
  and ignores the rest of that group's values.

For current nonbinary string output, a truncated prefix must end at a UTF-8
codepoint boundary. If the byte cap falls inside a multibyte character, MyLite
backs off to the previous valid boundary. This can leave the result shorter
than `@@group_concat_max_len`, matching the observed MySQL behavior for UTF-8
strings. ASCII values therefore truncate exactly at the byte cap.

The aggregate finalizer returns:

- SQL `NULL` when no non-`NULL` value was seen;
- the accumulated byte string otherwise.

When a group was truncated, the finalizer appends one warning:

| Code | SQLSTATE | Message |
| --- | --- | --- |
| `1260` | `HY000` | `Row N was cut by GROUP_CONCAT()` |

`N` is the one-based non-`NULL` aggregate value ordinal captured when the group
first exceeded the cap. For the current one-aggregate, one-table supported
forms, this matches the MySQL 8.4.9 observations.

## Diagnostics

Supported in-range assignments and uncapped aggregate output report zero
warnings. Assignment clamping appends:

| Code | SQLSTATE | Message |
| --- | --- | --- |
| `1292` | `HY000` | `Truncated incorrect group_concat_max_len value: '<text>'` |

Unsupported assignment values fail with:

| Code | SQLSTATE | Message |
| --- | --- | --- |
| `1232` | `42000` | `Incorrect argument type to variable 'group_concat_max_len'` |

Unknown variable names, malformed system-variable paths, quoted scope names,
unsupported global mutation, allocation failure, public API misuse, and
physical SQLite failures reuse the existing system-variable and execution
diagnostic policies.

## Tests

Fast C tests must cover:

- default scalar/global/session/local readback and `SHOW VARIABLES` readback;
- supported `SET` spellings, `DEFAULT`, `4`, `8`, `+8`, `1`, `0`, `-1`,
  `TRUE`, `FALSE`, and integer user variables;
- clamp warnings, `SHOW WARNINGS`, `@@warning_count`, and clean warning counts
  after supported in-range statements;
- unsupported string, decimal, `NULL`, and overflow assignment;
- atomic rollback across multi-assignment failure;
- ungrouped and grouped `GROUP_CONCAT()` caps for integer and nonbinary string
  value columns;
- no truncation at exact or larger caps;
- `NULL` all-input behavior;
- truncation that happens in separator bytes, value bytes, and UTF-8 multibyte
  values;
- aggregate-local `ORDER BY` direction remains honored under caps;
- independent handle state and close/reopen default reset;
- preamble and generation preservation.

The MySQL expectation script must verify each user-visible behavior above
against MySQL 8.4.9 before the implementation is accepted.
