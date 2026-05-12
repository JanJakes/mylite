# Baseline SQL Mode Session State

## Status

This feature extends the existing read-only `@@sql_mode` baseline into
connection-local session state for the common `SET sql_mode` forms used during
client bootstrap.

The slice is intentionally partial. MyLite will accept, canonicalize, store, and
reflect MySQL 8.4 SQL-mode names for the supported assignment forms. It will
apply only the effects that already have narrow MyLite ownership points:

- lexer parsing for `ANSI_QUOTES`;
- lexer parsing and runtime string-literal decoding for
  `NO_BACKSLASH_ESCAPES`;
- auto-increment zero handling for `NO_AUTO_VALUE_ON_ZERO`;
- `REAL` type mapping for `REAL_AS_FLOAT`.

Other mode effects remain deferred and explicitly documented. In particular,
setting `sql_mode = ''` must not be documented as non-strict conversion support
until the affected DDL/DML conversion paths implement MySQL's relaxed-mode
warnings and coercions.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline SQL mode scalar:
  `docs/specs/baseline-sql-mode-system-variable/specs.md`
- Baseline fixed `SET` system variables:
  `docs/specs/baseline-set-fixed-system-variables/specs.md`
- Baseline auto increment:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, server SQL modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, using system variables:
  https://dev.mysql.com/doc/refman/8.4/en/using-system-variables.html
- MySQL 8.4 Reference Manual, `SET` variable assignment:
  https://dev.mysql.com/doc/refman/8.4/en/set-variable.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes were run against local container `mylite-mysql-849` using
`mysql:8.4.9`.

The default value for both global and session `sql_mode` was:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

Observed supported assignment examples:

```sql
SET sql_mode = ''
SET sql_mode = 'STRICT_TRANS_TABLES'
SET sql_mode = 'NO_ZERO_DATE'
SET sql_mode = 'NO_ZERO_IN_DATE'
SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO'
SET @@sql_mode = "NO_ENGINE_SUBSTITUTION"
SET SESSION sql_mode = "NO_ZERO_DATE"
SET @@SESSION.sql_mode = "NO_ZERO_IN_DATE"
SET @@session.SQL_mode = "only_full_group_by"
SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'
```

The resulting `@@sql_mode` values are canonical uppercase comma-separated mode
names. `@@sql_mode`, `@@SESSION.sql_mode`, and `@@LOCAL.sql_mode` read the
session value. `@@GLOBAL.sql_mode` reads the global default and is unchanged by
session assignments. `SET ... = DEFAULT` restores the session value to the
current global default.

MySQL canonicalizes mode order independently of input order and removes
duplicates. Empty comma-separated elements are ignored.
Whitespace inside a mode token is not trimmed and makes that token invalid.

Combination modes expand while retaining the combination name:

```text
ANSI -> REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI
TRADITIONAL -> STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
```

When `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES` is set without the complete
`NO_ZERO_IN_DATE`, `NO_ZERO_DATE`, and `ERROR_FOR_DIVISION_BY_ZERO` group, or
when that group appears without strict mode, MySQL emits warning `3135`:

```text
'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes should be used with strict mode. They will be merged with strict mode in a future release.
```

When `PAD_CHAR_TO_FULL_LENGTH` is set, MySQL emits warning `3090`:

```text
Changing sql mode 'PAD_CHAR_TO_FULL_LENGTH' is deprecated. It will be removed in a future release.
```

Invalid mode names fail with error `1231`, SQLSTATE `42000`, and a message
containing:

```text
Variable 'sql_mode' can't be set to the value of '<mode>'
```

Successful `SET sql_mode` statements report zero affected rows. If a later
statement such as `SHOW WARNINGS` runs before `ROW_COUNT()`, `ROW_COUNT()` then
reports that later statement's row count instead.

## Scope

Supported assignment forms:

```sql
SET sql_mode = DEFAULT
SET sql_mode = 'mode_list'
SET sql_mode = "mode_list"
SET SESSION sql_mode = DEFAULT
SET SESSION sql_mode = 'mode_list'
SET LOCAL sql_mode = 'mode_list'
SET @@sql_mode = 'mode_list'
SET @@SESSION.sql_mode = 'mode_list'
SET @@LOCAL.sql_mode = 'mode_list'
```

Variable and scope names are ASCII case-insensitive. Backtick-quoted final
variable-name components keep the existing supported behavior. Backtick-quoted
scope names remain rejected.

`mode_list` is a comma-separated list of supported MySQL 8.4 mode names, or an
empty string. Mode names are ASCII case-insensitive. Empty comma-separated
elements are ignored. Duplicate modes are collapsed.

Supported mode names for state:

- `ALLOW_INVALID_DATES`
- `ANSI`
- `ANSI_QUOTES`
- `ERROR_FOR_DIVISION_BY_ZERO`
- `HIGH_NOT_PRECEDENCE`
- `IGNORE_SPACE`
- `NO_AUTO_VALUE_ON_ZERO`
- `NO_BACKSLASH_ESCAPES`
- `NO_DIR_IN_CREATE`
- `NO_ENGINE_SUBSTITUTION`
- `NO_UNSIGNED_SUBTRACTION`
- `NO_ZERO_DATE`
- `NO_ZERO_IN_DATE`
- `ONLY_FULL_GROUP_BY`
- `PAD_CHAR_TO_FULL_LENGTH`
- `PIPES_AS_CONCAT`
- `REAL_AS_FLOAT`
- `STRICT_ALL_TABLES`
- `STRICT_TRANS_TABLES`
- `TIME_TRUNCATE_FRACTIONAL`
- `TRADITIONAL`

Supported effects in this slice:

- `ANSI_QUOTES`: the next statement's lexer treats double-quoted text as quoted
  identifiers instead of string literals.
- `NO_BACKSLASH_ESCAPES`: the next statement's lexer does not use backslash to
  continue or escape string literals, and runtime string decoding preserves
  backslash bytes literally.
- `NO_AUTO_VALUE_ON_ZERO`: explicit `0` inserted into an `AUTO_INCREMENT`
  column is stored as `0` rather than being replaced by a generated sequence
  value. This slice verifies the storage effect and does not claim exact
  InnoDB-style counter reservation gaps after every prior insert pattern.
- `REAL_AS_FLOAT`: a `REAL` column definition maps to MyLite's `FLOAT`
  descriptor instead of `DOUBLE`.

The global SQL-mode value remains fixed to MySQL 8.4.9's default string. MyLite
does not implement mutable global assignment or persisted variable state in
this slice.

## Non-Goals

This feature must not implement:

- `SET GLOBAL sql_mode`, `PERSIST`, `PERSIST_ONLY`, startup options, or
  persisted variables;
- assignment lists, user variables, stored-program variables, `:=`, expression
  assignments, `CONCAT(@@sql_mode, ...)`, parameters, or subqueries in `SET`;
- full effects for strict and non-strict conversion, zero-date handling,
  `ERROR_FOR_DIVISION_BY_ZERO`, `ONLY_FULL_GROUP_BY`, `PIPES_AS_CONCAT`,
  `IGNORE_SPACE`, `HIGH_NOT_PRECEDENCE`, `NO_UNSIGNED_SUBTRACTION`,
  `PAD_CHAR_TO_FULL_LENGTH`, `TIME_TRUNCATE_FRACTIONAL`,
  `ALLOW_INVALID_DATES`, `NO_DIR_IN_CREATE`, or `NO_ENGINE_SUBSTITUTION`;
- protocol session-state tracking for changed system variables;
- global/session privilege semantics;
- catalog storage of session modes, because modes are connection-local.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns public validation,
  statement context setup, parser configuration, result ownership, diagnostics
  snapshot replacement, and failure cleanup.
- Session state owns the current mode bitset and canonical session mode text.
  It is handle-local, initialized to the fixed MySQL 8.4.9 default, and reset
  on close/reopen because it is not file catalog state.
- Lexer/parser own syntax admission. The parser receives lexer mode flags from
  session state at statement start; it does not query SQLite.
- Runtime execution owns `SET sql_mode` validation, canonicalization, warnings,
  session state mutation, scalar variable reads, `SHOW VARIABLES` values,
  string-literal decoding, `REAL` mapping, and auto-increment zero planning.
- Catalog descriptors remain authoritative for table structure. Session mode
  assignments do not mutate catalog rows, descriptor versions, catalog
  generation, or SQLite schema generation.
- Storage, VFS, and SQLite physical row storage are involved only through
  existing descriptor-driven DDL/DML. This feature must not touch the MyLite
  preamble or add SQLite fork patches.

## Supported Grammar

The parser already owns the broad `SET` target and literal grammar. The
admitted subset for this feature is:

```lemon
set_statement ::= SET set_system_variable_target EQ set_sql_mode_value.

set_system_variable_target ::= identifier.
set_system_variable_target ::= identifier identifier.
set_system_variable_target ::= SYSTEM_VARIABLE.

set_sql_mode_value ::= DEFAULT.
set_sql_mode_value ::= STRING_LITERAL.
```

Runtime then requires the resolved target to be `sql_mode` with no scope,
`SESSION`, `LOCAL`, `@@`, `@@SESSION.`, or `@@LOCAL.`. `GLOBAL` targets are
rejected by the existing deterministic MyLite unsupported diagnostic.

## Canonicalization

MyLite stores modes as a bitset and rebuilds the visible text in MySQL's
observed canonical order:

```text
REAL_AS_FLOAT
PIPES_AS_CONCAT
ANSI_QUOTES
IGNORE_SPACE
ONLY_FULL_GROUP_BY
NO_UNSIGNED_SUBTRACTION
NO_DIR_IN_CREATE
ANSI
NO_AUTO_VALUE_ON_ZERO
NO_BACKSLASH_ESCAPES
STRICT_TRANS_TABLES
STRICT_ALL_TABLES
NO_ZERO_IN_DATE
NO_ZERO_DATE
ALLOW_INVALID_DATES
ERROR_FOR_DIVISION_BY_ZERO
TRADITIONAL
HIGH_NOT_PRECEDENCE
NO_ENGINE_SUBSTITUTION
PAD_CHAR_TO_FULL_LENGTH
TIME_TRUNCATE_FRACTIONAL
```

`ANSI` sets its component bits plus the `ANSI` bit. `TRADITIONAL` sets its
component bits plus the `TRADITIONAL` bit.

## Diagnostics

Supported successful assignments may emit MySQL-compatible warnings:

- warning `3135`, SQLSTATE `HY000`, for incomplete strict/zero-date/division
  mode combinations as observed above;
- warning `3090`, SQLSTATE `HY000`, for `PAD_CHAR_TO_FULL_LENGTH`.

Unsupported or invalid input diagnostics:

- invalid SQL-mode token: error `1231`, SQLSTATE `42000`;
- non-string and non-`DEFAULT` assignment value: deterministic MyLite
  unsupported diagnostic until expression assignment is designed;
- `SET GLOBAL`: existing deterministic MyLite unsupported diagnostic;
- quoted system-variable scope: existing deterministic MyLite unsupported
  diagnostic;
- malformed `SET` syntax and unsupported assignment lists: existing parser
  diagnostics;
- allocation failure: existing MyLite `MYLITE_NOMEM` diagnostic.

## Tests

Add or update fast C tests under `packages/libmylite/tests/` and MySQL 8.4.9
expectation scripts.

Coverage must include:

- all user-listed `SET sql_mode` forms;
- default restoration, empty string, mixed case names, double-quoted values,
  `SESSION`, `LOCAL`, `@@`, `@@SESSION`, and `@@LOCAL` scopes;
- session reads versus fixed global reads through scalar `SELECT` and
  `SHOW [SESSION|GLOBAL] VARIABLES LIKE 'sql_mode'`;
- canonical ordering, duplicate mode removal, combination mode expansion, and
  empty comma-element handling;
- invalid mode diagnostics and warnings `3135` and `3090`;
- independent handles and close/reopen reset of session-local state;
- no catalog generation, SQLite schema generation, descriptor, or preamble
  mutation from `SET sql_mode`;
- `ANSI_QUOTES` parser effect for following statements;
- `NO_BACKSLASH_ESCAPES` lexer and string-decoding effect for following
  statements;
- `NO_AUTO_VALUE_ON_ZERO` auto-increment zero handling;
- `REAL_AS_FLOAT` `REAL` type mapping;
- existing lexer, parser, runtime SQL-mode scalar, fixed `SET` variables,
  auto-increment, approximate type, string type, and full workflow tests.
