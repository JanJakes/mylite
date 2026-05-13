# Baseline BIT Type

## Status

This feature specifies MyLite's first descriptor-owned `BIT` column slice for
persistent `.mylite` handles. It adds `BIT` / `BIT(n)` storage, byte-safe
readback, descriptor-backed DML conversion, descriptor-driven predicates and
ordering, and MySQL-shaped introspection for `n` in `1..64`.

The feature is intentionally not full MySQL bit-value expression support. It
does not implement table-backed `BIT` arithmetic, casts, functions over `BIT`
columns, bitwise operations over `BIT` columns, `BIT` indexes, generated
columns, full expression defaults, or protocol-grade metadata.

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
- Baseline row values, update, predicate, ordering, defaults, and binary-string
  specs:
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-where-between-predicates/specs.md`,
  `docs/specs/baseline-where-in-predicates/specs.md`,
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`,
  `docs/specs/baseline-dml-default-keyword-values/specs.md`, and
  `docs/specs/baseline-binary-string-types/specs.md`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md` and
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `BIT` type:
  https://dev.mysql.com/doc/refman/8.4/en/bit-type.html
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, bit-value literals:
  https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, data type defaults:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_bit_type_expectations.sh` records the
runtime probes for this feature. Observed behavior that shapes this slice:

- `BIT` without an explicit width is accepted and renders as `bit(1)`.
- `BIT(n)` accepts widths `1..64`. `BIT(0)` fails with `3013 / HY000`,
  `BIT(65)` fails with `1439 / 42000`, and `BIT()` is a syntax error.
- MySQL accepts noninteger display-width expressions such as `BIT(1.2)` and
  normalizes them to `bit(1)`. MyLite deliberately admits only integer width
  tokens in this baseline.
- `SHOW COLUMNS` and `SHOW CREATE TABLE` render lower-case `bit(n)`.
  Nullable `BIT` columns render `DEFAULT NULL`; `BIT NOT NULL` columns without
  explicit defaults render no visible default.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = bit`,
  `COLUMN_TYPE = bit(n)`, `NUMERIC_PRECISION = n`, `NUMERIC_SCALE = NULL`,
  `CHARACTER_MAXIMUM_LENGTH = NULL`, and `CHARACTER_OCTET_LENGTH = NULL`.
- Result values are exposed as fixed-width binary strings. The byte width is
  `ceil(n / 8)`. For example, `BIT(9)` reads back as two bytes and `BIT(64)`
  reads back as eight bytes when inspected through `HEX()` / `LENGTH()`.
- Assigning a shorter bit value pads on the left with zero bits. Stored bytes
  are the big-endian byte representation of the value, padded to the fixed byte
  width.
- Decimal integer, `TRUE` / `FALSE`, bit literal, hexadecimal literal, ordinary
  string literal, `NULL`, and `DEFAULT` values are accepted by MySQL in row DML
  positions when they fit the target `BIT(n)` descriptor.
- Ordinary string literals are interpreted as their binary string bytes in `BIT`
  assignment contexts. For example, `'A'` assigned to `BIT(8)` stores `0x41`.
- Negative values, values with more significant bits than the target width, and
  byte-string inputs whose value cannot fit fail in strict mode with data-too-
  long or out-of-range diagnostics matching the tested source category.
- `INSERT IGNORE` demotes over-width and `NULL`-into-`NOT NULL` `BIT` row
  failures to warnings. Over-width values clip to the target all-ones value and
  `NULL` / omitted no-default `NOT NULL` values store zero.
- `UPDATE` reports changed-row affected counts after `BIT` canonicalization.
  Reassigning the same stored value reports zero affected rows.
- Descriptor predicates compare `BIT` values by numeric bit value for the
  tested single-column comparison, null-safe equality, `BETWEEN`, `IN`,
  `IS NULL`, and `IS NOT NULL` forms.
- `ORDER BY bit_column ASC` places `NULL` values first and orders non-`NULL`
  values by numeric bit value. `DESC` reverses non-`NULL` values and places
  `NULL` values last. Ties without additional sort keys are not guaranteed by
  this slice.
- MySQL accepts explicit `BIT` defaults such as `DEFAULT b'101'`,
  `DEFAULT 5`, `DEFAULT TRUE`, and `DEFAULT FALSE` and renders nonexpression
  defaults as canonical bit literals. MyLite supports those literal defaults in
  this slice and defers `BIT` expression defaults.

## Scope

The implementation must add:

- parser and AST support for `BIT` and `BIT(integer_width)`;
- durable catalog descriptors with logical type text `BIT(n)` and physical
  type text `BLOB`;
- persistent base-table `CREATE TABLE` support for `BIT(1..64)` columns;
- append-only `ALTER TABLE ... ADD [COLUMN]` support for `BIT` columns;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` descriptor cloning/copying for compatible `BIT`
  descriptors;
- MySQL-shaped `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS` metadata;
- byte-safe public result readback for `BIT` values through existing result
  byte APIs;
- row DML conversion for `INSERT ... VALUES`, `INSERT ... SET`,
  `REPLACE ... VALUES`, `REPLACE ... SET`, and one-assignment `UPDATE`;
- admitted `BIT` values: `NULL`, `DEFAULT`, decimal integer literals with
  optional unary `+` / `-`, `TRUE`, `FALSE`, `b'...'`, `B'...'`, `0b...`,
  hexadecimal literals, and ordinary string literals;
- explicit `BIT` column defaults for `DEFAULT NULL`, `DEFAULT bit_literal`,
  `DEFAULT decimal_integer`, `DEFAULT TRUE`, and `DEFAULT FALSE`;
- limited `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` adjustment for
  `NULL` into `BIT NOT NULL`, omitted no-explicit-default `BIT NOT NULL`,
  explicit `DEFAULT` on no-default `BIT NOT NULL`, and over-width `BIT` values;
- descriptor-backed `SELECT`, `DELETE`, and `UPDATE` predicates using the
  existing comparison, null-safe equality, `BETWEEN`, `IN`, `IS NULL`, and
  `IS NOT NULL` predicate shapes, where `BIT` right operands use the admitted
  literal subset;
- descriptor-backed one-column `ORDER BY bit_column [ASC | DESC]` in existing
  `SELECT`, `DELETE`, `UPDATE`, `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, and `REPLACE ... SELECT` slices where one-column
  descriptor order keys are already admitted;
- duplicate-key validation only if later key support explicitly admits `BIT`
  key descriptors. This slice must not silently add `BIT` keys;
- persistence across close/reopen, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `BIT()` with empty parentheses, zero width, widths greater than `64`, signed
  widths, noninteger widths, parameters, or width expressions;
- `BIT` indexes, primary keys, unique keys, nonunique keys, prefix key parts,
  generated columns, invisible-column creation syntax, auto-increment, or
  constraints;
- `BIT` expression defaults such as `DEFAULT (1 + 2)`;
- `ALTER TABLE ... MODIFY` / `CHANGE` conversion to or from `BIT`;
- table-backed `BIT` arithmetic, bitwise operators, boolean truth tests,
  aggregate arguments, scalar function arguments, casts, expression projection,
  grouping, `DISTINCT`, joins, subqueries in predicates, or general expression
  conversion;
- `BIT` column ordering with expression keys, ordinal keys, table-qualified
  order keys beyond the already admitted table-alias slice, multiple sort keys
  beyond existing statement support, or deterministic tie selection;
- protocol-grade MySQL type codes, flags, charset metadata, or origin metadata;
- SQLite fork patches.

## Ownership Boundary

- Public API owns call validation, result object lifetime, public misuse
  behavior, and byte-safe result access. No public ABI changes are required.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported in-range `BIT` operations record
  `warning_count == 0`; supported `INSERT IGNORE` adjustments record warnings
  through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for `BIT` type nodes and bit literal
  token shape. They preserve source spans and structural payloads only; they do
  not resolve catalog descriptors, convert values, or generate SQLite SQL.
- Analyzer/planner code maps `BIT` AST nodes to durable descriptors, resolves
  schemas/tables/columns against the MyLite catalog, decodes admitted literals,
  validates width and assignment ranges, canonicalizes values to fixed-width
  bytes, and produces descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, defaults, column order, and table/index descriptors.
  SQLite schema text and SQLite runtime metadata are not compatibility
  authority.
- Result and introspection builders render logical descriptors to MySQL-shaped
  text. `BIT` values remain byte sequences in result cells.
- SQLite owns physical row storage, scans, ordering, predicate filtering, and
  mutations for generated prepared statements. MyLite binds `BIT` values with
  `sqlite3_bind_blob(..., SQLITE_TRANSIENT)` after conversion.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

The feature extends the existing limited column definition grammar:

```sql
column_definition:
    column_name column_type column_attributes

column_type:
    existing_type
  | BIT
  | BIT ( unsigned_decimal_integer_literal )

column_default:
    DEFAULT NULL
  | DEFAULT bit_literal
  | DEFAULT decimal_integer_literal
  | DEFAULT + decimal_integer_literal
  | DEFAULT - decimal_integer_literal
  | DEFAULT TRUE
  | DEFAULT FALSE
```

The equivalent MyLite Lemon-syntax extension is:

```lemon
column_type(A) ::= bit_type(T). {
    A = T;
}

bit_type(A) ::= BIT(T). {
    A = mylite_sql_parser_make_bit_type(
        state,
        (struct mylite_sql_bit_type_tokens){
            .type_token = T,
            .length_token = (struct mylite_sql_token){0},
            .end_token = T,
            .has_length = 0,
        });
}

bit_type(A) ::= BIT(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_bit_type(
        state,
        (struct mylite_sql_bit_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .has_length = 1,
        });
}

column_default_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
```

DML values for `BIT` targets reuse existing value syntax plus already-tokenized
literal kinds:

```sql
bit_dml_value:
    NULL
  | DEFAULT
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | bit_literal
  | hex_literal
  | string_literal
```

The lexer already recognizes `b'...'`, `B'...'`, and `0b...` as bit literals.
`BIT` is admitted as a type keyword while still remaining usable as an
identifier where the existing nonreserved-identifier grammar allows it.

## Descriptor Semantics

`BIT` without a width maps to `BIT(1)`. `BIT(n)` maps to logical type text
`BIT(n)` and physical type text `BLOB`.

Width validation:

| Input | MyLite behavior |
| --- | --- |
| omitted width | accepted as `BIT(1)` |
| `1..64` | accepted |
| `0` | error `3013 / HY000`, `Invalid size for column '<name>'.` |
| `65` or larger | error `1439 / 42000`, `Display width out of range for column '<name>' (max = 64)` |
| empty, signed, noninteger, or expression width | syntax or unsupported grammar error |

The physical byte width is `(n + 7) / 8`. Row-size validation counts that fixed
byte width for each `BIT` column.

## Physical SQLite Handling

MyLite stores every non-`NULL` `BIT(n)` value as a SQLite `BLOB` with exactly
`ceil(n / 8)` bytes. The bytes are big-endian and represent the unsigned
numeric bit value. Unused high bits in the first byte are zero.

This encoding keeps MyLite close to SQLite's optimal execution path:

- Equality, inequality, range predicates, `BETWEEN`, and `IN` compare bound
  same-width BLOB parameters against same-width BLOB column values. SQLite's
  bytewise BLOB comparison matches unsigned numeric ordering for fixed-width
  big-endian values.
- `ORDER BY` uses the physical BLOB column directly. SQLite's `NULL` ordering
  for `ASC` and `DESC` matches the MySQL 8.4.9 behavior verified for this
  slice.
- `UPDATE`, `DELETE`, `SELECT`, CTAS, and insert-select plans stay descriptor-
  generated SQLite statements with bound parameters. MyLite does not fetch all
  rows to filter or sort them for `BIT` semantics.

All generated SQLite identifiers must be quoted with existing identifier
quoting helpers. Generated physical table names stay stable descriptor names
such as `_mylite_user_table_<table_id>`. Literal values are always bound with
prepared-statement parameters; MyLite must not interpolate admitted bit,
string, hex, integer, or limit values into generated SQLite SQL.

No SQLite fork patch is required for this feature.

## Value Conversion

Conversion is MyLite-owned and occurs before binding. A non-`NULL` converted
value has:

- an unsigned magnitude in `0..2^n - 1`;
- a fixed output byte width `ceil(n / 8)`;
- big-endian bytes with left zero-padding.

Admitted source categories:

| Source | Conversion |
| --- | --- |
| decimal integer literal | parse as unsigned magnitude unless unary `-` is present |
| unary `+` integer | same as the integer literal |
| unary `-` integer | strict data-too-long error for nonzero values |
| `TRUE` / `FALSE` | `1` / `0` |
| `b'...'`, `B'...'`, `0b...` | parse binary digits as an unsigned magnitude; empty bit literal is zero |
| hexadecimal literal | decode bytes, interpret as big-endian unsigned magnitude |
| ordinary string literal | decode according to current string-literal SQL mode, then interpret bytes as a big-endian unsigned magnitude |
| `NULL` | store SQL `NULL` if nullable, else current bad-null behavior |
| `DEFAULT` | materialize the descriptor default, implicit nullable `NULL`, or current no-default behavior |

Strict assignment diagnostics:

- Negative nonzero values fail with `1406 / 22001`, `Data too long for column
  '<name>' at row <n>`.
- Values that fit unsigned 64-bit but exceed the target `BIT(n)` maximum fail
  with `1406 / 22001`.
- Decimal or bit-literal values beyond unsigned 64-bit fail with
  `1264 / 22003`, `Out of range value for column '<name>' at row <n>`.
- Hex and string byte inputs whose decoded magnitude exceeds the target maximum
  fail with `1406 / 22001` for the tested subset.
- `NULL` into `BIT NOT NULL` fails with `1048 / 23000`, `Column '<name>'
  cannot be null`.

`INSERT IGNORE` adjustment:

- `NULL` into `BIT NOT NULL` stores zero and records warning `1048`.
- Omitted or explicit `DEFAULT` for `BIT NOT NULL` with no explicit default
  stores zero and records warning `1364`.
- Over-width source values store the target all-ones value and record the
  MySQL-shaped warning(s) verified for the source category. Inputs that first
  overflow the unsigned-64 parser record an out-of-range warning before the
  data-too-long warning, matching the runtime probe for this slice.

## Defaults

Explicit nullable `DEFAULT NULL` remains represented by the existing
descriptor default semantics and renders as `DEFAULT NULL`.

This slice supports nonexpression `BIT` defaults from bit literals, decimal
integer literals with optional unary sign, `TRUE`, and `FALSE`. Successful
defaults are range-checked at DDL time and stored in the catalog as canonical
bit-literal text such as `b'101'`. `SHOW COLUMNS`,
`INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT`, and `SHOW CREATE TABLE` render the
same canonical bit-literal form.

MyLite defers `BIT` expression defaults, hexadecimal defaults, and string
defaults even though MySQL accepts wider forms. Unsupported default forms must
fail before catalog mutation and before physical SQLite SQL generation.

Omitted nullable `BIT` columns insert `NULL`. Omitted `BIT NOT NULL` columns
without explicit defaults fail with `1364 / HY000` outside `INSERT IGNORE` and
store zero with warning `1364` under `INSERT IGNORE`.

`ALTER TABLE ... ADD BIT NOT NULL` with no explicit default backfills existing
rows with zero, while catalog metadata remains no-explicit-default, matching
the current MyLite policy for implicit values.

## Predicate And Ordering Semantics

`BIT` predicate support uses the existing descriptor-backed predicate tree:

- comparisons: `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, `>=`;
- `BETWEEN lower AND upper`;
- `IN (value[, ...])` with admitted bit values and descriptor-preserving
  `NULL` list elements;
- `IS NULL` and `IS NOT NULL`.

Predicate right operands convert through the same `BIT` conversion rules as
DML values, except they use deterministic predicate diagnostics and never
mutate rows or warnings. Unsupported predicate expressions remain rejected by
the existing predicate analyzer.

Ordering support is limited to one descriptor `BIT` column in statement shapes
that already admit one descriptor order key. Default direction is `ASC`.
`ASC` places `NULL` first, then non-`NULL` values by unsigned bit value.
`DESC` places non-`NULL` values by descending unsigned bit value and `NULL`
last. Duplicate sort values without additional sort keys have unspecified tie
order; tests must not overclaim which tied row is selected by ordered limited
DML.

## Descriptor Copying

`CREATE TABLE ... LIKE` clones `BIT` descriptors, nullability, visibility,
defaults, and column order.

`CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and `REPLACE ... SELECT` copy
`BIT` rows only when source and target logical descriptors are compatible for
this slice. Compatible descriptor copies preserve the canonical fixed-width
BLOB values and do not reinterpret SQLite schema metadata. Incompatible
source/target combinations fail deterministically before mutation.

Single-table `UPDATE ... SET bit_col = (SELECT bit_col FROM source ...)`
reuses the existing scalar-subquery assignment envelope only when the source
and target `BIT` descriptors are compatible. It must validate that the scalar
SQLite value is a canonical BLOB of the target byte width before binding the
updated value.

## Result Reporting

Successful `BIT` DDL and DML return through existing public result conventions.
`SELECT` returns row result sets with byte-safe `BIT` cells. Successful DML
returns no row result set.

Affected-row behavior follows the existing MySQL-compatible statement policy:

- `INSERT`, `REPLACE`, CTAS, and insert-select report inserted/copied row
  counts already defined by their statement slices.
- `UPDATE` reports changed rows after `BIT` canonicalization. No-op assignment
  reports zero changed rows.
- `DELETE` reports deleted rows.
- Supported in-range statements report `warning_count == 0`.
- Supported `INSERT IGNORE` adjustments report warnings through existing
  diagnostics and statement warning count.

## Diagnostics

The implementation must cover these deterministic diagnostics:

| Case | Diagnostic |
| --- | --- |
| syntax errors and unsupported grammar | existing parse or unsupported diagnostics |
| missing default schema | existing `1046 / 3D000` no-database diagnostic |
| unknown schema | existing unknown-database diagnostic |
| unknown table | existing unknown-table diagnostic |
| reserved `_mylite_*` schema/table/column names | existing reserved-name diagnostic before SQLite SQL generation |
| unsupported object kind | existing unsupported object-kind diagnostic |
| unknown assignment, predicate, or order column | existing unknown-column diagnostic for the current statement context |
| `BIT(0)` | `3013 / HY000`, `Invalid size for column '<name>'.` |
| `BIT(65)` or wider | `1439 / 42000`, `Display width out of range for column '<name>' (max = 64)` |
| unsupported `BIT` default form | `1067 / 42000`, invalid default |
| out-of-range default | `1067 / 42000`, invalid default |
| strict over-width assignment | `1406 / 22001`, data too long |
| decimal or bit-literal magnitude beyond unsigned 64-bit | `1264 / 22003`, out of range |
| `NULL` into `BIT NOT NULL` | `1048 / 23000`, bad null |
| omitted no-default `BIT NOT NULL` | `1364 / HY000`, no default |
| unsupported assignment expression | existing unsupported assignment diagnostic |
| unsupported predicate/order expression | existing unsupported predicate/order diagnostic |
| physical SQLite failure | MyLite runtime diagnostic preserving SQLite failure detail where current policy does |
| allocation failure | `MYLITE_NOMEM` and handle-owned diagnostics |
| public API misuse | existing `MYLITE_MISUSE` behavior; no new public surface |

Warnings for `INSERT IGNORE` use the same MySQL error numbers and messages
stored as warnings for the tested `BIT` adjustments.

## Tests

Add a fast plain C runtime test, preferably
`packages/libmylite/tests/runtime_bit_type_test.c`, registered as a dotted
CTest name. It must cover:

- parser acceptance for `BIT` and `BIT(n)` plus rejection of empty, signed,
  zero, and too-large widths;
- `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, `CREATE TABLE ... LIKE`, and
  descriptor metadata for `BIT` columns;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and limited
  `INFORMATION_SCHEMA.COLUMNS` rendering;
- byte-safe row readback for `BIT(1)`, `BIT(6)`, `BIT(8)`, `BIT(9)`, and
  `BIT(64)`;
- `INSERT`, `INSERT ... SET`, `REPLACE`, `REPLACE ... SET`, and `UPDATE`
  assignment from decimal integer, signed integer, `TRUE` / `FALSE`, bit
  literal, hex literal, string literal, `NULL`, and `DEFAULT` where admitted;
- explicit literal `BIT` defaults and omitted/default DML materialization;
- `NULL` into nullable columns and deterministic `NULL` into `NOT NULL`
  diagnostics;
- strict range boundaries and out-of-range diagnostics;
- `INSERT IGNORE` clipping and warning counts for over-width and bad-null
  values;
- comparison, null-safe equality, `BETWEEN`, `IN`, `IS NULL`, and
  `IS NOT NULL` predicates in `SELECT`, `DELETE`, and `UPDATE`;
- `ORDER BY` `ASC`, `DESC`, `LIMIT`, nullable `BIT` columns, `NULL` ordering,
  and duplicate key ties without overclaiming tie order;
- changed-row affected counts and no result rows for successful `UPDATE`;
- compatible descriptor copying through CTAS, insert-select, replace-select,
  and scalar-subquery update assignment where supported;
- reopen persistence, table rename/drop behavior, independent file-backed
  handle isolation, and `.mylite` preamble preservation;
- zero-initialized cleanup for new AST/planner/result helpers;
- unchanged public API misuse behavior.

Keep the MySQL expectation script runnable against MySQL 8.4.9. The script
must verify result rows, errors, warnings, metadata, affected rows, and side
effects for every new user-visible behavior in this slice.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md` and
`docs/compatibility/type-system-literals-conversion.md` to mark the limited
`BIT` type as supported with documented gaps. Update
`docs/compatibility/sql-table-ddl.md`, `docs/compatibility/sql-table-dml.md`,
`docs/compatibility/sql-query-expressions.md`, and `docs/compatibility/operators.md`
only for exact surfaces this slice changes. Do not overclaim full `BIT`
expressions, indexes, arithmetic, casts, bitwise operators, expression
defaults, or protocol metadata.
