# Baseline BLOB Expression Defaults

## Status

This feature specifies a narrow generated-default slice for MyLite's existing
descriptor-owned BLOB-family columns. It admits parenthesized hexadecimal
literal defaults and parenthesized `NULL` defaults for `TINYBLOB`, `BLOB`,
`MEDIUMBLOB`, `LONGBLOB`, and the normalized `LONG VARBINARY` alias in full
column-definition DDL.

This is not a general binary expression-default feature. It does not admit
bare BLOB defaults, ordinary string expression defaults, arithmetic or function
expression defaults, JSON expression defaults, or `ALTER COLUMN SET DEFAULT`
for BLOB-family columns.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline binary string types:
  `docs/specs/baseline-binary-string-types/specs.md`
- Baseline binary string defaults:
  `docs/specs/baseline-binary-string-defaults/specs.md`
- Baseline parenthesized string defaults:
  `docs/specs/baseline-parenthesized-string-defaults/specs.md`
- Baseline DML default keyword values:
  `docs/specs/baseline-dml-default-keyword-values/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, BLOB and TEXT types:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL 8.4 Reference Manual, hexadecimal literals:
  https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html

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

- `BLOB DEFAULT X'41'` and `BLOB DEFAULT 'abc'` fail with
  `1101 / 42000` because bare defaults are not allowed for BLOB-family
  columns.
- `BLOB DEFAULT (X'4100FF')`, `TINYBLOB DEFAULT (0x4200)`, and
  `BLOB DEFAULT (X'')` are accepted in `CREATE TABLE`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report hexadecimal generated
  defaults as display text such as `0x4100ff` or `X''` and report
  `DEFAULT_GENERATED` in `Extra`.
- `SHOW CREATE TABLE` renders accepted hexadecimal generated defaults inside
  parentheses, for example `DEFAULT (0x4100ff)` or `DEFAULT (X'')`.
- Omitted-column `INSERT` and explicit DML `DEFAULT` materialize the generated
  BLOB default bytes.
- `DEFAULT (NULL)` is accepted as a generated default, including for `NOT NULL`
  BLOB-family columns. Later DML that materializes that generated null into a
  `NOT NULL` column fails with `1048 / 23000`.
- `ALTER TABLE ... ADD COLUMN`, `ALTER TABLE ... MODIFY COLUMN`, and
  `ALTER TABLE ... CHANGE COLUMN` accept BLOB-family generated defaults through
  full column definitions. Existing rows are backfilled for `ADD COLUMN` when
  the generated default materializes a non-`NULL` value.
- `ALTER TABLE ... ALTER COLUMN blob_col SET DEFAULT (X'41')` fails with
  `1101 / 42000`.
- MySQL also accepts broader BLOB expression defaults, including ordinary
  string literals and arithmetic expressions. MyLite defers those because they
  require preserving expression-origin metadata and broader expression
  evaluation/coercion timing than this default-focused slice should introduce.
- MySQL permits generated defaults that are longer than a `TINYBLOB`
  descriptor at DDL time, but strict omitted-column DML later fails with
  `1406 / 22001`. MyLite's current catalog default payload envelope is smaller
  than larger BLOB-family types, so this slice rejects generated defaults that
  cannot fit MyLite's descriptor payload instead of accepting values that cannot
  be durably represented.

The implementation must add
`packages/libmylite/tests/mysql_baseline_blob_expression_defaults_expectations.sh`
to record these expectations and guard against accidental drift.

## Scope

The implementation must add:

- parenthesized hexadecimal literal defaults for `TINYBLOB`, `BLOB`,
  `MEDIUMBLOB`, `LONGBLOB`, and normalized `LONG VARBINARY` descriptors;
- parenthesized `NULL` generated defaults for the same descriptors;
- coverage through existing full column-definition planning paths:
  `CREATE TABLE`, `CREATE TEMPORARY TABLE` where the descriptor is otherwise
  supported, `ALTER TABLE ... ADD COLUMN`, `ALTER TABLE ... MODIFY COLUMN`, and
  `ALTER TABLE ... CHANGE COLUMN`;
- descriptor-owned storage of converted generated default bytes without
  embedding NUL bytes into catalog text fields;
- descriptor materialization through omitted-column `INSERT`, explicit DML
  `DEFAULT`, `REPLACE`, and supported `UPDATE column = DEFAULT` paths;
- descriptor cloning/copying through `CREATE TABLE ... LIKE` and compatible
  descriptor-copy paths;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering with `DEFAULT_GENERATED`;
- persistence through reopen, table rename/drop, independent file-backed
  handles, and `.mylite` preamble preservation.

The implementation must not add:

- bare BLOB-family non-`NULL` defaults;
- ordinary string generated defaults for BLOB-family columns;
- generated defaults for `BINARY` and `VARBINARY`;
- arithmetic, function, column-reference, subquery, parameter, variable, JSON,
  geometry, or general expression defaults;
- `ALTER TABLE ... ALTER COLUMN blob_col SET DEFAULT (X'...')`;
- warning-producing default truncation, runtime expression reevaluation,
  charset introducer parsing, charset conversion, collation evaluation, or
  broader SQL mode behavior;
- SQLite fork patches.

## Syntax

The existing parser already represents `DEFAULT (expression)` as a
parenthesized expression node. The independently authored MyLite grammar shape
for this feature is:

```lemon
column_default ::= DEFAULT LPAREN blob_generated_default RPAREN.
blob_generated_default ::= hex_literal.
blob_generated_default ::= NULL.
```

The rule is semantic, not a separate parser production. It applies only when
the target descriptor is a BLOB-family descriptor and only in statements that
carry a full column definition.

## Semantics

For `DEFAULT (hex_literal)`, MyLite decodes the hexadecimal literal through the
same binary literal decoder used by descriptor-backed row DML. `X'...'` forms
must have an even digit count as the lexer already enforces. `0x...` forms use
MySQL-compatible odd-digit normalization by padding the first byte on the left,
so `0x1` stores one byte `0x01` and `0xabc` stores bytes `0x0a 0xbc`.

The decoded byte sequence must fit both the target descriptor and MyLite's
current catalog default payload envelope. Catalog default bytes are stored as
uppercase hexadecimal text in `default_text`, so this slice admits at most 511
converted bytes. That is enough for the full `TINYBLOB` range and a limited
prefix of wider BLOB-family descriptors. Values that exceed the target
descriptor or the payload envelope fail with MyLite's deterministic invalid
default diagnostic. MyLite does not accept MySQL's DDL-time overlength default
shape until catalog storage and DML-time strict/non-strict adjustment can
represent it correctly.

For `DEFAULT (NULL)`, MyLite stores a generated null default. DDL succeeds even
when the column is `NOT NULL`. DML default materialization follows the existing
null-assignment path, so materializing that generated null into a `NOT NULL`
column fails unless an already supported `IGNORE` or non-strict adjustment path
explicitly changes the value.

Generated BLOB-family defaults reuse MyLite's existing binary-default catalog
payload kind. The logical descriptor type distinguishes generated BLOB-family
defaults from bare `BINARY` and `VARBINARY` literal defaults for metadata and
`SHOW CREATE TABLE` rendering. This avoids a catalog schema bump while keeping
the catalog descriptor authoritative and byte-safe.

`SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` decode the
catalog payload as generated-default expression text and return:

- `0x` followed by uppercase hex digits for nonempty generated BLOB defaults,
  including bytes after embedded `0x00`;
- `X''` for zero-length generated BLOB defaults.

`SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.EXTRA` include
`DEFAULT_GENERATED` for admitted generated BLOB-family defaults.

`SHOW CREATE TABLE` renders admitted generated BLOB defaults as
`DEFAULT (0x...)`, with uppercase hex digits from the internal payload. The
zero-length generated default renders as `DEFAULT (X'')`. This is a stable,
semantically equivalent MyLite rendering for the admitted hex-literal subset;
it intentionally does not preserve whether the input spelling used `X'...'` or
`0x...`.

## Ownership Boundaries

- Public API: no ABI or API changes. Successful statements return through the
  existing non-row result conventions; errors use the existing diagnostics
  surface.
- Statement context: unchanged except that supported in-range generated
  defaults report `warning_count == 0`.
- Parser/AST: owns syntax admission only. It records the parenthesized
  expression and literal spans but does not decode BLOB bytes.
- Analyzer/planner: validates that the target descriptor is BLOB-family and the
  inner expression is exactly `HEX_LITERAL` or `NULL`; decodes hex bytes;
  checks descriptor and catalog-payload bounds; and constructs descriptor
  default fields.
- Catalog: owns logical type, physical type, nullability, default kind, and the
  internal uppercase-hex default payload. Catalog descriptors remain
  authoritative and separate from SQLite schema text.
- Result builder: renders descriptor metadata for `SHOW` and
  `INFORMATION_SCHEMA`, including generated-extra reporting for this
  type/default combination.
- Runtime execution: materializes DML defaults as BLOB or `NULL` values through
  the existing prepared-statement binding paths and emits generated SQLite DDL
  only from descriptors.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are preserved.
- SQLite physical storage: generated MyLite user tables remain ordinary SQLite
  rowid tables storing BLOB values. This slice uses MyLite wrapper/translation
  logic and public SQLite APIs, not a SQLite fork hook.

## Diagnostics

Supported in-range definitions and DML default materialization produce no
warnings. Unsupported or invalid behavior returns existing deterministic
diagnostics:

- bare BLOB-family non-`NULL` defaults: MySQL-compatible
  `1101 / 42000` BLOB/TEXT default rejection where the current path provides
  it, otherwise the existing invalid-default diagnostic for the same surface;
- parenthesized BLOB-family defaults whose inner expression is not `HEX_LITERAL`
  or `NULL`: invalid-default diagnostic;
- generated defaults that exceed the descriptor or catalog-payload envelope:
  invalid-default diagnostic;
- materializing `DEFAULT (NULL)` into a `NOT NULL` target: existing null-value
  DML diagnostic, normally `1048 / 23000`;
- unsupported `ALTER TABLE ... ALTER COLUMN ... SET DEFAULT (X'...')`: existing
  invalid-default diagnostic;
- allocation failures and physical SQLite failures: existing runtime
  diagnostics.

## Performance And SQLite Fit

No query path is reimplemented. This is descriptor and DDL planning work:
generated default metadata is validated once, persisted in MyLite's catalog,
and materialized only when DML requests a default value. `ALTER TABLE ... ADD
COLUMN ... DEFAULT (...)` may require SQLite to backfill existing rows through
the existing generated physical DDL path; ordinary queries and DML continue to
use SQLite prepared statements with bound values. No SQLite fork patch is
required.

## Tests

Add fast C tests under `packages/libmylite/tests/` and MySQL-runtime
expectations to cover:

- `CREATE TABLE` generated BLOB-family defaults from `X'...'`, `0x...`,
  odd-digit `0x...`, empty hex, and `NULL`;
- `SHOW COLUMNS`, `DESCRIBE`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS` default and `DEFAULT_GENERATED` metadata;
- omitted-column `INSERT`, explicit `DEFAULT` in `INSERT`/`REPLACE`, and
  supported `UPDATE column = DEFAULT` materialization;
- `DEFAULT (NULL)` on nullable and `NOT NULL` BLOB-family columns, including
  DML failure when the generated null is materialized into a `NOT NULL` column;
- `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN`
  generated-default preservation and add-column backfill;
- `CREATE TABLE ... LIKE`, table rename/drop, reopen persistence, independent
  file-backed handles, and `.mylite` preamble preservation;
- rejection of bare BLOB-family defaults, ordinary string generated defaults,
  arithmetic generated defaults, `BINARY`/`VARBINARY` generated defaults,
  `ALTER COLUMN SET DEFAULT`, and generated defaults outside the supported
  descriptor/payload envelope;
- unchanged binary string defaults, parenthesized string defaults, row DML
  default keyword, metadata, catalog, file-format, and runtime lifecycle tests.
