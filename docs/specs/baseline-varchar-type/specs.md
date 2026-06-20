# Baseline VARCHAR Type

## Status

This feature specifies the first string-storage type slice for file-backed
`.mylite` handles. It adds descriptor-owned `VARCHAR(n)` columns and ordinary
string literal row values on top of the existing persistent base-table,
integer/`NULL` row-value, `SELECT`, `INSERT`, `REPLACE`, and single-table
`UPDATE` paths.

The feature is intentionally not full MySQL string support. It stores and
returns bounded non-`NUL` UTF-8 text, but it does not implement string defaults,
character-set conversion, collation comparison, string expression evaluation,
string ordering, `LIKE` predicates over table data, or protocol-grade string
metadata.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline table charset and collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CHAR` and `VARCHAR`:
  https://dev.mysql.com/doc/refman/8.4/en/char.html
- MySQL 8.4 Reference Manual, string literals:
  https://dev.mysql.com/doc/refman/8.4/en/string-literals.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_varchar_type_expectations.sh` records
the runtime probes for this feature. Observed behavior that shapes this slice:

- MySQL 8.4.9 under the default SQL mode accepts `VARCHAR(0)` and
  `VARCHAR(16383)` for a single `utf8mb4` column, rejects `VARCHAR` without a
  length with syntax error `1064`, rejects negative lengths with syntax error
  `1064`, and rejects `VARCHAR(16384)` with error `1074`.
- `SHOW COLUMNS` renders `varchar(n)`, and `SHOW CREATE TABLE` renders nullable
  columns with `DEFAULT NULL` plus the current fixed
  `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci` suffix.
- `VARCHAR` stores and retrieves trailing spaces without padding when the value
  length fits the declared length.
- Assigning a nonspace-overlength value in strict mode fails with error `1406`,
  SQLSTATE `22001`, and `Data too long for column ...`.
- Assigning only excess trailing spaces succeeds in MySQL with note `1265` and
  truncates to fit. This original `VARCHAR` slice admitted only values already
  within the declared character length; the later baseline non-strict string
  truncation slice adds limited DML trailing-space notes.
- `INSERT IGNORE` converts string length and `NULL`/no-default failures to
  warnings in MySQL. This original `VARCHAR` slice supports the no-default and
  `NULL` adjustments for the admitted non-overlength string subset; the later
  baseline non-strict string truncation slice adds limited DML overlength string
  warning demotion.
- MySQL accepts explicit string defaults such as `DEFAULT 'xy'`. MyLite defers
  string defaults to avoid expanding the durable catalog default schema before
  the first string-storage slice.
- MySQL compares and sorts nonbinary strings according to the column collation.
  MyLite's current `utf8mb4_0900_ai_ci` surface is metadata-only, so this slice
  does not admit collation-sensitive string comparisons, grouping, distinct, or
  ordering.

## Scope

The implementation must add:

- parser and AST support for `VARCHAR(length)` column types;
- descriptor-owned logical type text `VARCHAR(n)` for admitted lengths
  `0..255`;
- physical SQLite type text `TEXT` for admitted `VARCHAR` descriptors;
- `CREATE TABLE` support for persistent base tables containing `VARCHAR`
  columns;
- `ALTER TABLE ... ADD [COLUMN]` support for `VARCHAR` columns, including
  physical empty-string backfill for `NOT NULL` no-explicit-default additions;
- `CREATE TABLE ... LIKE` descriptor cloning for tables containing admitted
  `VARCHAR` columns;
- descriptor-backed `CREATE TABLE ... SELECT` and
  `INSERT ... SELECT` copying when source and target values are already
  compatible with admitted `VARCHAR` target descriptors;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  rendering for `VARCHAR(n)` descriptors without string defaults;
- ordinary string literal values for `INSERT ... VALUES`, `INSERT ... SET`,
  `REPLACE ... VALUES`, `REPLACE ... SET`, and single-table `UPDATE`
  assignments into `VARCHAR` columns;
- `NULL` assignment and effective nullable `DEFAULT NULL` materialization for
  `VARCHAR` columns;
- limited `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` adjustment for
  `NULL` into `VARCHAR NOT NULL` and omitted or explicit `DEFAULT` for no-
  explicit-default `VARCHAR NOT NULL`, storing the MySQL implicit empty string
  and recording warnings;
- descriptor-backed `SELECT` readback of `TEXT` values as public result text;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  `VARCHAR` columns;
- deterministic rejection of collation-sensitive `VARCHAR` comparisons,
  `BETWEEN`, `IN`, truth predicates, ordering, `DISTINCT`, grouped columns,
  and numeric aggregates; `COUNT(DISTINCT column)` is covered by
  `docs/specs/baseline-string-count-distinct-aggregates/specs.md`;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted `VARCHAR` data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `CHAR`, `CHARACTER`, `CHARACTER VARYING`, `NCHAR`, `NVARCHAR`, `TEXT`,
  `BLOB`, `BINARY`, `VARBINARY`, `ENUM`, `SET`, `JSON`, or other string-family
  types;
- `VARCHAR` lengths above `255`, row-size accounting, compact length-prefix
  storage, or protocol length metadata;
- column-level `CHARACTER SET`, `COLLATE`, `BINARY`, or character-set
  introducer syntax;
- explicit string defaults, expression defaults, `DEFAULT(col_name)`, or
  string default catalog storage;
- string-to-integer or integer-to-string DML conversion;
- string comparison predicates, `LIKE` over table data, `REGEXP`, collations,
  coercibility, `ORDER BY` over string columns, grouped string keys, string
  `DISTINCT`, or collation-aware uniqueness;
- adjacent string literal concatenation, national strings, hex/bit string
  values, parameters, user variables, functions, arbitrary expressions, or
  scalar string projection;
- embedded `NUL` result values or binary strings;
- `ALTER TABLE ... MODIFY [COLUMN]` or `CHANGE [COLUMN]` to or from `VARCHAR`;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and failure cleanup. The public
  result API remains NUL-terminated text, so embedded `NUL` string values stay
  unsupported.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported in-range `VARCHAR` operations record
  `warning_count == 0`; supported `INSERT IGNORE` string adjustments record
  warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for `VARCHAR(n)` and DML string
  literals. They store source spans and structural payloads only; they do not
  resolve catalog descriptors or perform storage conversion.
- Analyzer/planner code maps `VARCHAR(n)` AST nodes to durable descriptors,
  resolves schemas/tables/columns against the MyLite catalog, decodes admitted
  string literals, validates UTF-8 and declared character length, rejects
  unsupported conversion or collation-sensitive operations, and produces
  descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, defaults, and column order. This slice reuses the
  existing string `logical_type` / `physical_type` descriptor fields and does
  not change `_mylite_catalog_columns` schema because explicit string defaults
  are deferred.
- Result and introspection builders render logical descriptors to MySQL-shaped
  text. SQLite schema text and `sqlite_schema` are not metadata authority.
- SQLite owns physical row storage, scans, and mutations for generated prepared
  statements. MyLite binds string values with length-aware
  `sqlite3_bind_text(..., SQLITE_TRANSIENT)` and reads SQLite `TEXT` values for
  result rows.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

The feature extends the existing limited column definition grammar:

```sql
column_definition:
    column_name column_type [NULL | NOT NULL] [DEFAULT NULL]
  | column_name column_type

column_type:
    existing_integer_type
  | VARCHAR ( unsigned_decimal_integer_literal )
```

`VARCHAR` length must be a decimal integer literal from `0` through `255`.
Signs, expressions, parameters, omitted lengths, and non-decimal forms are not
admitted.

DML values for `VARCHAR` targets extend the existing row-value grammar:

```sql
insert_value:
    existing_integer_or_boolean_or_NULL_or_DEFAULT_value
  | string_literal

update_value:
    existing_integer_or_boolean_or_NULL_or_DEFAULT_value
  | string_literal
```

`string_literal` is an ordinary MySQL string token under MyLite's fixed default
SQL mode: single-quoted or double-quoted text with doubled quote characters and
backslash escapes decoded by MyLite. The admitted escape decoding follows the
MySQL 8.4.9 runtime observations for ordinary string values: recognized control
and quote escapes are decoded, `\%` and `\_` preserve the backslash outside
pattern matching, and other unrecognized escapes drop the backslash. National
strings, introducers, adjacent literal concatenation, hex literals, bit
literals, parameters, and expressions are not part of this slice.

The existing table-backed `WHERE` grammar is unchanged. `VARCHAR` columns are
valid only for the already supported `IS NULL` and `IS NOT NULL` predicate
forms.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
column_definition ::=
    identifier column_type nullability_opt column_default_opt.

column_type ::= integer_type.
column_type ::= varchar_type.

varchar_type ::= VARCHAR LPAREN INTEGER RPAREN.

insert_value ::= STRING.
update_value ::= STRING.
```

The parser may continue to reject unsupported forms as syntax errors. If a
shape is parsed for an existing wider statement family, the analyzer must
return deterministic unsupported diagnostics before generating SQLite SQL.

## Type and Value Semantics

`VARCHAR(n)` uses logical descriptor text `VARCHAR(n)` and physical descriptor
text `TEXT`. `n` is stored only in logical descriptor text for this slice.
Runtime helpers parse that descriptor text when validating values or rendering
introspection.

Admitted string values must satisfy all of the following before SQLite binding:

- decoded string contains no embedded `NUL` byte;
- decoded string is valid UTF-8;
- decoded Unicode scalar count is less than or equal to the declared
  `VARCHAR(n)` length;
- decoded byte length fits SQLite's `int` binding length limit.

Values are not padded. Trailing spaces are preserved when the admitted value
fits the declared length. Empty strings are distinct from `NULL`, and
`VARCHAR(0)` admits only the empty string or `NULL`.

Overlength nonspace values fail with MySQL error `1406`, SQLSTATE `22001`, and
a message naming the target column and row number where that concept exists.
The later baseline non-strict string truncation slice adds limited DML
trailing-space notes and non-strict warning demotion for `VARCHAR` literals.

`NULL` into a nullable `VARCHAR` stores `NULL`. `NULL` into `VARCHAR NOT NULL`
fails with the existing MySQL-compatible `1048` diagnostic unless the current
statement is an admitted `INSERT IGNORE ... VALUES` or `INSERT IGNORE ... SET`
form; the `IGNORE` form stores the empty string and records warning `1048`.

Omitted nullable `VARCHAR` columns and explicit `DEFAULT` for nullable
no-explicit-default `VARCHAR` columns materialize as `NULL`. Omitted
`VARCHAR NOT NULL` columns and explicit `DEFAULT` for no-explicit-default
`VARCHAR NOT NULL` columns fail with `1364`, except in admitted `INSERT IGNORE`
forms, where MyLite stores the empty string and records warning `1364`.

String defaults such as `DEFAULT 'abc'` remain unsupported in this slice. The
parser may reject them as syntax errors; if a parsed shape reaches the planner,
the planner must reject it before mutating descriptors.

## Runtime Semantics

`CREATE TABLE` creates catalog column descriptors and SQLite physical tables
from MyLite descriptors. Generated SQLite DDL uses stable physical table names
such as `_mylite_user_table_<table_id>`, quoted identifiers, and descriptor
physical type text. The generated shape for `VARCHAR` columns is SQLite `TEXT`,
with `NOT NULL` when the descriptor is not nullable.

`ALTER TABLE ... ADD [COLUMN]` appends admitted `VARCHAR` descriptors and
generates SQLite `ALTER TABLE ... ADD COLUMN` from descriptor physical type
text. For `VARCHAR NOT NULL` without an explicit default, the physical SQLite
statement uses an internal `DEFAULT ''` only to satisfy SQLite's existing-row
backfill requirement. The catalog still records the MySQL no-explicit-default
state, so later omitted values continue to follow MySQL diagnostics.

`ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` to or from `VARCHAR`
remain unsupported because MyLite does not yet implement MySQL string/integer
row conversion or `VARCHAR` length narrowing warnings.

`INSERT`, `REPLACE`, and `UPDATE` bind admitted string values with SQLite text
parameters. Generated SQL never interpolates string literal contents.

`INSERT ... SELECT` and `CREATE TABLE ... SELECT` validate materialized source
rows against target descriptors before inserting. SQLite `TEXT` source values
are accepted only for `VARCHAR` targets after the same UTF-8, NUL, and length
checks used by literal DML. SQLite integer source values remain accepted only
for integer-family targets.

`SELECT` result readback accepts SQLite `NULL`, `INTEGER`, and `TEXT` cells for
descriptor-backed table reads. `TEXT` cells are returned through the existing
public result text convention, which requires no embedded `NUL`.

`WHERE varchar_col IS NULL` and `WHERE varchar_col IS NOT NULL` reuse the
existing descriptor-backed predicate lowering because those predicates do not
depend on collation or string conversion. Other predicate families over
`VARCHAR` columns are rejected deterministically.

Ordering, grouping, `DISTINCT`, and `MIN`/`MAX` over `VARCHAR` columns were
deferred in the original type slice. Later aggregate slices cover
`COUNT(DISTINCT column)` and `MIN`/`MAX` over nonbinary string descriptors with
MyLite's registered string-key collation. Plain `COUNT(varchar_col)` may use
the existing descriptor `COUNT(column)` path because it only distinguishes
`NULL` from non-`NULL` and does not compare string values.

## Introspection

`SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` render admitted `VARCHAR`
descriptors in lower-case MySQL form:

| Logical descriptor | Rendered type |
| --- | --- |
| `VARCHAR(0)` | `varchar(0)` |
| `VARCHAR(255)` | `varchar(255)` |

The same rendering rule applies to every admitted length in `0..255`.

`SHOW CREATE TABLE` renders `varchar(n)` in column definitions, `NOT NULL` for
non-nullable descriptors, `DEFAULT NULL` for nullable descriptors with no
explicit default, and no default clause for `NOT NULL` no-explicit-default
descriptors. String defaults are not rendered because they are not yet stored.

## Diagnostics

The implementation must keep existing diagnostics for public API misuse,
syntax errors, missing selected schemas, unknown schemas, unknown tables,
reserved `_mylite_*` names, unsupported object kinds, unknown columns,
allocation failures, physical SQLite failures, file-format failures, and
statement-context failures.

New or newly reachable diagnostics:

- `VARCHAR` length outside MyLite's admitted `0..255` range:
  deterministic MyLite unsupported or range diagnostic before catalog mutation;
- non-integer `VARCHAR` length expression, signed length, omitted length:
  syntax error or deterministic unsupported diagnostic;
- explicit string defaults: syntax error or deterministic unsupported
  diagnostic before catalog mutation;
- string value into an integer-family column: deterministic unsupported
  conversion diagnostic;
- integer/boolean value into a `VARCHAR` column: deterministic unsupported
  conversion diagnostic;
- overlength `VARCHAR` value: MySQL error `1406`, SQLSTATE `22001`, message
  `Data too long for column '<column>' at row <n>`;
- invalid UTF-8 or embedded `NUL`: deterministic MyLite unsupported diagnostic;
- `NULL` into `VARCHAR NOT NULL`: MySQL error `1048`, SQLSTATE `23000`;
- no default for `VARCHAR NOT NULL`: MySQL error `1364`, SQLSTATE `HY000`;
- `INSERT IGNORE` adjustment for `NULL` or no default: warnings `1048` or
  `1364` with existing MyLite warning-result behavior;
- `ORDER BY`, `DISTINCT`, grouping, and non-`IS NULL` predicates over
  `VARCHAR`: deterministic unsupported diagnostics explaining that the current
  operation supports only documented string aggregate or null-test forms.

## Performance and SQLite Integration

This feature uses MyLite wrapper/translation code and public SQLite prepared
statement APIs only. No SQLite fork patch is required.

The supported fast path stays close to SQLite:

- SQLite stores `TEXT` values in ordinary rowid tables;
- descriptor-backed `SELECT` scans and projects SQLite columns directly;
- `INSERT`, `REPLACE`, and `UPDATE` bind text parameters instead of building
  string SQL;
- `IS NULL` predicates are lowered to SQLite predicates;
- no string values are materialized outside the existing public result object
  except during literal decoding, statement planning, and validation of
  materialized `INSERT ... SELECT` rows.

MyLite deliberately rejects operations that would otherwise need in-memory
collation emulation or full string expression evaluation in this slice.

## Compatibility Updates

After implementation, update:

- `COMPATIBILITY.md` to mark `VARCHAR` and ordinary string literals as limited;
- `docs/compatibility/type-system-literals-conversion.md` for the admitted
  `VARCHAR(0..255)` and string literal conversion subset;
- `docs/compatibility/sql-table-ddl.md` for `CREATE TABLE`, `ALTER TABLE ADD`,
  `CREATE TABLE LIKE`, and limited CTAS descriptor copying;
- `docs/compatibility/sql-table-dml.md` for string row values in admitted
  `INSERT`, `REPLACE`, and `UPDATE` forms;
- `docs/compatibility/sql-query-expressions.md` only for `VARCHAR` readback
  and null-test predicates;
- `docs/compatibility/sql-show-statements.md` for `SHOW COLUMNS` and
  `SHOW CREATE TABLE` rendering;
- `docs/compatibility/character-sets.md` and `docs/compatibility/collations.md`
  to state that `VARCHAR` storage exists but charset conversion and collation
  comparison semantics remain deferred.

## Test Plan

Add `packages/libmylite/tests/runtime_varchar_type_test.c` and register it as
`libmylite.runtime.varchar_type`.

Coverage must include:

- parser acceptance for `VARCHAR(0)`, `VARCHAR(1)`, and `VARCHAR(255)` column
  definitions and string values in `INSERT` / `UPDATE` grammar positions;
- parser/analyzer rejection for missing, signed, expression, and out-of-scope
  lengths;
- successful `CREATE TABLE` with nullable and `NOT NULL` `VARCHAR` columns;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  rendering;
- `INSERT ... VALUES`, multi-row values, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and `UPDATE` string assignments;
- `NULL`, empty string, trailing spaces within length, doubled quotes, decoded
  backslash escapes including ordinary `\%` and `\_`, and UTF-8 text without
  embedded `NUL`;
- `VARCHAR(0)` empty-string and `NULL` behavior;
- `NULL` into `NOT NULL`, omitted `NOT NULL`, `DEFAULT` for nullable, and
  `INSERT IGNORE` adjustments for admitted string columns;
- deterministic rejection of integer-to-`VARCHAR`, string-to-integer,
  overlength, invalid UTF-8, embedded `NUL`, and explicit string default cases;
- `WHERE varchar_col IS NULL` and `IS NOT NULL`;
- rejection of string comparison predicates, `BETWEEN`, `IN`, truth tests,
  `ORDER BY`, `DISTINCT`, grouped string columns, and numeric aggregates over
  `VARCHAR` outside separately documented aggregate slices;
- `CREATE TABLE LIKE`, `CREATE TABLE ... SELECT`, and `INSERT ... SELECT` for
  compatible `VARCHAR` source/target descriptors;
- reopen persistence, update persistence, rename/drop interaction, preamble
  preservation, and independent file-backed handles;
- cleanup of zero-initialized plans and planner failure paths;
- existing lexer, parser, runtime row values, show, alter, insert, replace,
  update, delete, select, catalog, storage, VFS, diagnostics, and workflow
  checks.

Verification commands:

1. `cmake --build --preset dev`
2. `ctest --preset dev -R 'libmylite\\.(parser|runtime\\.varchar_type|runtime\\.row_values_lifecycle|runtime\\.update_lifecycle|runtime\\.insert_set_lifecycle|runtime\\.replace_values_lifecycle|runtime\\.replace_set_lifecycle|runtime\\.show_columns_introspection|runtime\\.show_create_table)' --output-on-failure`
3. `./packages/libmylite/tests/mysql_baseline_varchar_type_expectations.sh`
4. `cmake --workflow --preset check`
