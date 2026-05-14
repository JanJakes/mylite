# Baseline ENUM Type

## Status

This phase adds a deliberately narrow descriptor-owned `ENUM` column type for
common MySQL application schemas:

```sql
CREATE TABLE posts (status ENUM('draft','published') NOT NULL)
INSERT INTO posts VALUES ('draft')
UPDATE posts SET status = 'published' WHERE status = 'draft'
```

The implementation stores the selected label in SQLite `TEXT` columns and keeps
MyLite descriptors authoritative for the permitted label list, defaults,
conversion, result metadata, and introspection. It does not add MySQL's internal
ordinal storage model, enum ordering, invalid index-0 storage, column-level
collations, or full expression semantics.

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
- Baseline row values, string defaults, `VARCHAR`, `TEXT`, update, result
  metadata, primary-key, and prefix-index specs:
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-string-defaults/specs.md`,
  `docs/specs/baseline-varchar-type/specs.md`,
  `docs/specs/baseline-text-family-types/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-result-column-metadata/specs.md`,
  `docs/specs/baseline-primary-key-lifecycle/specs.md`,
  `docs/specs/baseline-index-prefix-key-parts/specs.md`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ENUM` type:
  https://dev.mysql.com/doc/refman/8.4/en/enum.html
- MySQL 8.4 Reference Manual, string type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, data type storage requirements:
  https://dev.mysql.com/doc/refman/8.4/en/storage-requirements.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes were run against local container `mylite-mysql-849` using
`docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --force`.

Observed behavior shaping this slice:

- `ENUM('b','a')` stores and reads labels, while numeric context exposes
  ordinal indexes beginning at `1`.
- `SHOW COLUMNS` and `SHOW CREATE TABLE` render lower-case
  `enum('label',...)` type text and preserve the label lettercase from the
  definition.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = enum`, `COLUMN_TYPE` as the
  full enum definition, `CHARACTER_MAXIMUM_LENGTH` as the longest label length,
  `CHARACTER_OCTET_LENGTH` as that length multiplied by four under `utf8mb4`,
  and the table default character set/collation.
- C API metadata for table-backed enum columns reports field type `STRING`, the
  connection collation, length equal to the longest label length multiplied by
  four, and the `ENUM` flag.
- `ENUM` labels must be quoted string literals. A default must be `NULL` for a
  nullable column or a matching string label; unquoted ordinal defaults such as
  `DEFAULT 2` fail with invalid-default diagnostics.
- `ENUM` labels have trailing spaces removed when the table is created.
- Duplicate labels after trailing-space removal and default collation matching
  fail with `1291 / HY000` under the default strict mode.
- Under `utf8mb4_0900_ai_ci`, labels match ASCII case-insensitively for
  assignment and comparison, while readback uses the definition label's
  original lettercase.
- Omitted values for nullable enum columns use `NULL`. Omitted values for
  `NOT NULL` enum columns without an explicit default store the first label,
  while `SHOW CREATE TABLE` still omits an explicit default clause.
- Assigning a numeric literal to an enum column stores the label at that ordinal
  if the ordinal is in range. Assigning a quoted numeric string first tries to
  match a label; if no label matches and the string is a valid ordinal, MySQL
  stores the label at that ordinal.
- Equality predicates compare string literals as labels and decimal integer
  literals as ordinals. A string literal that looks numeric is not converted to
  an ordinal in a predicate.
- `ORDER BY enum_col` sorts by enum ordinal with `NULL` before non-`NULL`
  values. This is deferred because this slice stores labels as SQLite `TEXT`.
- Invalid enum row values fail under the default strict mode with
  `1265 / 01000`. In non-strict or `IGNORE` paths, MySQL can store a special
  empty-string error value with ordinal `0`; that storage model is deferred.
- Full secondary indexes and primary keys over enum columns are accepted by
  MySQL. Prefix lengths on enum key parts fail with `1089 / HY000`. MyLite
  defers enum keys until enum collation and ordinal interactions are designed.

## Scope

Supported:

- persistent and shadowing temporary base tables where the existing DDL/DML
  paths already support the statement class;
- `ENUM('label'[, ...])` column definitions in `CREATE TABLE`,
  `CREATE TABLE ... LIKE`, and `ALTER TABLE ... ADD [COLUMN]`;
- one or more ordinary single- or double-quoted string literal labels, decoded
  through the current MyLite SQL string literal policy;
- labels with valid non-`NUL` UTF-8 bytes, including the empty string;
- trailing-space removal from definition labels before duplicate checks and
  descriptor serialization;
- duplicate detection with the current MyLite ASCII case-insensitive
  `utf8mb4_0900_ai_ci` subset;
- durable logical descriptor text `ENUM('label',...)` and physical descriptor
  text `TEXT`;
- row values for `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and one-assignment `UPDATE`;
- enum row assignments from `NULL`, `DEFAULT`, a matching string label, an
  in-range decimal integer ordinal, or a quoted decimal string that has no
  label match and names an in-range ordinal;
- nullable omitted/default values as `NULL`, explicit string defaults as the
  matching definition label, and omitted/default `NOT NULL` no-explicit-default
  values as the first definition label;
- strict invalid-value diagnostics for unsupported labels, ordinal `0`, and
  out-of-range ordinals;
- `NULL` into `NOT NULL` diagnostics using the existing row-values policy;
- descriptor-backed `WHERE` predicates for `IS NULL`, `IS NOT NULL`, `=`,
  `<=>`, `<>`, and `!=` with a string-label or decimal-integer-ordinal right
  operand, plus `<=> NULL`;
- descriptor-backed result metadata for table-backed enum columns, including a
  public enum flag bit matching MySQL protocol flag values;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, limited
  `INFORMATION_SCHEMA.COLUMNS`, and `CREATE TABLE ... LIKE` descriptor
  rendering;
- reopen persistence, table rename/drop behavior, `.mylite` preamble
  preservation, and independent file-backed handle isolation.

Deferred:

- MySQL's compact ordinal physical storage and invalid index-0 value;
- non-strict invalid enum insertion, `INSERT IGNORE` invalid enum demotion, and
  warning-producing invalid conversion;
- enum `ORDER BY`, enum `GROUP BY`, enum `DISTINCT`, numeric enum expressions
  such as `enum_col + 0`, casts, functions, aggregates, expression defaults,
  generated columns, or parameters;
- enum primary keys, unique keys, nonunique secondary indexes, prefix key
  parts, foreign-key participation, and optimizer/index-use guarantees;
- column-level `CHARACTER SET`, `CHARSET`, `COLLATE`, `BINARY`, national
  string, introducer, adjacent literal concatenation, and full non-ASCII
  collation semantics;
- `ALTER TABLE ... MODIFY [COLUMN]` / `CHANGE [COLUMN]` conversion to or from
  enum descriptors;
- enum values in `INSERT ... SELECT`, scalar subquery assignment, arbitrary
  expression assignment, and general cross-descriptor conversion;
- the MySQL maximum of 65,535 enum elements. MyLite's current baseline is
  bounded by the durable logical-type descriptor text capacity.

## Ownership Boundary

- Public API: no new functions. `mylite_execute()` owns public misuse behavior,
  result lifetime, and cleanup. The public result-column flag mask gains the
  stable `MYLITE_RESULT_COLUMN_FLAG_ENUM` bit used by metadata accessors.
- Statement context: owns diagnostics, warning count, affected rows, insert id,
  and statement transaction completion. Successful supported enum operations
  report `warning_count == 0`.
- Lexer/parser/AST: admits enum type syntax and stores string-label literal
  nodes. It does not resolve labels, defaults, table descriptors, or physical
  storage.
- Analyzer/planner/runtime: normalizes labels, resolves defaults and row values
  against enum descriptors, validates strict conversion, maps compatible
  predicate values, and rejects unsupported enum contexts before generated
  SQLite SQL is built.
- Catalog: MyLite catalog descriptors are authoritative for the enum label
  list, logical type, physical type, nullability, defaults, visibility, and
  column order. SQLite schema text is never consulted as logical metadata.
- Result and introspection builders: render enum descriptors from catalog text
  and compute metadata lengths from decoded descriptor labels.
- SQLite physical row storage: stores the selected enum label as `TEXT` in the
  stable generated physical user table. MyLite binds label bytes and `NULL`
  values with prepared statements.
- Storage/VFS: unchanged. Enum data lives inside the shifted SQLite payload and
  must not mutate the `.mylite` preamble.

No SQLite fork patch is required for this slice. The implementation uses
MyLite-side descriptor translation and public SQLite prepared statements.

## Supported SQL Grammar

Supported column type surface:

```sql
ENUM ( string_literal [, string_literal ...] )
```

`ENUM` remains an identifier in non-type contexts, matching the current lexer
keyword policy. The parser must reject empty label lists, non-string label
tokens, expressions, parameters, user variables, subqueries, and type
attributes outside the existing column-attribute grammar.

MyLite Lemon-syntax sketch:

```lemon
column_type ::= enum_type.

enum_type ::= ENUM LPAREN enum_label_list RPAREN.

enum_label_list ::= string_literal.
enum_label_list ::= enum_label_list COMMA string_literal.
```

The analyzer owns label decoding, trailing-space trimming, duplicate checks,
descriptor-size checks, and all MySQL-compatible diagnostics.

## Descriptor and Conversion Semantics

The logical descriptor text is canonicalized as:

```sql
ENUM('label1','label2')
```

Labels are serialized with single-quote escaping. The physical descriptor text
is `TEXT`. The logical type text must fit MyLite's durable descriptor capacity.
This phase may raise MyLite's internal descriptor text capacity, but it must
not change the on-disk catalog table schema unless a schema migration is
actually needed.

Definition labels are decoded with the session string-literal policy active at
parse/analyze time, then trailing ASCII spaces are removed. NUL bytes are
rejected. Duplicate labels compare through the current ASCII case-insensitive
default collation subset.

String assignment conversion:

- decode the string literal according to the current session string-literal
  policy;
- remove trailing ASCII spaces before enum label and ordinal matching;
- find a definition label using the current ASCII case-insensitive enum match;
- store the definition label bytes, preserving the definition's lettercase;
- if no label matches and the decoded string is an unsigned decimal integer
  naming an in-range ordinal `1..label_count`, store that ordinal's label;
- otherwise fail with a strict invalid enum value diagnostic.

Integer assignment conversion:

- accepts only decimal integer literal values already admitted by the DML value
  grammar;
- ordinal `1` stores the first definition label, ordinal `2` the second, and so
  on;
- ordinal `0`, negative values, and values greater than the label count fail in
  this strict baseline.

Default conversion:

- `DEFAULT NULL` is accepted only for nullable enum columns;
- explicit string defaults must match a definition label and store that
  definition label text in catalog default metadata;
- unquoted numeric defaults are rejected in this slice because MySQL rejects
  them for enum definitions;
- nullable no-explicit-default columns insert/read `NULL` when omitted or when
  assigned `DEFAULT`;
- `NOT NULL` no-explicit-default columns insert/read the first definition label
  when omitted or assigned `DEFAULT`, while descriptor metadata remains
  no-explicit-default.

Predicate conversion:

- `IS NULL` and `IS NOT NULL` do not bind values;
- string-literal right operands for `=`, `<=>`, `<>`, and `!=` compare as enum
  labels only and do not use numeric-string ordinal fallback or assignment-style
  trailing-space trimming;
- decimal integer right operands for those operators compare as ordinals
  `1..label_count`;
- `NULL` right operands keep the existing null-safe or NULL comparison
  behavior;
- unsupported enum predicate operators, `BETWEEN`, `IN`, `LIKE`, range
  comparisons, expression operands, table-qualified operands in DML, and source
  qualified enum operands outside existing `SELECT` resolution remain
  rejected.

## Metadata and Introspection

For a descriptor `ENUM('b','alpha')` under default `utf8mb4` metadata:

- `SHOW COLUMNS.Type` and `SHOW CREATE TABLE` render `enum('b','alpha')`;
- `INFORMATION_SCHEMA.COLUMNS.DATA_TYPE` is `enum`;
- `INFORMATION_SCHEMA.COLUMNS.COLUMN_TYPE` is `enum('b','alpha')`;
- `CHARACTER_MAXIMUM_LENGTH` is the maximum decoded label character length;
- `CHARACTER_OCTET_LENGTH` is that length multiplied by four;
- `CHARACTER_SET_NAME` and `COLLATION_NAME` use the table default metadata;
- result-column type is MySQL field type `STRING`;
- result-column length is the maximum decoded label character length multiplied
  by four;
- result-column flags include `MYLITE_RESULT_COLUMN_FLAG_ENUM` plus existing
  nullability/key/default flags where applicable.

## Generated SQLite Shape and Performance

Generated physical table DDL uses the stable physical column name from the
descriptor and SQLite type `TEXT`. Row DML keeps using existing descriptor-built
`INSERT`, `REPLACE`, and `UPDATE` statements with bound values. Enum conversion
is a scalar pre-bind step; it does not require scanning or materializing the
target table.

Filtered equality predicates lower to direct SQLite text comparisons against a
bound normalized label. This stays close to SQLite's ordinary indexable path
once enum indexes are later admitted. `ORDER BY enum_col` is not admitted in
this phase because plain SQLite text ordering would not match MySQL ordinal
ordering.

## Diagnostics

Diagnostics should use MySQL-compatible codes/states/messages where the
project already has helpers or where the behavior is covered by runtime
expectations. Otherwise MyLite-specific deterministic diagnostics are allowed
and must be covered by tests.

Required diagnostics:

- syntax errors for malformed `ENUM` type grammar;
- unsupported enum label expressions, variables, parameters, subqueries, and
  non-string labels;
- empty enum label list;
- decoded enum label containing `NUL`;
- duplicate enum labels after trailing-space normalization and default
  collation matching;
- descriptor logical type text too long for MyLite's current catalog capacity;
- invalid enum explicit default;
- `DEFAULT NULL` or explicit `NULL` assigned to `NOT NULL` enum columns;
- invalid string labels, invalid numeric-string ordinals, ordinal `0`, negative
  ordinals, and out-of-range ordinals for row DML;
- invalid enum predicate labels or ordinals;
- unsupported enum ordering, range predicates, `IN`, `LIKE`, keys/indexes,
  scalar subquery assignment, `INSERT ... SELECT`, and modify/change
  conversions;
- unknown schema/table/column names and reserved `_mylite_*` names through
  existing descriptor-resolution paths;
- physical SQLite failures, allocation failures, and public API misuse through
  existing runtime paths.

## Compatibility Documentation

Update the compatibility matrix and detail docs only after implementation
matches this specification:

- `COMPATIBILITY.md`
- `docs/compatibility/type-system-literals-conversion.md`
- `docs/compatibility/sql-table-ddl.md`
- `docs/compatibility/sql-table-dml.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/error-warning-result-semantics.md` if the public enum
  result flag is documented there.

The wording must remain partial. Do not claim MySQL's full enum ordinal storage
model, invalid index-0 values, enum ordering, enum indexes, full collations,
full expression semantics, or protocol-grade wire metadata.

## Test Plan

Add MySQL-runtime expectation coverage and fast C tests for:

- create-time enum definitions, trailing-space normalization, duplicate-label
  diagnostics, empty-list diagnostics, NUL-label diagnostics, and descriptor
  capacity diagnostics;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`, and
  result-column metadata including the enum flag;
- insert, replace, and update row values from labels, ASCII case variants,
  integer ordinals, quoted numeric ordinals, `NULL`, `DEFAULT`, omitted nullable
  values, omitted `NOT NULL` no-explicit-default values, and explicit defaults;
- strict invalid labels, invalid ordinals, ordinal `0`, negative ordinals,
  out-of-range ordinals, and `NULL` into `NOT NULL`;
- equality, null-safe equality, inequality, `IS NULL`, and `IS NOT NULL`
  predicates over enum columns with string labels and integer ordinals;
- unsupported enum `ORDER BY`, range predicates, `IN`, `LIKE`, keys/indexes,
  `INSERT ... SELECT`, scalar subquery assignment, and modify/change
  conversion;
- `CREATE TABLE ... LIKE`, table rename/drop, reopen persistence, physical
  preamble preservation, and independent file-backed handles;
- zero-initialized cleanup for new parser/AST/planner helper objects;
- existing lexer, parser, runtime DDL/DML, metadata, catalog, VFS, and
  registration tests.

Verification before marking done:

1. `cmake --build --preset dev`
2. Focused CTest entries for parser, enum runtime, row-values, update,
   information schema, show, result metadata, create-like, rename/drop, and
   file-format coverage.
3. `packages/libmylite/tests/mysql_baseline_enum_type_expectations.sh`
4. `cmake --workflow --preset check`
