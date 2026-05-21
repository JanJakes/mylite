# Baseline Character Expression Defaults

## Status

This feature specifies a narrow generated-default slice for MyLite's existing
descriptor-owned `CHAR` and `VARCHAR` columns. It admits parenthesized constant
string defaults such as `VARCHAR(10) DEFAULT ('abc')` in full column-definition
DDL and materializes them through the same descriptor conversion used by
ordinary row DML.

This is not a general expression-default feature. It does not evaluate
functions, arithmetic, column references, variables, parameters, subqueries,
hexadecimal values, binary-string generated defaults, `TEXT`/BLOB generated
defaults beyond the existing slices, `ENUM`/`SET` expression defaults, or
charset introducers.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline string defaults:
  `docs/specs/baseline-string-defaults/specs.md`
- Baseline parenthesized string defaults:
  `docs/specs/baseline-parenthesized-string-defaults/specs.md`
- Baseline binary string defaults:
  `docs/specs/baseline-binary-string-defaults/specs.md`
- Baseline BLOB expression defaults:
  `docs/specs/baseline-blob-expression-defaults/specs.md`
- Baseline DML default keyword values:
  `docs/specs/baseline-dml-default-keyword-values/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes were run against MySQL 8.4.9 with:

```sh
MYLITE_MYSQL_BIN=/opt/homebrew/opt/mysql@8.4/bin/mysql
MYLITE_MYSQL_SOCKET=/tmp/mylite-mysql-849.jsgoZE/mysql.sock
```

Observed behavior that shapes this slice:

- `CHAR(n) DEFAULT ('abc')`, `VARCHAR(n) DEFAULT ('abc')`, and
  `DEFAULT ('')` are accepted in `CREATE TABLE`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report decoded string
  generated defaults with a `_utf8mb4'...'` expression and
  `DEFAULT_GENERATED` in `Extra`.
- `SHOW CREATE TABLE` renders admitted values as
  `DEFAULT (_utf8mb4'...')`.
- Omitted-column `INSERT`, explicit DML `DEFAULT`, `REPLACE`, and
  `UPDATE column = DEFAULT` materialize the generated value.
- `DEFAULT (NULL)` is accepted as a generated default, including for
  `NOT NULL` `CHAR`/`VARCHAR` columns. Later DML that materializes that
  generated null into a `NOT NULL` column fails with `1048 / 23000`.
- `ALTER TABLE ... ADD COLUMN`, `ALTER TABLE ... MODIFY COLUMN`, and
  `ALTER TABLE ... CHANGE COLUMN` accept generated string defaults through full
  column definitions.
- `ALTER TABLE ... ALTER COLUMN char_col SET DEFAULT ('x')` accepts the syntax
  but stores an ordinary literal default, not a generated default: metadata has
  no `DEFAULT_GENERATED` extra and `SHOW CREATE TABLE` renders
  `DEFAULT 'x'`. This slice keeps the existing ordinary default behavior for
  `ALTER COLUMN SET DEFAULT`.
- MySQL accepts generated defaults longer than the declared `CHAR`/`VARCHAR`
  length at DDL time, but strict omitted-column DML later fails with
  `1406 / 22001`. MyLite may store generated text only while it fits the
  current catalog default payload. Within that envelope, materialization must
  use descriptor conversion so overlength values fail or adjust at DML time
  according to existing strict/non-strict row-value policy.
- MySQL also accepts broader generated defaults such as `DEFAULT (1 + 2)`,
  `DEFAULT (0x41)`, and deterministic function expressions on `VARCHAR`
  columns. MyLite defers those until general expression defaults are designed.

The implementation must add
`packages/libmylite/tests/mysql_baseline_character_expression_defaults_expectations.sh`
to record these expectations and guard against drift.

## Scope

The implementation must add:

- parenthesized constant string literal defaults for descriptor-backed `CHAR`
  and `VARCHAR` columns, including `CHAR(0)` and `VARCHAR(0)` empty-string
  defaults;
- generated `NULL` defaults for the same descriptor family where not already
  covered by the existing expression-default path;
- coverage through full column-definition planning paths: `CREATE TABLE`,
  `CREATE TEMPORARY TABLE` where the descriptor is otherwise supported,
  `ALTER TABLE ... ADD COLUMN`, `ALTER TABLE ... MODIFY COLUMN`, and
  `ALTER TABLE ... CHANGE COLUMN`;
- descriptor-owned storage that distinguishes ordinary text defaults from
  generated text defaults for metadata and `SHOW CREATE TABLE`;
- DML materialization through omitted-column `INSERT`, explicit `DEFAULT` in
  admitted `INSERT`/`REPLACE`, supported `UPDATE column = DEFAULT`, and
  compatible `DEFAULT(column_name)` forms already admitted by the baseline;
- descriptor cloning/copying through `CREATE TABLE ... LIKE` and compatible
  descriptor-copy paths;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and limited
  `INFORMATION_SCHEMA.COLUMNS` rendering with `DEFAULT_GENERATED`;
- persistence through reopen, table rename/drop, independent file-backed
  handles, and `.mylite` preamble preservation.

The implementation must not add:

- general expression evaluation for `CHAR`/`VARCHAR` defaults;
- numeric, hexadecimal, bit, function, arithmetic, column-reference, subquery,
  parameter, variable, JSON, geometry, binary string, `ENUM`, or `SET`
  generated defaults;
- binary-string generated defaults for `BINARY`/`VARBINARY`;
- charset introducers, national string literals, adjacent literal
  concatenation, charset conversion, collation evaluation, or broader SQL mode
  behavior;
- changes to public API or SQLite fork patches.

## Syntax

The existing parser already represents `DEFAULT (expression)` as a
parenthesized expression node. The independently authored MyLite grammar shape
for this feature is:

```lemon
column_default_value(A) ::= LPAREN(L) expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, E, R);
}
```

This feature narrows semantic acceptance after parsing:

```text
character_generated_default:
    DEFAULT ( string_literal )
  | DEFAULT ( NULL )
```

The string form applies only to `CHAR` and `VARCHAR` descriptor targets in full
column-definition DDL. The `NULL` form uses the existing generated-null default
representation.

## Semantics

For `DEFAULT ('text')`, MyLite decodes the SQL string literal with the existing
string-literal decoder, rejects embedded `NUL`, requires valid UTF-8, and
stores the decoded text as descriptor-owned generated-default text. The stored
text is the user expression value before `CHAR` trimming or declared-length
conversion. This preserves MySQL's DDL-time acceptance of overlength generated
defaults while allowing DML materialization to run descriptor conversion at the
time a row actually requests the default.

When generated text is materialized:

- `CHAR` defaults use the current `CHAR` row-value conversion, including
  trailing-space canonicalization and strict/non-strict truncation behavior;
- `VARCHAR` defaults use the current `VARCHAR` row-value conversion, including
  strict/non-strict truncation behavior;
- DML that materializes an overlength generated default in strict mode fails
  with the existing data-too-long diagnostic;
- non-strict or `IGNORE` paths may use the existing string-truncation warning
  adjustment if the current DML path already applies that policy.

For `DEFAULT (NULL)`, MyLite stores a generated null default. DML
materialization follows the existing null-assignment path. Materializing the
default into a `NOT NULL` column fails with `Column 'name' cannot be null`
unless an already-supported `IGNORE` or non-strict adjustment path changes the
value.

Generated text defaults must fit `MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY`. Values
larger than that durable descriptor envelope are rejected with MyLite's
deterministic invalid-default diagnostic until the catalog gains larger default
payload storage.

`ALTER TABLE ... ALTER COLUMN char_col SET DEFAULT ('x')` remains an ordinary
literal-default operation for this slice, matching observed MySQL behavior.

## Ownership Boundaries

- Public API: no ABI or API changes. Successful statements return through the
  existing non-row result conventions; errors use existing diagnostics.
- Statement context: unchanged except that supported in-range generated
  defaults record `warning_count == 0`.
- Parser/AST: owns syntax admission only. It records the parenthesized
  expression and literal spans but does not decide default compatibility.
- Analyzer/planner: validates that the target descriptor is `CHAR`/`VARCHAR`
  and the inner expression is exactly `STRING` or `NULL`; decodes and validates
  generated string text; checks catalog payload bounds; and constructs
  descriptor default fields.
- Catalog: owns logical type, physical type, nullability, default kind, and
  decoded generated-default text. The catalog schema advances because older
  readers cannot distinguish generated character defaults from ordinary text
  defaults.
- Result builder: renders descriptor metadata for `SHOW` and
  `INFORMATION_SCHEMA`, including generated-extra reporting.
- Runtime execution: materializes generated defaults through existing
  descriptor conversion and prepared-statement binding paths.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are preserved.
- SQLite physical storage: generated MyLite user tables remain ordinary SQLite
  rowid tables storing text values. This slice uses MyLite wrapper/translation
  logic and public SQLite APIs, not a SQLite fork hook.

## Metadata Rendering

`SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and
`INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` render generated character defaults
as `_utf8mb4'decoded'` with MySQL-style escaping for quotes, backslashes, and
common control characters. `EXTRA` includes `DEFAULT_GENERATED`.

`SHOW CREATE TABLE` renders:

```sql
`v` varchar(10) DEFAULT (_utf8mb4'decoded')
```

For generated null defaults, metadata renders `NULL` and `DEFAULT_GENERATED`,
and `SHOW CREATE TABLE` renders `DEFAULT (NULL)`.

## Diagnostics

Supported definitions and in-range DML default materialization produce no
warnings. Unsupported or invalid behavior returns existing deterministic
diagnostics:

- parenthesized `CHAR`/`VARCHAR` defaults whose inner expression is not
  `STRING` or `NULL`: invalid-default diagnostic;
- generated string defaults containing embedded `NUL`, invalid UTF-8, or
  exceeding MyLite's default payload envelope: invalid-default diagnostic;
- strict DML materialization of an overlength generated default:
  data-too-long diagnostic;
- materializing `DEFAULT (NULL)` into a `NOT NULL` target: existing bad-null
  DML diagnostic, normally `1048 / 23000`;
- generated defaults for binary strings, `TEXT`/BLOB outside existing slices,
  `BIT`, `YEAR`, temporal, `JSON`, spatial, `ENUM`, `SET`, or arbitrary
  expression targets: existing invalid-default or unsupported diagnostics;
- allocation failures and physical SQLite failures: existing runtime
  diagnostics.

## Performance And SQLite Fit

No query path is reimplemented. This is descriptor and DDL planning work:
generated-default metadata is validated once, persisted in MyLite's catalog,
and materialized only when DML requests a default value. Ordinary DML still
binds converted values into SQLite prepared statements. `ALTER TABLE ... ADD
COLUMN ... DEFAULT ('x')` may backfill existing rows through the existing
generated physical DDL path when the default is materialized. No SQLite fork
patch is required.

## Tests

Add fast C tests under `packages/libmylite/tests/` and MySQL-runtime
expectations to cover:

- `CREATE TABLE` generated `CHAR` and `VARCHAR` defaults from nonempty and
  empty string literals;
- `CHAR(0)` and `VARCHAR(0)` empty generated defaults;
- generated `NULL` defaults on nullable and `NOT NULL` columns, including DML
  materialization diagnostics for `NOT NULL`;
- `SHOW COLUMNS`, `DESCRIBE`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS` default and `DEFAULT_GENERATED` metadata;
- omitted-column `INSERT`, explicit `DEFAULT`, `REPLACE`, supported
  `UPDATE column = DEFAULT`, and compatible `DEFAULT(column_name)` forms;
- overlength generated defaults accepted within the catalog payload envelope
  and rejected when materialized in strict mode;
- `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN` generated
  defaults through full column definitions;
- `ALTER TABLE ... ALTER COLUMN ... SET DEFAULT ('x')` still producing an
  ordinary literal default with no `DEFAULT_GENERATED`;
- rejected generated defaults for numeric, function, arithmetic, hexadecimal,
  parameter, and binary-string expression forms;
- persistence after close/reopen, `CREATE TABLE ... LIKE`, table rename/drop,
  independent file-backed handles, catalog migration, and `.mylite` preamble
  preservation;
- existing string-default, parenthesized string-default, binary-default,
  DML-default, modify/change column, SHOW, information-schema, catalog, storage,
  and lifecycle tests.
