# Baseline CHAR Type

## Status

This feature specifies the fixed-length character-string slice for persistent
`.mylite` handles. It adds descriptor-owned `CHAR` and `CHAR(n)` columns on top
of the existing integer, `VARCHAR`, and `TEXT` family storage, DML, and
introspection paths.

The feature is intentionally not full MySQL character-string support. It stores
and returns MySQL's default visible `CHAR` value shape for the fixed
`utf8mb4` / `utf8mb4_0900_ai_ci` surface: UTF-8 non-`NUL` text, default-mode
trailing-space stripping, strict nonspace overlength errors, and silent
truncation of excess trailing spaces. It does not implement
`PAD_CHAR_TO_FULL_LENGTH`, string defaults, column-level character sets or
collations, string comparison semantics, string ordering, string indexes, or
protocol-grade string metadata.

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
- Baseline VARCHAR type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline TEXT type:
  `docs/specs/baseline-text-type/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline INFORMATION_SCHEMA core:
  `docs/specs/baseline-information-schema-core/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, string data type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, `CHAR` and `VARCHAR`:
  https://dev.mysql.com/doc/refman/8.4/en/char.html
- MySQL 8.4 Reference Manual, keywords and reserved words:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_char_type_expectations.sh` records the
runtime probes for this feature. Observed behavior that shapes this slice:

- MySQL 8.4.9 under its default strict SQL mode accepts bare `CHAR` as
  `char(1)`, accepts `CHAR(0)`, accepts lengths through `CHAR(255)`, rejects
  `CHAR(256)` with error `1074`, and rejects `CHAR()` and negative lengths
  with syntax error `1064`.
- `SHOW COLUMNS` renders `char(n)`, including `char(1)` for bare `CHAR`.
  `SHOW CREATE TABLE` renders nullable `CHAR` columns with `DEFAULT NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE` as `char`, `COLUMN_TYPE` as
  `char(n)`, character length as `n`, octet length as `n * 4` under `utf8mb4`,
  and the fixed default charset/collation names.
- MySQL stores fixed-length `CHAR` values padded internally but, unless
  `PAD_CHAR_TO_FULL_LENGTH` is enabled, retrieves values with trailing spaces
  removed.
- Strict-mode assignment of nonspace-overlength values fails with error `1406`,
  SQLSTATE `22001`, and `Data too long for column ...`.
- Assignment of only excess trailing spaces to `CHAR` succeeds silently and the
  value reads back without those trailing spaces.
- `CHAR(0)` nullable columns store and read back either `NULL` or the empty
  string; assigning a nonspace character to `CHAR(0)` fails in strict mode.
- `INSERT IGNORE` demotes nonspace-overlength and `NULL` into `NOT NULL`
  failures to warnings and stores adjusted values. This original `CHAR` slice
  supports existing `NULL` and no-default string adjustments; the later
  baseline non-strict string truncation slice adds limited DML overlength
  warning demotion.
- Single-table `UPDATE` reports changed-row affected counts after default
  `CHAR` trimming. Reassigning `'y '` to a `CHAR(1)` column that already reads
  back as `'y'` reports zero affected rows.
- MySQL accepts explicit string defaults such as `CHAR(2) DEFAULT 'x'`. MyLite
  defers string default catalog storage in this slice.

## Scope

The implementation must add:

- parser and AST support for bare `CHAR` and `CHAR(length)` column types;
- descriptor-owned logical type text `CHAR(n)` for admitted lengths `0..255`,
  with bare `CHAR` normalized to `CHAR(1)`;
- physical SQLite type text `TEXT` for admitted `CHAR` descriptors;
- `CREATE TABLE` support for persistent base tables containing admitted
  `CHAR` columns;
- `ALTER TABLE ... ADD [COLUMN]` support for `CHAR` columns, including
  physical empty-string backfill for `NOT NULL` no-explicit-default additions;
- `CREATE TABLE ... LIKE` descriptor cloning for tables containing admitted
  `CHAR` columns;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source and target values are already
  compatible with admitted `CHAR` target descriptors;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for `CHAR(n)` descriptors;
- ordinary string literal values for `INSERT ... VALUES`, `INSERT ... SET`,
  `REPLACE ... VALUES`, `REPLACE ... SET`, and single-table `UPDATE`
  assignments into `CHAR` columns;
- `NULL` assignment and effective nullable `DEFAULT NULL` materialization for
  `CHAR` columns;
- MyLite-owned default-mode `CHAR` canonicalization before binding row values:
  UTF-8 validation, embedded-`NUL` rejection, trailing-space trimming, silent
  excess-trailing-space truncation, and strict nonspace-overlength errors;
- limited `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` adjustment for
  `NULL` into `CHAR NOT NULL` and omitted or explicit `DEFAULT` for no-
  explicit-default `CHAR NOT NULL`, storing the MySQL implicit empty string and
  recording warnings;
- descriptor-backed `SELECT` readback of SQLite `TEXT` values as public result
  text in the same canonical default-mode shape;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  `CHAR` columns;
- deterministic rejection of collation-sensitive `CHAR` comparisons,
  `BETWEEN`, `IN`, truth predicates, ordering, `DISTINCT`, grouped columns,
  and numeric aggregates; `COUNT(DISTINCT column)` is covered by
  `docs/specs/baseline-string-count-distinct-aggregates/specs.md`;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted `CHAR` data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `CHARACTER`, `CHARACTER VARYING`, `NCHAR`, `NATIONAL CHAR`, `NVARCHAR`,
  `BINARY`, `VARBINARY`, `BLOB`, `ENUM`, `SET`, `JSON`, or other deferred
  string/binary-family types;
- `CHAR BYTE`, `ASCII`, `UNICODE`, `CHARACTER SET`, `CHARSET`, `COLLATE`, or
  `BINARY` column attributes;
- mutable `PAD_CHAR_TO_FULL_LENGTH` behavior or padded physical readback;
- explicit string defaults, expression defaults, `DEFAULT(col_name)`, or
  string default catalog storage;
- string-to-integer or integer-to-string DML conversion;
- string comparison predicates, `LIKE` over table data, `REGEXP`, collations,
  coercibility, `ORDER BY` over string columns, grouped string keys, string
  `DISTINCT`, or collation-aware uniqueness;
- `CHAR` primary keys, secondary indexes, prefix indexes, or unique indexes;
- adjacent string literal concatenation, national strings, introducers,
  hex/bit string values, parameters, user variables, functions, arbitrary
  expressions, or scalar string projection;
- embedded `NUL` result values or binary strings;
- `ALTER TABLE ... MODIFY [COLUMN]` or `CHANGE [COLUMN]` to or from `CHAR`;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and failure cleanup. The public
  result API remains NUL-terminated text, so embedded `NUL` string values stay
  unsupported.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported in-range `CHAR` operations record
  `warning_count == 0`; supported `INSERT IGNORE` string adjustments record
  warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for `CHAR` / `CHAR(n)` and ordinary
  string literals. They store source spans and structural payloads only; they
  do not resolve catalog descriptors or perform storage conversion.
- Analyzer/planner code maps `CHAR(n)` AST nodes to durable descriptors,
  resolves schemas/tables/columns against the MyLite catalog, decodes admitted
  string literals, validates UTF-8, applies default-mode `CHAR`
  canonicalization, rejects unsupported conversion or collation-sensitive
  operations, and produces descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, defaults, and column order. This slice reuses the
  existing string `logical_type` / `physical_type` descriptor fields and does
  not change `_mylite_catalog_columns` schema because explicit string defaults
  are deferred.
- Result and introspection builders render logical descriptors to MySQL-shaped
  text. SQLite schema text and `sqlite_schema` are not metadata authority.
- SQLite owns physical row storage, scans, and mutations for generated prepared
  statements. MyLite binds canonical `CHAR` values with length-aware
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
  | CHAR
  | CHAR ( unsigned_decimal_integer_literal )
  | existing_text_family_type
```

`CHAR` without a length is normalized to descriptor length `1`. `CHAR(n)`
length must be a decimal integer literal from `0` through `255`. Empty
parentheses, signs, expressions, parameters, and non-decimal forms are not
admitted.

DML values for `CHAR` targets reuse the existing string row-value grammar:

```sql
insert_value:
    existing_integer_or_boolean_or_NULL_or_DEFAULT_value
  | string_literal

update_value:
    existing_integer_or_boolean_or_NULL_or_DEFAULT_value
  | string_literal
```

Only ordinary MySQL string tokens already supported by MyLite are admitted.
National strings, introducers, adjacent string literal concatenation, hex/bit
strings, parameters, and expressions remain unsupported.

The corresponding MyLite Lemon-syntax snippets are:

```lemon
column_type(A) ::= char_type(T). { A = T; }

char_type(A) ::= CHAR(T). {
    A = mylite_sql_parser_make_char_type(state, T, NULL);
}

char_type(A) ::= CHAR(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_char_type(
        state,
        T,
        &(struct mylite_sql_char_type_tokens){.length_token = L, .right_paren = R}
    );
}
```

The exact constructor shape may differ to match local parser style, but the AST
must represent whether an explicit length was present and preserve the length
source span for diagnostics.

## Semantics

### Descriptor Mapping

Admitted `CHAR` definitions produce:

- logical type `CHAR(n)`, where bare `CHAR` maps to `CHAR(1)`;
- physical SQLite type `TEXT`;
- existing descriptor nullability/default/visibility metadata;
- no catalog schema change.

`n` is a MySQL character count. Under the fixed `utf8mb4` metadata surface,
`INFORMATION_SCHEMA.COLUMNS.CHARACTER_MAXIMUM_LENGTH` is `n` and
`CHARACTER_OCTET_LENGTH` is `n * 4`.

### Value Conversion

For `CHAR` targets, MyLite decodes the admitted string literal first, then:

1. rejects embedded `NUL` bytes;
2. validates UTF-8 and counts Unicode scalar byte sequences using the existing
   `VARCHAR` character-count policy;
3. trims trailing ASCII space bytes (`0x20`) from the decoded value to model
   MySQL's default retrieval shape;
4. if the untrimmed decoded value exceeds the declared character count only
   because of trailing spaces, silently stores the trimmed value;
5. if the trimmed value still exceeds the declared character count, reports
   MySQL-compatible data-too-long diagnostics;
6. binds the trimmed value as SQLite `TEXT`.

This slice intentionally stores the default visible `CHAR` value, not MySQL's
internal padded image. That keeps result readback and DML affected-row behavior
compatible for the admitted surface while `PAD_CHAR_TO_FULL_LENGTH`,
collation-sensitive comparisons, and string indexes remain unsupported.

`CHAR(0)` accepts only the empty string after trailing-space trimming and
`NULL` when nullable. A nonspace character in `CHAR(0)` is data-too-long.

`NULL` into a nullable `CHAR` stores SQL `NULL`. `NULL` into `CHAR NOT NULL`
fails with the existing bad-null diagnostic outside `IGNORE` and follows the
current string-family `INSERT IGNORE` adjustment inside supported `IGNORE`
paths.

### DML And Affected Rows

`INSERT`, `REPLACE`, and `UPDATE` reuse the existing descriptor-driven
single-table paths. Successful supported updates report MySQL's changed-row
affected count through the existing result API. Because `CHAR` assignment is
canonicalized before binding, reassigning a value that differs only in trailing
spaces from the stored canonical value reports zero changed rows.

### Predicates, Ordering, And Aggregates

This slice admits only `IS NULL` and `IS NOT NULL` predicates over `CHAR`
columns. Collation-sensitive comparison, `BETWEEN`, `IN`, truth tests,
ordering, distinct, grouping, and aggregate argument use are rejected
deterministically like current `VARCHAR` and `TEXT` family descriptors.

### Introspection

`SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` render `char(n)` in the `Type`
field, `YES`/`NO` nullability, `NULL` defaults for nullable no-explicit-default
columns, and existing key/extra metadata where applicable.

`SHOW CREATE TABLE` renders descriptor-owned `CHAR(n)` as lower-case
`char(n)`, nullable no-explicit-default columns as `DEFAULT NULL`, and
`NOT NULL` columns without a visible default unless future default support
adds one.

`INFORMATION_SCHEMA.COLUMNS` reports the current limited table rows with
`DATA_TYPE='char'`, `COLUMN_TYPE='char(n)'`, fixed charset/collation names,
and character/octet length values as described above.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors: `CHAR()`, signed lengths, expression lengths, omitted right
  parens, unsupported aliases and attributes;
- unsupported `CHAR` length above `255`;
- unsupported `CHARACTER`, `NCHAR`, `NATIONAL CHAR`, `CHARACTER SET`,
  `COLLATE`, `BINARY`, `ASCII`, `UNICODE`, and `CHAR BYTE` forms;
- explicit string defaults and expression defaults;
- unsupported non-string assignment values for `CHAR` targets;
- unsupported string literal forms, embedded `NUL`, invalid UTF-8, and values
  too large for local memory/build limits;
- data-too-long for nonspace-overlength `CHAR` values;
- `NULL` into `NOT NULL`;
- unknown schemas, tables, and columns through existing descriptor resolution;
- unsupported predicates, ordering, distinct/grouped/aggregate contexts;
- physical SQLite failures, allocation failures, and public API misuse through
  existing runtime/error paths.

Where MySQL-runtime-compatible diagnostics already exist for `VARCHAR` and
`TEXT`, `CHAR` should reuse them with type-specific wording only where that
improves determinism.

## Physical SQLite Handling

Generated physical tables remain ordinary MyLite user tables with stable names
such as `_mylite_user_table_<table_id>`. `CHAR` columns use quoted SQLite
identifiers, physical `TEXT` type text, prepared statements, and bound values.
No SQL literals are interpolated into generated DML. The feature does not add
indexes, constraints, triggers, generated columns, SQLite virtual tables, or
SQLite fork patches.

## Tests

Add a fast C runtime test, preferably `runtime_char_type`, plus parser coverage
and MySQL expectation probes.

Coverage must include:

- parser acceptance for `CHAR`, `CHAR(0)`, `CHAR(1)`, and `CHAR(255)`;
- parser/runtime rejection for `CHAR()`, signed lengths, `CHAR(256)`, aliases,
  and unsupported attributes;
- `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, `CREATE TABLE ... LIKE`,
  `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, `REPLACE ... SELECT`,
  `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS` over `CHAR` descriptors;
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and `UPDATE` string assignments;
- `CHAR(0)`, nullable `NULL`, `NULL` into `NOT NULL`, omitted no-default
  `NOT NULL`, and supported `INSERT IGNORE` adjustments;
- default-mode trailing-space trimming, silent excess-trailing-space
  truncation, data-too-long for nonspace overlength, and no-op update affected
  rows after canonicalization;
- `WHERE column IS NULL` and `WHERE column IS NOT NULL`;
- deterministic rejection of unsupported string comparisons, ordering,
  distinct, grouping, string default, and expression assignment forms, plus
  aggregate forms outside the string count-distinct slice;
- persistence across close/reopen, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handles;
- zero-initialized cleanup for new parser/planner/runtime structures;
- `cmake --build --preset dev`, focused CTest entries, the MySQL expectation
  script, and `cmake --workflow --preset check`.

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/type-system-literals-conversion.md`,
`docs/compatibility/sql-table-ddl.md`,
`docs/compatibility/sql-table-dml.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/sql-show-statements.md`,
`docs/compatibility/metadata-information-schema.md`,
`docs/compatibility/character-sets.md`, and
`docs/compatibility/collations.md` only for the implemented limited `CHAR`
subset. Do not claim support for full `CHARACTER` aliases, national character
sets, collations, string defaults, string comparison/order semantics,
`PAD_CHAR_TO_FULL_LENGTH`, string indexes, or full protocol metadata.
