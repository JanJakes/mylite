# Result metadata and expression labels

## Scope

Task 23 defines MyLite's MySQL-compatible result-column metadata contract for
currently supported `SELECT` output. It fills the gap left by Tasks 15, 16, 18,
and the column-type tasks: projected expressions and table columns must expose
the labels, origin information, type descriptors, lengths, flags, decimals,
collations, and nullability that MySQL clients expect.

In scope:

- result metadata for scalar `SELECT` expressions without `FROM`
- result metadata for one-table `SELECT` projections
- direct table columns, wildcards, and aliases already supported by Task 15
- supported Task 16 scalar expression operators in projected output
- alias labels, generated expression labels, duplicate output labels, and
  empty labels where MySQL permits them
- `MYSQL_FIELD`-style name, origin, type, length, flags, decimals, and
  collation semantics
- nullability inference for direct columns, literals, and supported operators
- metadata preservation through `WHERE`, `ORDER BY`, `LIMIT`, and `OFFSET`
- hidden `ORDER BY` expressions that affect sorting but not result metadata
- implementation and test expectations for the current public C API and future
  MySQL protocol column-definition packets

Out of scope:

- joins, derived tables, views, `UNION`, CTEs, subqueries, and set operations
- grouping, aggregate output metadata, `HAVING`, windows, and rollups
- built-in scalar functions beyond existing operator/literal support
- prepared-statement binary-protocol metadata and parameter metadata
- `SELECT ... INTO`, cursor metadata, and stored-program result metadata
- privilege-sensitive metadata filtering
- optional result-set metadata suppression through MySQL's
  `resultset_metadata` system variable
- exhaustive charset/collation coercion for expressions beyond the current
  charset foundation
- exact `max_length` population for stored result sets; MyLite may initially
  expose static declared/display lengths only

Follow-up result-set producers outside the original Task 23 `SELECT` scope must
attach descriptors through the same statement-owned metadata model instead of
falling back to SQLite column metadata. Current covered non-`SELECT` surfaces
include `SHOW DATABASES`, `SHOW TABLES`, `SHOW TABLE STATUS`, and
`CHECK` / `OPTIMIZE` / `REPAIR TABLE`. The static `SHOW` surfaces covered by
the same model are `SHOW VARIABLES`, `SHOW STATUS`, `SHOW CHARACTER SET`,
`SHOW COLLATION`, `SHOW WARNINGS`, `SHOW ERRORS`, `SHOW COUNT(*) WARNINGS`,
and `SHOW COUNT(*) ERRORS`. Target-table SHOW surfaces covered by the same
model are `SHOW COLUMNS` / `SHOW FIELDS`, `DESCRIBE` / `DESC` table metadata,
`SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, `SHOW CREATE DATABASE` /
`SHOW CREATE SCHEMA`, and `SHOW CREATE TABLE`.

This task should not change SQL grammar meaning except where the parser already
accepts projection expressions that runtime has not yet executed. It turns
accepted supported projections into fully described result fields.

## Sources

- MySQL 8.4 C API Developer Guide, C API Basic Data Structures:
  https://dev.mysql.com/doc/c-api/8.4/en/c-api-data-structures.html
- MySQL 8.4 C API Developer Guide, `mysql_fetch_field()`:
  https://dev.mysql.com/doc/c-api/8.4/en/mysql-fetch-field.html
- MySQL 8.4 C API Developer Guide, Optional Result Set Metadata:
  https://dev.mysql.com/doc/c-api/8.4/en/c-api-optional-metadata.html
- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/integer-boolean-column-types/specs.md`
  - `docs/specs/string-binary-column-types/specs.md`
  - `docs/specs/numeric-column-types/specs.md`
  - `docs/specs/temporal-column-types/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using:
  - `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv`
  - a focused C API `MYSQL_FIELD` probe built against local `libmysqlclient`
    and connected to the container at `192.168.215.2:3306`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, protocol code, or implementation sources.

## MySQL 8.4.9 behavior summary

### Metadata surface

For result sets, MySQL exposes one field descriptor per output column. The C
API descriptor is the best compatibility model for MyLite because it maps to
both client-library metadata and wire-protocol column-definition packets.

Task 23 must model these fields for each result column:

| Field | MyLite meaning |
| --- | --- |
| `name` | visible output label; aliases replace the expression's default label |
| `org_name` | original base column name, or empty for expressions |
| `table` | visible table name or table alias for base columns, or empty for expressions |
| `org_table` | base table name for base columns, or empty for expressions |
| `db` | origin schema for base columns, or empty for expressions |
| `catalog` | always `def` for MySQL protocol compatibility |
| `length` | display/storage width in bytes for the result type under the effective result charset |
| `max_length` | largest actual row value after materialization; may be deferred |
| `type` | `MYSQL_TYPE_*` field type |
| `flags` | `NOT_NULL`, key, unsigned, binary, numeric, blob, auto-increment, and related bits |
| `decimals` | numeric scale or temporal fractional seconds precision |
| `charsetnr` | collation id; `63` represents binary strings and non-character numeric/temporal values |

The current public MyLite metadata accessors expose only a subset
(`name`, schema, table, origin table, origin column). Task 23 should expand the
internal descriptor first and then expose ABI-safe accessors for type, length,
flags, decimals, charset/collation, and nullability.

### Labels and origin metadata

Observed behavior:

| Query shape | Expected field label and origin behavior |
| --- | --- |
| `SELECT id FROM t` | `name='id'`, `org_name='id'`, table/origin table both `t` |
| `SELECT id AS label FROM t` | `name='label'`, `org_name='id'`; alias does not change origin |
| `SELECT (id) AS label FROM t AS tt` | parenthesized direct references keep base-column metadata, with `table='tt'`, `org_table='t'`, and `org_name='id'` |
| `SELECT +((n)) AS label FROM t AS tt` | unary positive references through parentheses keep the referenced column descriptor and origin metadata |
| `SELECT id AS label, n AS label FROM t` | both output labels are exactly `label`; duplicates are allowed |
| `SELECT n + 1 AS expr FROM t` | `name='expr'`, empty `org_name`, `db`, `table`, and `org_table` |
| `SELECT n + 1 FROM t` | default label is the expression text as normalized by MySQL formatting |
| `SELECT 'abc'` | default label is the literal string value `abc`, without SQL quotes |
| `SELECT s AS s_alias FROM t AS tt` | `table='tt'`, `org_table='t'`, `org_name='s'` |
| `SELECT 1 AS one` | empty origin metadata and `name='one'` |
| `SELECT NULL AS nil` | empty origin metadata and `name='nil'` |

The focused C API probe returned:

```text
SQL: SELECT id AS label, n AS label, s AS s_alias, n + 1 AS expr FROM t AS tt LIMIT 0
1 name='label' org_name='id' db='mylite_task23_capi' table='tt' org_table='t' type=3 length=11 decimals=0 flags=49667 charsetnr=63
2 name='label' org_name='n' db='mylite_task23_capi' table='tt' org_table='t' type=3 length=11 decimals=0 flags=49160 charsetnr=63
3 name='s_alias' org_name='s' db='mylite_task23_capi' table='tt' org_table='t' type=253 length=48 decimals=0 flags=0 charsetnr=255
4 name='expr' org_name='' db='' table='' org_table='' type=8 length=12 decimals=0 flags=32896 charsetnr=63
```

The numeric `type` and `flags` values are implementation constants from
`mysql.h`; MyLite tests should assert symbolic meanings rather than raw numeric
values unless the public ABI deliberately mirrors MySQL constants.

### Duplicate labels and ambiguity

MySQL permits duplicate result-column labels. Metadata preserves duplicates
without suffixes or disambiguation:

```sql
SELECT n AS id, id FROM meta_t LIMIT 0;
SELECT id AS first, n AS first FROM meta_t LIMIT 0;
```

Both queries produce duplicate visible labels. MyLite must not make labels
unique for client convenience.

Duplicate labels can affect later name resolution. MySQL resolves unqualified
`ORDER BY` references by checking output labels before base table columns. A
duplicate projected label is ambiguous:

```sql
SELECT n AS id, id FROM meta_t ORDER BY id LIMIT 0;
```

This fails with 1052 / SQLSTATE `23000`, `Column 'id' in order clause is
ambiguous`. Qualifying the base column removes the ambiguity:

```sql
SELECT n AS id, id FROM meta_t ORDER BY meta_t.id LIMIT 0;
```

Task 23 must preserve Task 18's binding behavior. Metadata work must not
retrofit duplicate-label disambiguation that would hide the ambiguity from
`ORDER BY`, grouping, or future `HAVING` binding.

### Hidden `ORDER BY` expressions

Hidden sort keys do not become result columns and do not alter output
metadata:

```sql
SELECT id FROM meta_t ORDER BY n + 1 LIMIT 0;
```

MySQL reports only the `id` field. Task 23 must keep sort-key descriptors
separate from output descriptors. Hidden expressions may need internal type
metadata for comparison and warning behavior, but they are not client-visible.

### Nullability and `NOT_NULL`

MySQL represents non-nullability primarily through `NOT_NULL_FLAG`. Observed
patterns for current MyLite scope:

| Expression or column | Nullability expectation |
| --- | --- |
| `INT NOT NULL` base column | `NOT_NULL` set |
| nullable base column | `NOT_NULL` clear |
| `AUTO_INCREMENT PRIMARY KEY` | `NOT_NULL`, `PRI_KEY`, `AUTO_INCREMENT` set |
| integer literal `1` | `NOT_NULL` set |
| string literal `'abc'` | `NOT_NULL` set |
| binary string literal `_binary 'abc'` | `NOT_NULL` and `BINARY` set |
| `NULL` literal | nullable; `NOT_NULL` clear |
| arithmetic using a nullable operand, such as `n + 1` | nullable; `NOT_NULL` clear |
| arithmetic over non-null literals, such as `1 + 2` | `NOT_NULL` set |
| comparisons that cannot return `NULL`, such as `1 = 1`, `n IS NULL`, `n <=> NULL` | `NOT_NULL` set |
| comparisons that can return `NULL`, such as `n IN (1,2)` or `n BETWEEN 1 AND 20` | `NOT_NULL` clear when operands can be `NULL` |
| `s LIKE 'a%'` with nullable `s` | `NOT_NULL` clear |

Implementation should infer nullability from expression semantics, not from
runtime values in the first row.

### Type, length, decimals, flags, and charset

The following expectations are verified against MySQL 8.4.9 with
`mysql --column-type-info -vvv` unless noted. Character lengths depend on the
connection `character_set_results`; tests must set connection charset
explicitly when asserting byte lengths.

#### Base columns currently in MyLite scope

| MySQL column definition | Type | Length and decimals | Flags |
| --- | --- | --- | --- |
| `INT` nullable | `LONG` | length `11`, decimals `0` | `NUM`; `MULTIPLE_KEY` if indexed |
| `INT UNSIGNED NOT NULL` | `LONG` | length `10`, decimals `0` | `NOT_NULL UNSIGNED NUM`; key/no-default flags as applicable |
| `INT NOT NULL AUTO_INCREMENT PRIMARY KEY` | `LONG` | length `11`, decimals `0` | `NOT_NULL PRI_KEY AUTO_INCREMENT NUM` |
| `VARCHAR(12)` with utf8mb4 results | `VAR_STRING` | length `48`, decimals `0` | usually no flags unless binary/key/nullable attributes apply |
| `CHAR(3) NOT NULL` | `STRING` | charset-dependent byte length, decimals `0` | `NOT_NULL`; `NO_DEFAULT_VALUE` when no explicit/default nullable rule supplies a default |
| `TEXT` | `BLOB` | length `65535`, decimals `0` | `BLOB` |
| `VARBINARY(8)` | `VAR_STRING` | length `8`, decimals `0` | `BINARY` |
| `DECIMAL(6,2)` | `NEWDECIMAL` | length `8`, decimals `2` | `NUM` |
| `DOUBLE` | `DOUBLE` | length `22`, decimals `31` | `NUM` |
| `DATETIME(3)` | `DATETIME` | length `23`, decimals `3` | `BINARY` |
| `TIMESTAMP` | `TIMESTAMP` | length `19`, decimals `0` | `BINARY` |

Task 23 should map all column descriptors already accepted by the column-type
tasks. Unsupported column families should stay deferred rather than guessed.

#### Literals and supported expressions

| Expression | Type | Length and decimals | Flags |
| --- | --- | --- | --- |
| `1 AS one` | `LONGLONG` | length `2`, decimals `0` | `NOT_NULL BINARY NUM` |
| `NULL AS nil` | `NULL` | length `0`, decimals `0` | `BINARY NUM`; `NOT_NULL` clear |
| `'abc' AS str_lit` under utf8mb4 results | `VAR_STRING` | length `12`, decimals `31` | `NOT_NULL` |
| `_binary 'abc' AS bin_lit` | `VAR_STRING` | length `3`, decimals `31` | `NOT_NULL BINARY` |
| `BINARY 'abc' AS binary_expr` under utf8mb4 results | `VAR_STRING` | length `12`, decimals `31` | `BINARY`; nullable |
| `BINARY c` for `c CHAR(3) NOT NULL` under utf8mb4 results | `VAR_STRING` | length `12`, decimals `31` | `BINARY`; nullable |
| `BINARY b` for `b VARBINARY(8)` | `VAR_STRING` | length `8`, decimals `31` | `BINARY`; nullable |
| `1 + 2 AS sum_expr` | `LONGLONG` | length `3`, decimals `0` | `NOT_NULL BINARY NUM` |
| `n + 1 AS n_plus` where `n INT NULL` | `LONGLONG` | length `12`, decimals `0` | `BINARY NUM` |
| `id * id AS id_mul` where `id INT NOT NULL` | `LONGLONG` | length `21`, decimals `0` | `NOT_NULL BINARY NUM` |
| `u * n AS u_n_mul` where `u INT UNSIGNED NOT NULL` and `n INT NULL` | `LONGLONG` | length `20`, decimals `0` | `UNSIGNED BINARY NUM`; nullable |
| `-n AS n_neg` where `n INT NULL` | `LONGLONG` | length `11`, decimals `0` | `BINARY NUM`; nullable |
| `-u AS u_neg` where `u INT UNSIGNED NOT NULL` | `LONGLONG` | length `11`, decimals `0` | `NOT_NULL BINARY NUM` |
| `+n AS n_pos` where `n INT NULL` | same descriptor and origin metadata as `n` |
| `+u AS u_pos` where `u INT UNSIGNED NOT NULL` | same descriptor and origin metadata as `u` |
| `-d AS d_neg` where `d DECIMAL(6,2)` | `NEWDECIMAL` | length `8`, decimals `2` | `BINARY NUM`; nullable |
| `d + 1 AS d_plus` where `d DECIMAL(6,2)` | `NEWDECIMAL` | length `9`, decimals `2` | `BINARY NUM`; nullable |
| `d - 1 AS d_sub` where `d DECIMAL(6,2)` | `NEWDECIMAL` | length `9`, decimals `2` | `BINARY NUM`; nullable |
| `d * 2 AS d_mul` where `d DECIMAL(6,2)` | `NEWDECIMAL` | length `9`, decimals `2` | `BINARY NUM`; nullable |
| `d / 2 AS d_div` where `d DECIMAL(6,2)` | `NEWDECIMAL` | length `12`, decimals `6` | `BINARY NUM`; nullable |
| `u / u AS u_div_u` where `u INT UNSIGNED NOT NULL` | `NEWDECIMAL` | length `15`, decimals `4` | `UNSIGNED BINARY NUM`; nullable |
| `r / 2 AS r_div` where `r DOUBLE` | `DOUBLE` | length `23`, decimals `31` | `BINARY NUM`; nullable |
| `s / 2 AS s_div` where `s VARCHAR(12)` | `DOUBLE` | length `23`, decimals `31` | `BINARY NUM`; nullable |
| `r + 1 AS r_plus` where `r DOUBLE` | `DOUBLE` | length `23`, decimals `31` | `BINARY NUM`; nullable |
| `s + 1 AS s_plus` where `s VARCHAR(12)` | `DOUBLE` | length `23`, decimals `31` | `BINARY NUM`; nullable |
| `-r AS r_neg` where `r DOUBLE` | `DOUBLE` | length `23`, decimals `31` | `BINARY NUM`; nullable |
| `-s AS s_neg` where `s VARCHAR(12)` | `DOUBLE` | length `23`, decimals `31` | `BINARY NUM`; nullable |
| `5 / 2 AS slash_expr` | `NEWDECIMAL` | length `7`, decimals `4` | `BINARY NUM` |
| `5 DIV 2 AS div_expr` | `LONGLONG` | length `2`, decimals `0` | `BINARY NUM` |
| `1 = 1 AS eq_expr` | `LONGLONG` | length `1`, decimals `0` | `NOT_NULL BINARY NUM` |
| `n IS NULL` | `LONGLONG` | length `1`, decimals `0` | `NOT_NULL BINARY NUM` |
| `n <=> NULL` | `LONGLONG` | length `1`, decimals `0` | `NOT_NULL BINARY NUM` |
| `n IN (1,2)` | `LONGLONG` | length `1`, decimals `0` | `BINARY NUM` if nullable |
| `n BETWEEN 1 AND 20` | `LONGLONG` | length `1`, decimals `0` | `BINARY NUM` if nullable |
| `s LIKE 'a%'` | `LONGLONG` | length `1`, decimals `0` | `BINARY NUM` if nullable |
| `1 & 3` | `LONGLONG` | implementation should set `UNSIGNED BINARY NUM` |

Where exact display length rules are not yet fully specified, MyLite should
prefer a conservative MySQL-compatible maximum for the result type rather than
the observed value length. For example, nullable `n + 1` over `INT` used length
`12`, accounting for signed integer display width plus possible carry.

### Aggregate and function deferrals

This task should not claim aggregate or general built-in function metadata as
supported. It should create an inference structure that later tasks can extend:

- Task 24 owns scalar built-in function return types, collations, decimals, and
  nullability. Its current descriptor slices reuse this statement-owned
  metadata path for table-backed scalar calls, including `ABS`, `MOD`,
  `FLOOR`, `CEIL`, and `CEILING` over the covered integer, unsigned integer,
  decimal, approximate, text, and `NULL` argument domains.
- Task 25 owns aggregate return metadata, including `COUNT(*)` being non-null
  integer metadata and `SUM`/`AVG` precision rules.
- Grouping, joins, set operations, and subqueries own merged output metadata
  for their result shapes.

The MySQL probe observed `SELECT COUNT(*) AS count_all FROM meta_t` only as a
future fixture. Task 23 should leave it unsupported unless aggregate execution
already exists when implementation starts.

### Errors, warnings, and side effects

Metadata generation has no independent SQL side effects:

- no catalog mutation
- no data mutation
- no last-insert-id change
- no affected-row change beyond existing `SELECT` semantics

Metadata must still be available for empty result sets, including `LIMIT 0`.

Metadata inference should normally occur during statement preparation or
binding. If expression metadata binding detects an unsupported projected
expression, MyLite should return the same deterministic unsupported-feature
diagnostic used by the expression/runtime layer. It should not execute a query
partially and then fail while exposing incomplete metadata.

Warnings belong to expression evaluation, not metadata construction. A
`LIMIT 0` query exposes metadata but does not evaluate row expressions and
should not create conversion warnings from projected or hidden row-dependent
expressions. Constant expression folding must preserve MySQL warning timing;
do not precompute warnings during prepare unless MySQL also reports them before
row evaluation for that expression shape.

## MyLite grammar and AST notes

Task 23 does not require new SQL grammar. It relies on the existing projection
and alias grammar from Tasks 15 and 16. The relevant Lemon-shape contract is:

```lemon
select_item(A) ::= expr(E). {
    A = mylite_ast_select_item(E, NULL);
}

select_item(A) ::= expr(E) AS_SYM select_alias(L). {
    A = mylite_ast_select_item(E, L);
}

select_item(A) ::= expr(E) select_alias(L). {
    A = mylite_ast_select_item(E, L);
}

select_item(A) ::= wildcard(W). {
    A = mylite_ast_select_wildcard(W);
}
```

The implementation need is AST preservation:

- preserve the exact explicit alias token text after identifier/string-literal
  unquoting according to existing alias rules
- preserve enough expression source text to produce MySQL-compatible default
  labels for unaliased expressions, except string literals where MySQL uses
  the unquoted literal value
- retain wildcard expansion order and origin column identity
- keep hidden `ORDER BY` expressions outside the output list

If current AST nodes discard normalized expression text, Task 23 should add a
statement-owned display-label string during binding rather than changing lexer
token ownership globally.

## MyLite metadata model

Extend `struct mylite_result_column_metadata` into a complete internal
descriptor. The public ABI can expose this through individual accessors first
and a growable descriptor struct later if needed.

Required internal fields:

- visible label
- original column name
- visible schema name
- visible table name or alias
- origin schema name
- origin table name
- catalog name, fixed to `def` where protocol code needs it
- MySQL field type enum
- display length in bytes
- maximum observed length, initially optional
- decimals
- collation id and/or internal charset/collation descriptor
- flags bitset
- nullable boolean derived from `NOT_NULL`
- expression-origin marker for empty origin fields

Ownership:

- metadata is statement-owned
- text fields use length-aware internal strings
- public string accessors return borrowed statement-owned text
- descriptors are immutable after successful prepare/bind
- cleanup remains part of statement finalization

ABI notes:

- Do not expose raw internal struct layout.
- If MyLite mirrors MySQL flag values, document the numeric constants in a
  public header and keep them stable.
- Prefer explicit public constants such as `MYLITE_COLUMN_NOT_NULL` over
  leaking SQLite or private bit values.
- Add length-aware accessors where possible; NUL-terminated convenience
  accessors can remain for labels and names.

## Type inference rules

### Direct base columns

Base-column metadata comes from the MyLite catalog and index metadata:

1. Resolve the projected column exactly as Task 15 does.
2. Copy the visible label from the alias or base column name.
3. Copy `org_name` from the catalog column name.
4. Set visible table to the table alias when present, otherwise the table name.
5. Set origin schema/table to the real catalog schema/table.
6. Map catalog type descriptors to MySQL field type, display length, decimals,
   charset id, and flags.
7. Add key flags from primary, unique, and secondary index membership.
8. Add `AUTO_INCREMENT`, `UNSIGNED`, `BINARY`, `BLOB`, `NUM`, and `NOT_NULL`
   flags from catalog attributes.

Primary-key columns must expose `NOT_NULL` even if the original column
definition omitted an explicit `NOT NULL`, matching MySQL's primary-key
metadata behavior.

### Literals

Literal descriptors should be inferred without reading rows:

- integer literals produce signed or unsigned `LONGLONG` metadata depending on
  literal range and operator context
- `NULL` produces `MYSQL_TYPE_NULL`, nullable metadata, empty origin, length
  `0`
- nonbinary string literals produce `VAR_STRING` metadata in the result
  character set, with byte length equal to character length times maximum bytes
  per character for that charset
- binary string literals produce `VAR_STRING` metadata with binary collation,
  byte length equal to literal byte length, and `BINARY`
- decimal literals should produce `NEWDECIMAL` with precision/scale-derived
  length and decimals once decimal literal metadata is implemented

### Operators

Use Task 16 expression semantics as the basis for result values and warnings.
Task 23 adds static metadata inference:

- comparison and logical predicates return `LONGLONG`, length `1`, `BINARY`,
  `NUM`; set `NOT_NULL` only for operators whose semantics cannot return
  `NULL`
- `IS NULL`, `IS NOT NULL`, `IS TRUE`, `IS FALSE`, `IS UNKNOWN`, and
  null-safe equality return non-null integer metadata
- arithmetic integer operators return `LONGLONG`; exact arithmetic involving a
  table-backed `DECIMAL` operand keeps `NEWDECIMAL` metadata for the covered
  `+`, `-`, and `*` cases
- `/` over exact numeric operands returns `NEWDECIMAL` with MySQL-compatible
  scale rules and nullable metadata, even when both operands are non-null;
  `/` over approximate or text operands returns `DOUBLE`
- `DIV` and modulo return integer metadata but remain nullable for division by
  zero or nullable operands
- bitwise operators return unsigned integer metadata where MySQL marks
  `UNSIGNED`
- string comparison operators return integer metadata and inherit nullable
  status from operands where SQL three-valued logic can produce `NULL`

When a rule is not yet verified for a supported expression, implementation
should either add a MySQL 8.4.9 probe and test expectation or keep that
expression unsupported as a projected output.

### Wildcards

Wildcard expansion creates one descriptor per visible base column in catalog
ordinal order. Invisible columns are omitted from `*` and qualified wildcards
but keep normal metadata when selected explicitly by name.

## MySQL-runtime-verified test expectations

### Fixture

Use a deterministic fixture similar to:

```sql
DROP DATABASE IF EXISTS mylite_task23_metadata;
CREATE DATABASE mylite_task23_metadata DEFAULT CHARACTER SET utf8mb4;
USE mylite_task23_metadata;

CREATE TABLE meta_t (
  id INT NOT NULL AUTO_INCREMENT,
  n INT NULL,
  u INT UNSIGNED NOT NULL,
  s VARCHAR(12) NULL,
  c CHAR(3) NOT NULL,
  txt TEXT,
  b VARBINARY(8),
  d DECIMAL(6,2),
  f DOUBLE,
  dt DATETIME(3),
  ts TIMESTAMP NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uq_u (u),
  KEY k_n (n)
);
```

Set `character_set_results` explicitly in metadata tests that assert string
byte lengths.

### C API / public metadata tests

| SQL | Expected metadata |
| --- | --- |
| `SELECT id, n AS alias_n, n AS alias_n FROM meta_t AS mt LIMIT 0` | three result fields, duplicate `alias_n` labels preserved; base columns expose `db=mylite_task23_metadata`, visible table `mt`, origin table `meta_t`, origin names `id`/`n` |
| `SELECT id AS label, n AS label FROM meta_t LIMIT 0` | both labels are `label`; origin names remain `id` and `n` |
| `SELECT (id) AS paren_id, +((n)) AS plus_n FROM meta_t AS mt LIMIT 0` | parenthesized references and unary-positive references preserve base-column descriptors and origin metadata through the table alias |
| `SELECT n + 1 AS expr FROM meta_t LIMIT 0` | label `expr`; empty origin schema/table/column; `LONGLONG`, length `12`, nullable |
| `SELECT 1 AS one, NULL AS nil, 'abc' AS str_lit, _binary 'abc' AS bin_lit LIMIT 0` | literal metadata from the table above; all origins empty |
| `SELECT id FROM meta_t ORDER BY n + 1 LIMIT 0` | one result field only, for `id`; hidden order key not exposed |
| `SELECT n AS id, id FROM meta_t ORDER BY id LIMIT 0` | error 1052 / `23000`, not metadata disambiguation |
| `SELECT n AS id, id FROM meta_t ORDER BY meta_t.id LIMIT 0` | succeeds with duplicate `id` labels preserved |
| `SELECT * FROM meta_t LIMIT 0` | visible columns in catalog order; each descriptor matches direct column metadata |

### Type and flag tests

| SQL | Expected metadata |
| --- | --- |
| `SELECT id FROM meta_t LIMIT 0` | `LONG`, length `11`, `NOT_NULL PRI_KEY AUTO_INCREMENT NUM` |
| `SELECT u FROM meta_t LIMIT 0` | `LONG`, length `10`, `NOT_NULL UNIQUE_KEY UNSIGNED NUM` plus no-default/key flags where public API exposes them |
| `SELECT s FROM meta_t LIMIT 0` | `VAR_STRING`, charset-dependent length, nullable |
| `SELECT b FROM meta_t LIMIT 0` | `VAR_STRING`, length `8`, binary collation, `BINARY` |
| `SELECT d FROM meta_t LIMIT 0` | `NEWDECIMAL`, length `8`, decimals `2`, `NUM` |
| `SELECT f FROM meta_t LIMIT 0` | `DOUBLE`, length `22`, decimals `31`, `NUM` |
| `SELECT dt, ts FROM meta_t LIMIT 0` | temporal types with binary collation; `DATETIME(3)` decimals `3`, `TIMESTAMP` decimals `0` |
| `SELECT 1 + 2, d + 1, d - 1, d * 2, d / 2, 5 / 2, 5 DIV 2 FROM meta_t LIMIT 0` | `LONGLONG`, three `NEWDECIMAL(9,2)` table-decimal arithmetic fields, table-backed `NEWDECIMAL(12,6)` division, scalar `NEWDECIMAL(7,4)` division, and `LONGLONG` respectively |
| `SELECT n IS NULL, n <=> NULL, n IN (1,2), s LIKE 'a%' FROM meta_t LIMIT 0` | all `LONGLONG` length `1`; first two non-null, latter two nullable with nullable operands |

### Runtime and lifecycle tests

| Behavior | Expected result |
| --- | --- |
| prepare/select metadata before stepping | metadata is available before first row |
| `LIMIT 0` | metadata available, no rows, no row-expression warnings |
| empty table without `LIMIT` | metadata available even when no rows are produced |
| statement finalize | borrowed metadata pointers become invalid; no leaks under sanitizer builds |
| repeated prepare on same handle | previous statement metadata does not leak into new statement |
| unsupported projected expression | deterministic unsupported diagnostic, no partial metadata exposure |

## Implementation plan

1. Add internal MySQL field type, flag, and collation constants. Keep values
   stable and separate from SQLite types.
2. Extend the result metadata descriptor and cleanup/copy helpers.
3. Add catalog-column to result-field mapping for all currently supported
   column types and attributes.
4. Add expression metadata inference for literals and the Task 16 operator
   subset.
5. Teach scalar `SELECT` and table-backed `SELECT` binding to attach complete
   descriptors for every output expression.
6. Preserve and expose default labels for unaliased expressions; use explicit
   aliases when present.
7. Keep hidden `ORDER BY` expression descriptors internal-only.
8. Add public accessors for type, flags, length, decimals, charset/collation,
   nullability, and original column name. Keep existing name/table/origin
   accessors ABI-compatible.
9. Update protocol planning code, if present, to consume the same descriptor
   rather than re-inferring metadata.
10. Add runtime tests with MySQL-verified expectations, including C API-style
    metadata assertions where possible.

## Storage and performance implications

No `.mylite` file-format change is required. Result metadata is derived from
existing catalog rows, parsed expressions, and statement/session charset state.

Performance requirements:

- compute descriptors once during prepare/bind, not per row
- store result labels and origin names in statement-owned memory
- avoid duplicating catalog strings per row
- avoid SQLite metadata introspection for MySQL-facing descriptors, because
  SQLite type affinity and origin behavior do not match MySQL
- make hidden sort-key metadata reusable by sorter code without exposing it as
  output metadata

## SQLite-vs-MySQL semantic risks

- SQLite reports dynamic value types, while MySQL reports static result-field
  metadata before row values are known.
- SQLite origin metadata and expression labels are not MySQL-compatible,
  especially for aliases, duplicate labels, and expressions.
- SQLite does not expose MySQL key flags, unsigned attributes, decimal scale,
  character-set ids, or MySQL display lengths.
- SQLite string length semantics are byte/value oriented and cannot substitute
  for MySQL's charset-sensitive field length.
- SQLite may optimize hidden sort expressions differently; MyLite must keep
  client-visible output descriptors tied only to the projection list.

## Explicit deferred behavior

- Function metadata is deferred to Task 24.
- Aggregate and grouping metadata is deferred to Task 25.
- Join and duplicate-column merging behavior is deferred to Tasks 26 and 27.
- `DISTINCT` metadata interactions are deferred to Task 28.
- Subquery and derived-table origin metadata is deferred to Task 29 and later
  table-reference tasks.
- `UNION` type aggregation and output label selection are deferred to Task 30.
- Prepared-statement metadata refresh, binary protocol result metadata, and
  parameter metadata are deferred to Task 42.
- Optional metadata suppression through `resultset_metadata=NONE` is deferred
  until system variables and protocol negotiation exist.
- Full `max_length` population is deferred unless implementation already
  materializes complete result sets cheaply and can match MySQL timing.

## Remaining implementation risks

- Exact expression display-length rules are broad and sometimes depend on
  operand metadata. Task 23 should start with verified supported expressions
  and leave unknown expressions unsupported.
- Character-set and collation ids must align with MyLite's charset registry.
  Tests need explicit connection charset state to avoid client-default drift.
- Public ABI expansion should not lock MyLite into an incomplete struct layout.
  Individual accessors or versioned/growable descriptors are safer.
- Future protocol code must not reimplement metadata inference separately; a
  single descriptor source is necessary to avoid C API/protocol divergence.
