# Baseline Table Default Binary Charset

## Status

This feature extends the existing table charset/collation surface with one
narrow `CREATE TABLE` inheritance case:

```sql
CREATE [TEMPORARY] TABLE table_name (
    ...
) [DEFAULT] CHARSET [=] binary

CREATE [TEMPORARY] TABLE table_name (
    ...
) [DEFAULT] CHARACTER SET [=] binary

CREATE [TEMPORARY] TABLE table_name (
    ...
) [DEFAULT] COLLATE [=] binary
```

The slice applies only while planning a newly created table. Inherited
`CHAR`, `VARCHAR`, and `TEXT` family descriptors become MyLite's existing
binary string or BLOB descriptors. Explicit column charset/collation attributes
continue to override the table default. Existing tables, `ALTER TABLE ...
DEFAULT CHARSET=binary`, charset conversion, binary indexes, expression
semantics, and binary defaults remain deferred.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing table charset/collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- Column charset/collation attributes:
  `docs/specs/baseline-column-charset-collation-attributes/specs.md`
- Binary charset/collation metadata:
  `docs/specs/baseline-binary-character-set-collation/specs.md`
- Binary string descriptors:
  `docs/specs/baseline-binary-string-types/specs.md`
- MySQL 8.4 Reference Manual, table character set and collation:
  https://dev.mysql.com/doc/refman/8.4/en/charset-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes recorded in
`packages/libmylite/tests/mysql_baseline_table_default_binary_charset_expectations.sh`
verify:

- `DEFAULT CHARSET=binary`, `CHARACTER SET binary`, `CHARSET binary`,
  `COLLATE=binary`, and `CHARACTER SET binary COLLATE binary` are accepted.
- `SHOW CREATE TABLE` renders inherited `VARCHAR(n)` as `varbinary(n)`,
  `CHAR(n)` as `binary(n)`, `TINYTEXT` as `tinyblob`, `TEXT` as `blob`,
  `MEDIUMTEXT` as `mediumblob`, and `LONGTEXT` as `longblob`.
- `SHOW CREATE TABLE` renders the table suffix as `DEFAULT CHARSET=binary`
  without a separate `COLLATE=binary` clause.
- `INFORMATION_SCHEMA.COLUMNS` reports binary/BLOB data types and `NULL`
  charset/collation for the converted columns.
- Inserted string values are stored/read as bytes. Fixed `BINARY(n)` values are
  right-padded with NUL bytes.
- Explicit column `CHARACTER SET utf8mb4` or `COLLATE utf8mb4_*` overrides the
  binary table default for that column.
- `ENUM` and `SET` columns did not render explicit binary charset/collation
  metadata in the observed table-default case.
- Binary table-default columns may be indexed in MySQL. MyLite defers binary
  key parts because binary indexes are not yet part of the descriptor subset.
- `CHARACTER SET binary COLLATE utf8mb4_bin` and
  `CHARACTER SET utf8mb4 COLLATE binary` fail with `1253 / 42000`.
- Conflicting repeated character-set declarations fail with `1302 / HY000`.
  This slice does not broaden MyLite's existing repeated-option handling beyond
  the deterministic admitted subset below.

## Scope

Supported:

- persistent and session-temporary `CREATE TABLE` statements that already use
  the existing explicit-column definition path;
- table default `binary` charset/collation option names as unquoted
  identifiers, backtick-quoted identifiers, or string literals;
- ASCII case-insensitive matching of `binary`;
- inherited conversion for current non-national `CHAR`, `VARCHAR`, `TINYTEXT`,
  `TEXT`, `MEDIUMTEXT`, and `LONGTEXT` descriptors that do not have an
  explicit column charset/collation attribute;
- explicit column `utf8mb4` charset/collation override preservation;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, limited
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLE STATUS`,
  `INFORMATION_SCHEMA.TABLES`, result metadata, DML storage/readback,
  `CREATE TABLE ... LIKE`, reopen persistence, independent handles, and
  `.mylite` preamble preservation through existing paths;
- warning count `0`, affected rows `0`, and existing non-row result
  conventions for successful DDL.

Deferred:

- `ALTER TABLE ... DEFAULT CHARSET=binary`;
- `ALTER TABLE ... CONVERT TO CHARACTER SET binary`;
- database/server/client default charset state;
- binary defaults for inherited binary string descriptors;
- binary string indexes, binary primary keys, and binary unique keys;
- binary comparison, ordering, grouping, distinct, `LIKE`, regexp, aggregate,
  and expression semantics beyond existing binary storage/readback;
- table-default binary behavior for generated columns, views, partitions,
  comments, privileges, storage options, and full MySQL repeated-option
  diagnostics.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to own statement
  boundaries, result allocation, and cleanup.
- Statement context: owns diagnostics, warning count, row-count state, and
  selected schema.
- Lexer/parser/AST: admits `binary` only in the targeted `CREATE TABLE` table
  option positions. It does not make `BINARY` a general `option_name`, avoiding
  accidental grammar widening for unrelated expressions.
- Analyzer/planner/runtime: validates table charset/collation options and
  applies inherited binary conversion before key/check/default finalization can
  use the planned descriptors.
- Catalog: remains authoritative for table defaults and column descriptors.
  Converted columns are stored as binary string/BLOB logical descriptors with
  empty per-column charset/collation metadata, matching existing explicit
  column-level binary normalization.
- SQLite physical storage: unchanged. Generated physical tables use the
  descriptor's existing physical type (`BLOB` for binary string/BLOB
  descriptors) and existing prepared-statement value binding.
- Storage/VFS: unchanged. The feature writes only inside the shifted SQLite
  payload and does not touch the `.mylite` preamble or SQLite fork.

## Supported Grammar

The feature extends only `CREATE TABLE` table options:

```lemon
table_option(A) ::= default_opt CHARSET(C) equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_charset_option(
        state, C, mylite_sql_parser_make_identifier(state, N));
}
table_option(A) ::= default_opt CHARACTER(C) SET equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_charset_option(
        state, C, mylite_sql_parser_make_identifier(state, N));
}
table_option(A) ::= default_opt COLLATE(C) equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_collation_option(
        state, C, mylite_sql_parser_make_identifier(state, N));
}
```

The existing `option_name` grammar continues to admit identifiers and string
literals for other charset/collation names. `ALTER TABLE ... DEFAULT
CHARSET=binary` remains outside this slice.

## Descriptor Semantics

If the final validated table charset/collation defaults are `binary`, the table
descriptor stores:

- `default_charset = "binary"`
- `default_collation = "binary"`

For each planned column without explicit column charset/collation attributes:

- `CHAR(n)` becomes `BINARY(n)`;
- `VARCHAR(n)` becomes `VARBINARY(n)`;
- `TINYTEXT` becomes `TINYBLOB`;
- `TEXT` becomes `BLOB`;
- `MEDIUMTEXT` becomes `MEDIUMBLOB`;
- `LONGTEXT` becomes `LONGBLOB`.

National `CHAR` / `VARCHAR` aliases keep their current national descriptor
semantics. `ENUM` and `SET` descriptors are left unchanged for this slice,
matching the observed MySQL table-default rendering and avoiding new
collation-sensitive enum/set behavior.

Explicit column charset/collation attributes are planned first and win over the
table default:

- `VARCHAR(10) CHARACTER SET utf8mb4` remains `VARCHAR(10)` with explicit
  `utf8mb4_0900_ai_ci` metadata;
- `CHAR(2) COLLATE utf8mb4_bin` remains `CHAR(2)` with explicit
  `utf8mb4_bin` metadata;
- explicit column-level `CHARACTER SET binary` or `COLLATE binary` continues
  to use the previous column-level binary normalization path.

## Physical SQL

No user SQL is passed through to SQLite. MyLite continues to generate physical
`CREATE TABLE` statements from planned descriptors, quote all identifiers, and
use stable physical table names. Binary-inherited descriptors use the same
physical storage as explicit binary descriptors:

- `BINARY` / `VARBINARY` and BLOB-family logical descriptors use SQLite `BLOB`;
- row DML binds binary values with prepared statements through the existing
  binary conversion path;
- fixed `BINARY(n)` values are padded by MyLite-owned conversion before
  binding.

`SHOW CREATE TABLE` renders `DEFAULT CHARSET=binary` and omits
`COLLATE=binary`, matching the MySQL 8.4.9 observed table suffix.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| Unknown charset | Existing `1115 / 42000` unknown character set diagnostic |
| Unknown collation | Existing `1273 / HY000` unknown collation diagnostic |
| `CHARACTER SET binary COLLATE utf8mb4_*` | Existing `1253 / 42000` collation-not-valid diagnostic |
| `CHARACTER SET utf8mb4 COLLATE binary` | Existing `1253 / 42000` collation-not-valid diagnostic |
| Binary-inherited column default | Existing invalid-default diagnostic until binary defaults are implemented |
| Binary-inherited key part | Existing unsupported key-column diagnostic until binary indexes are implemented |
| Missing selected schema, unknown schema/table, reserved names | Existing `CREATE TABLE` diagnostics |
| Allocation failure | Existing `MYLITE_NOMEM` path |

## Tests

Add fast C tests under the existing table charset/collation runtime test and
parser coverage for:

- parser acceptance of `DEFAULT CHARSET=binary`, `CHARACTER SET binary`,
  `CHARSET binary`, and `COLLATE binary` in `CREATE TABLE`;
- persistent create with inherited `VARCHAR`, `CHAR`, and `TEXT` families;
- `SHOW CREATE TABLE` suffix and converted column types;
- `SHOW FULL COLUMNS` and limited `INFORMATION_SCHEMA.COLUMNS` charset and
  collation metadata;
- inserted value bytes and fixed `BINARY(n)` padding;
- explicit `utf8mb4` column override under a binary table default;
- temporary table behavior where it falls out of the shared create path;
- `CREATE TABLE ... LIKE` clone of descriptors and table defaults;
- reopen persistence, independent handles, and preamble preservation;
- deterministic diagnostics for binary/utf8mb4 charset-collation mismatch,
  inherited binary defaults, and inherited binary key parts.

Run:

1. `packages/libmylite/tests/mysql_baseline_table_default_binary_charset_expectations.sh`
2. focused parser/runtime CTest entries
3. `cmake --build --preset dev`
4. `cmake --workflow --preset check`
