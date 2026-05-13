# Baseline YEAR Type

## Status

This feature specifies MyLite's first descriptor-owned `YEAR` column slice for
persistent `.mylite` handles. It adds bare `YEAR` and deprecated `YEAR(4)`
declarations, canonical four-character year storage/readback, descriptor-backed
DML conversion, descriptor-driven predicates and ordering, and MySQL-shaped
introspection for the normal `YEAR` range.

The feature is intentionally not full MySQL temporal conversion. It does not
implement relaxed temporal parsing, function-to-year conversion, `YEAR`
expression defaults, `YEAR` primary keys, generated columns, mutable SQL-mode
conversion behavior, or protocol-grade metadata.

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
- Existing temporal type, row-value, update, predicate, ordering, defaults,
  index, `SHOW`, and `INFORMATION_SCHEMA` specs under `docs/specs/`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md` and
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `YEAR` type:
  https://dev.mysql.com/doc/refman/8.4/en/year.html
- MySQL 8.4 Reference Manual, date and time data types:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-types.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_year_type_expectations.sh` records the
runtime probes for this feature. Observed behavior that shapes this slice:

- `YEAR` is accepted and renders as `year` in `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.
- `YEAR(4)` is accepted as equivalent to `YEAR`, renders as `year`, and emits
  warning `1287` that explicit display width is deprecated.
- Other explicit display widths such as `YEAR(0)`, `YEAR(2)`, and `YEAR(5)`
  fail with `1818 / HY000`, `Invalid display width. Use YEAR instead.`
- `YEAR UNSIGNED`, `YEAR SIGNED`, and `YEAR ZEROFILL` are accepted by MySQL
  with deprecation warnings where applicable. MyLite defers those attributes in
  this slice.
- `SHOW COLUMNS` reports nullable `YEAR` columns with `Default = NULL`, not
  null no-default columns with no rendered default, and explicit defaults as
  unquoted canonical year text.
- `SHOW CREATE TABLE` renders explicit `YEAR` defaults as quoted canonical year
  text such as `DEFAULT '1970'` or `DEFAULT '0000'`.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = year`,
  `COLUMN_TYPE = year`, `NUMERIC_PRECISION = NULL`, `NUMERIC_SCALE = NULL`,
  and `DATETIME_PRECISION = NULL`.
- Stored `YEAR` result text is four characters. The valid represented values
  for this slice are `0000` and `1901..2155`.
- Numeric assignment conversion differs from string assignment conversion:
  numeric `0` stores `0000`, numeric `1..69` store `2001..2069`, numeric
  `70..99` store `1970..1999`, and numeric `1901..2155` store themselves.
  A decoded string `'0'` or `'00'` stores `2000`; decoded strings `'1'..'69'`
  and `'70'..'99'` use the same two-digit year ranges; four-digit strings
  `1901..2155` and `0000` store themselves.
- `TRUE` stores `2001`; `FALSE` stores `0000`.
- Strict invalid values fail before mutation. Numeric out-of-range values such
  as `1900`, `2156`, or `-1` fail with `1264 / 22003`. Non-numeric strings
  fail with `1366 / HY000`.
- `INSERT IGNORE` demotes invalid `YEAR` inputs to warnings and stores `0000`.
  Explicit `NULL` into `YEAR NOT NULL` also demotes to a warning and stores
  `0000`.
- Omitted or explicit `DEFAULT` for `YEAR NOT NULL` with no explicit default
  fails with `1364 / HY000`; `INSERT IGNORE` demotes it and stores `0000`.
- `YEAR DEFAULT 70`, `DEFAULT '70'`, `DEFAULT 0`, `DEFAULT '0'`,
  `DEFAULT TRUE`, and `DEFAULT FALSE` are accepted and render as canonical
  `1970`, `1970`, `0000`, `2000`, `2001`, and `0000` defaults.
- `ALTER TABLE ... ADD y YEAR NOT NULL` on a non-empty table backfills existing
  rows with `0000` and records no warnings; adding a nullable `YEAR` backfills
  `NULL`.
- `ALTER TABLE ... ALTER [COLUMN] y SET DEFAULT value` accepts the same
  non-expression `YEAR` default literal subset as column definitions and updates
  future omitted-column materialization without changing existing rows.
- MySQL accepts nonunique and unique secondary indexes on `YEAR`; index
  metadata renders `year` descriptors while comparison and ordering use the
  represented year value.
- MySQL accepts parenthesized expression defaults such as
  `DEFAULT (2000 + 1)` for `YEAR`. MyLite defers `YEAR` expression defaults in
  this slice.
- Single-table `UPDATE` reports changed-row affected counts after year
  canonicalization. Reassigning an already-stored canonical value reports zero
  changed rows.
- Descriptor predicates compare `YEAR` values by their represented numeric
  value for the tested comparison, null-safe equality, `BETWEEN`, `IS NULL`,
  and `IS NOT NULL` forms. `IN` uses additional MySQL list-coercion rules:
  one-element lists behave like equality, multi-element all-string lists use
  `YEAR` string conversion for each item, and multi-element lists containing a
  non-string item compare represented year values directly rather than applying
  two-digit `YEAR` conversion to every item.
- `ORDER BY year_column ASC` places `NULL` values first and orders non-`NULL`
  values by represented year. `DESC` reverses non-`NULL` values and places
  `NULL` values last. Ties without additional sort keys are not guaranteed by
  this slice.

## Scope

The implementation must add:

- parser and AST support for `YEAR` and `YEAR(4)`;
- deterministic runtime rejection for other parsed integer display widths;
- durable catalog descriptors with logical type text `YEAR` and physical type
  text `TEXT`;
- persistent base-table `CREATE TABLE` support for `YEAR` columns;
- append-only `ALTER TABLE ... ADD [COLUMN]` support for `YEAR` columns;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` descriptor cloning/copying for compatible `YEAR`
  descriptors;
- MySQL-shaped `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS` metadata;
- row DML conversion for `INSERT ... VALUES`, `INSERT ... SET`,
  `REPLACE ... VALUES`, `REPLACE ... SET`, and one-assignment `UPDATE`;
- admitted `YEAR` values: `NULL`, `DEFAULT`, decimal integer literals with
  optional unary `+` / `-`, `TRUE`, `FALSE`, and decoded ordinary string
  literals in the supported documented shapes;
- explicit `YEAR` column defaults for `DEFAULT NULL`, `DEFAULT decimal_integer`,
  `DEFAULT string_literal`, `DEFAULT TRUE`, and `DEFAULT FALSE`;
- limited `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` adjustment for
  invalid `YEAR` values, `NULL` into `YEAR NOT NULL`, omitted
  no-explicit-default `YEAR NOT NULL`, and explicit `DEFAULT` on no-default
  `YEAR NOT NULL`;
- descriptor-backed `SELECT`, `DELETE`, and `UPDATE` predicates using the
  existing comparison, null-safe equality, `BETWEEN`, `IN`, `IS NULL`, and
  `IS NOT NULL` predicate shapes, where `YEAR` right operands use the admitted
  integer, boolean, string, or `NULL` subset and `YEAR` `IN` lists follow the
  separately specified list-coercion subset;
- descriptor-backed one-column `ORDER BY year_column [ASC | DESC]` in existing
  `SELECT`, `DELETE`, `UPDATE`, `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, and `REPLACE ... SELECT` slices where one-column
  descriptor order keys are already admitted;
- supported nonunique and unique secondary indexes on `YEAR` descriptors where
  the current date/time secondary-index subset already admits similar scalar
  temporal descriptors;
- persistence across close/reopen, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `YEAR()` with empty parentheses, display widths other than `4`, signed
  display-width tokens, noninteger display widths, parameters, or display-width
  expressions;
- `YEAR UNSIGNED`, `YEAR SIGNED`, `YEAR ZEROFILL`, repeated attributes, or
  combined numeric attributes;
- `YEAR` expression defaults such as `DEFAULT (2000 + 1)`;
- function-to-year conversion such as assigning `NOW()` to a `YEAR` column;
- three-digit, five-digit, whitespace-padded, relaxed, or otherwise broader
  string-to-year conversions beyond the documented 1-, 2-, and 4-digit shapes
  admitted by this slice;
- `ALTER TABLE ... MODIFY` / `CHANGE` conversion to or from `YEAR`;
- `YEAR` primary keys, prefix key parts, fulltext/spatial/functional indexes,
  generated columns, invisible-column creation syntax, auto-increment, or
  constraints;
- table-backed year arithmetic, casts, scalar function arguments, expression
  projection, grouping, `DISTINCT`, joins, subqueries in predicates, or general
  expression conversion;
- `YEAR` column ordering with expression keys, ordinal keys, table-qualified
  order keys beyond the already admitted table-alias slice, multiple sort keys
  beyond existing statement support, or deterministic tie selection;
- mutable `ALLOW_INVALID_DATES`, `NO_ZERO_DATE`, `NO_ZERO_IN_DATE`, or
  strict/non-strict SQL-mode behavior beyond the current default-mode and
  `INSERT IGNORE` surfaces;
- protocol-grade MySQL type codes, flags, charset metadata, or origin metadata;
- SQLite fork patches.

## Ownership Boundary

- Public API owns call validation, result object lifetime, public misuse
  behavior, and result access. No public ABI changes are required.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  prior diagnostics snapshots, and transaction completion. Supported in-range
  `YEAR` operations record `warning_count == 0`; supported `INSERT IGNORE`
  adjustments record warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for `YEAR` type nodes and source spans.
  They do not resolve catalog descriptors, convert values, or generate SQLite
  SQL.
- Analyzer/planner code maps `YEAR` AST nodes to durable descriptors, resolves
  schemas/tables/columns against the MyLite catalog, decodes admitted literals,
  validates assignment ranges, canonicalizes values to four-character text, and
  produces descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, defaults, column order, and table/index descriptors.
  SQLite schema text and SQLite runtime metadata are not compatibility
  authority.
- Result and introspection builders render logical descriptors and canonical
  default text to MySQL-shaped output.
- SQLite owns physical row storage, scans, ordering, predicate filtering, and
  mutations for generated prepared statements. MyLite binds `YEAR` values as
  canonical `TEXT` after conversion.
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
  | YEAR
  | YEAR ( unsigned_decimal_integer_literal )

column_default:
    DEFAULT NULL
  | DEFAULT decimal_integer_literal
  | DEFAULT + decimal_integer_literal
  | DEFAULT - decimal_integer_literal
  | DEFAULT TRUE
  | DEFAULT FALSE
  | DEFAULT string_literal

insert_value:
    existing_insert_value
  | string_literal

update_value:
    existing_update_value
  | string_literal

predicate_value:
    existing_predicate_value
  | string_literal
```

MyLite Lemon-syntax snippets:

```lemon
identifier(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

column_type(A) ::= year_type(T). {
    A = T;
}

year_type(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_year_type(
        state,
        (struct mylite_sql_year_type_tokens){
            .type_token = T,
            .end_token = T,
            .has_width = 0,
        });
}

year_type(A) ::= YEAR(T) LPAREN INTEGER(W) RPAREN(R). {
    A = mylite_sql_parser_make_year_type(
        state,
        (struct mylite_sql_year_type_tokens){
            .type_token = T,
            .width_token = W,
            .end_token = R,
            .has_width = 1,
        });
}
```

Existing `STRING`, integer, boolean, `NULL`, and `DEFAULT` value productions are
reused for row values and defaults. Runtime conversion admits those productions
only when the target descriptor is `YEAR`.

## Descriptor Mapping And Storage

`YEAR` and `YEAR(4)` map to logical type text `YEAR` and physical SQLite type
text `TEXT`. Explicit `YEAR(4)` emits warning `1287` at DDL execution time, but
the durable descriptor stores the normalized logical type `YEAR`.

MyLite stores every non-`NULL` `YEAR` value as four ASCII digits:

- `0000` for the zero year;
- `1901` through `2155` for normal represented years.

The MySQL row-size envelope counts a `YEAR` descriptor as one byte, matching the
documented MySQL storage requirement. Physical SQLite storage is canonical text
to keep descriptor-driven comparisons and ordering simple and portable.

## Conversion Semantics

For strict non-`IGNORE` DML and defaults:

- `NULL` stores `NULL` for nullable `YEAR`; `NULL` into `YEAR NOT NULL` fails
  with `1048 / 23000`.
- `DEFAULT` materializes the descriptor default. If a `YEAR NOT NULL` column
  has no explicit default, omitted/default DML fails with `1364 / HY000`.
- Decimal integer literals with optional unary `+` convert as:
  - `0` -> `0000`;
  - `1..69` -> `2001..2069`;
  - `70..99` -> `1970..1999`;
  - `1901..2155` -> same value;
  - anything else -> `1264 / 22003`.
- Decimal integer literals with unary `-` fail with `1264 / 22003` unless the
  magnitude is zero.
- `TRUE` converts like numeric `1`, producing `2001`.
- `FALSE` converts like numeric `0`, producing `0000`.
- Decoded ordinary strings are admitted only for the documented 1-, 2-, and
  4-digit shapes:
  - `'0'` and `'00'` -> `2000`;
  - `'1'..'69'` and `'01'..'69'` -> `2001..2069`;
  - `'70'..'99'` -> `1970..1999`;
  - `'0000'` -> `0000`;
  - `'1901'..'2155'` -> same value;
  - other all-digit strings -> `1264 / 22003`;
  - non-digit strings -> `1366 / HY000`.

`INSERT IGNORE` demotes supported conversion failures to warnings and stores
`0000`, including invalid numeric/string values, explicit `NULL` into
`YEAR NOT NULL`, and no-default `YEAR NOT NULL` omitted/default values.

## Defaults

Supported `CREATE TABLE` and `ALTER TABLE ... ADD COLUMN` defaults are
`DEFAULT NULL`, decimal integer literals with optional unary sign, `TRUE`,
`FALSE`, and decoded ordinary strings within the admitted conversion subset.
The durable default text is canonical four-character year text.

When `ALTER TABLE ... ADD COLUMN` adds `YEAR NOT NULL` without an explicit
default to an existing table, MyLite backfills stored rows with `0000` while
leaving the descriptor without an explicit default. Later omitted/default DML
for that column still follows the ordinary no-default `NOT NULL` diagnostics.

`SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` render canonical
year text without quotes. `SHOW CREATE TABLE` renders explicit defaults as
quoted canonical year strings.

`ALTER TABLE ... ALTER [COLUMN] SET DEFAULT` for `YEAR` uses the same
non-expression conversion rules as column definitions. It is catalog-only:
existing rows and SQLite physical schema are unchanged, while future omitted
values materialize from the updated descriptor default. `DROP DEFAULT` keeps the
existing descriptor behavior shared by all column families.

## Predicates And Ordering

`YEAR` predicate support uses the existing descriptor-backed predicate tree:
`=`, `<>`, `!=`, `<`, `<=`, `>`, `>=`, `<=>`, `BETWEEN`, `IN`, `IS NULL`, and
`IS NOT NULL` in statement shapes that already admit those predicates.
Comparison and `BETWEEN` right operands convert to the same canonical
four-character text as DML row values.

`YEAR` `IN` lists are intentionally list-shape aware because MySQL 8.4.9 does
not apply the same two-digit conversion to every multi-value list:

- one-element `IN` lists convert the single item like equality;
- multi-element lists whose non-`NULL` values are all decoded ordinary string
  literals convert each string through the supported string-to-year rules;
- multi-element lists containing any integer, signed integer, boolean, or other
  non-string admitted item compare represented year values directly. In this
  mixed/numeric form, numeric `0` and string/numeric `1901..2155` can match
  stored values, but numeric/string `1..99` do not perform two-digit `YEAR`
  conversion.

`NULL` in `IN` lists remains `NULL` and does not match non-`NULL` `YEAR`
values.

Canonical year strings sort in the same order as represented year values, so
SQLite text ordering is sufficient for the admitted physical representation.
Ascending order places `NULL` before non-`NULL`; descending order places `NULL`
after non-`NULL`, matching the existing baseline order-key behavior.

## Indexes

This slice admits `YEAR` in the existing supported unique and nonunique
secondary-index subsets. Generated SQLite indexes are built over the canonical
text column, while descriptor metadata remains the MySQL compatibility
authority. Primary-key use remains deferred because the current primary-key
slice intentionally admits only integer-family and limited ASCII string keys.

## Result And Metadata Behavior

Successful `YEAR` DDL and DML return through existing public result
conventions. `SELECT` returns row result sets with text cells. Successful DML
reports changed/inserted/deleted affected rows through existing result fields.
Supported in-range statements report `warning_count == 0`; admitted
`INSERT IGNORE` adjustments report warning counts.

`SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` render:

- `Type = year`;
- `Null` from descriptor nullability;
- `Default = NULL`, canonical year text, or empty no-default value;
- existing `Key` and `Extra` values from descriptor indexes and attributes.

`INFORMATION_SCHEMA.COLUMNS` renders:

- `DATA_TYPE = year`;
- `COLUMN_TYPE = year`;
- character length/octet length `NULL`;
- numeric precision/scale `NULL`;
- datetime precision `NULL`;
- canonical default text or `NULL`.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| `YEAR(0)`, `YEAR(2)`, `YEAR(5)` | `1818 / HY000`, invalid display width |
| unsupported `YEAR` type attribute | deterministic unsupported syntax diagnostic |
| unsupported `YEAR` expression default | deterministic invalid-default or unsupported diagnostic |
| invalid `YEAR` default | `1067 / 42000`, invalid default |
| numeric year out of range | `1264 / 22003`, out-of-range value |
| non-numeric string year | `1366 / HY000`, incorrect integer value |
| `NULL` into `YEAR NOT NULL` | `1048 / 23000`, bad null |
| omitted/default `YEAR NOT NULL` with no explicit default | `1364 / HY000`, no default |
| unknown schema/table/column | existing descriptor-driven MySQL-shaped diagnostics |
| physical SQLite failure | existing physical row/schema error path |
| allocation failure | `MYLITE_NOMEM` with handle diagnostics |

`INSERT IGNORE` stores warnings for the tested value-adjustment cases.

## Tests

Implementation tests must cover:

- parser acceptance for `YEAR`, `YEAR(4)`, and `YEAR` as an identifier;
- parser/runtime rejection for empty, noninteger, signed, and unsupported
  display-width forms;
- `YEAR(4)` warning and normalized descriptor rendering;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `DESCRIBE`, `EXPLAIN table`, and
  `INFORMATION_SCHEMA.COLUMNS` metadata;
- row readback for `0000`, `1901`, `1970`, `1999`, `2000`, `2001`, `2069`,
  and `2155`;
- numeric, boolean, string, `NULL`, and `DEFAULT` insert/update/replace values;
- strict invalid numeric and string diagnostics;
- `INSERT IGNORE` warning demotion and adjusted `0000` storage;
- explicit defaults and omitted/default materialization;
- nullability diagnostics and no-default `NOT NULL` behavior;
- comparison, null-safe comparison, `BETWEEN`, `IN`, `IS NULL`,
  `IS NOT NULL`, and one-column ordering with `NULL` placement;
- supported unique and nonunique secondary indexes;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, `INSERT ... SELECT`,
  `REPLACE ... SELECT`, table rename/drop, reopen persistence, independent
  handles, and `.mylite` preamble preservation;
- deterministic rejection for deferred `YEAR` attributes, expression defaults,
  primary keys, `ALTER MODIFY` / `CHANGE`, function assignments, casts,
  relaxed/undocumented string conversions, and general expressions;
- existing lexer, parser, runtime lifecycle, type, DML, introspection, catalog,
  file-backed opening, VFS, and SQLite bootstrap tests still pass.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md`,
`docs/compatibility/type-system-literals-conversion.md`,
`docs/compatibility/sql-table-ddl.md`,
`docs/compatibility/sql-table-dml.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-show-statements.md`,
`docs/compatibility/metadata-information-schema.md`, and
`docs/compatibility/sql-indexes-constraints.md` only for the exact supported
subset. Do not overclaim full temporal conversion, expression defaults,
primary-key support, SQL-mode behavior, or protocol-grade metadata.
