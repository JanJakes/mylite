# Baseline Select Where Lifecycle

## Status

This feature specifies the next narrow user-visible query slice for file-backed
`.mylite` handles. It adds a descriptor-driven `WHERE` predicate to the
existing limited table `SELECT` path on top of `mylite_execute()`, statement
context, the MyLite parser scaffold, shifted `.mylite` storage, durable catalog
descriptors, basic create/drop/rename table lifecycle, and integer/`NULL` row
values.

The feature is intentionally not full MySQL `WHERE` or expression support. It
supports one persistent base table, descriptor-backed projections, and one
simple predicate over one descriptor column.

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
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, comparison operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, working with `NULL`:
  https://dev.mysql.com/doc/refman/8.4/en/working-with-null.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for `SELECT ... FROM table_name WHERE predicate`;
- `SELECT *` and explicit unqualified projection column lists over a single
  persistent base table;
- unqualified and schema-qualified table names using the existing selected
  schema policy;
- one predicate over one descriptor column;
- comparison predicates with a column on the left and a decimal integer
  literal with optional unary sign on the right;
- comparison operators `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`;
- `IS NULL` and `IS NOT NULL` predicates;
- optional parentheses around that one supported predicate;
- descriptor-driven projection and predicate column resolution;
- MyLite-owned predicate literal conversion before SQLite binding;
- generated SQLite SQL built only from descriptors and stable physical table
  names;
- prepared-statement binding for comparison literals;
- result rows, column names, `NULL` value behavior, affected rows, and warning
  count matching the existing row-values result API for the supported subset;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- full expression evaluation or expression result metadata;
- literal-on-left comparisons;
- table-qualified predicate columns;
- predicate column aliases or table aliases;
- `col = NULL`, `col <> NULL`, or any comparison to a `NULL` literal;
- decimal, float, string, hex, bit, boolean, temporal, JSON, parameter, or
  function predicate values;
- arithmetic, casts, collations, user variables, system variables, subqueries,
  row constructors, `BETWEEN`, `IN`, `LIKE`, `REGEXP`, `IS TRUE`, `IS FALSE`,
  `NOT`, `AND`, `OR`, `XOR`, or chained predicates;
- joins, grouping, `HAVING`, ordering, limiting, distinct, set operations, CTEs,
  locking clauses, query modifiers, aliases, or arbitrary SQLite pass-through;
- indexes, helper keys, generalized comparison hooks, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, affected rows, and backend execution status. Filtered
  `SELECT` does not mutate statement transaction state.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves schemas, table descriptors, projection
  columns, and predicate columns against MyLite catalog descriptors; rejects
  unsupported predicate shapes; converts supported predicate literals; and
  generates a physical SQLite query plan.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. Filtered reads do not
  mutate catalog rows, descriptor versions, generation, caches, or
  `sqlite_schema_generation`.
- The result builder owns descriptor-driven result column names and copied text
  values. SQL `NULL` remains represented as a `NULL` pointer returned by
  `mylite_result_value_text()`.
- SQLite owns physical b-tree row storage and scan execution for the generated
  query. SQLite schema text and `PRAGMA` output are not metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Filtered reads must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported table `SELECT` subset:

```sql
SELECT * FROM table_name WHERE predicate
SELECT column_name[, column_name ...] FROM table_name WHERE predicate
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

Supported predicate subset:

```sql
predicate:
    column_name comparison_operator signed_integer_literal
  | column_name IS NULL
  | column_name IS NOT NULL
  | ( predicate )

comparison_operator:
    =
  | <=>
  | <>
  | !=
  | <
  | <=
  | >
  | >=

signed_integer_literal:
    integer_literal
  | + integer_literal
  | - integer_literal
```

Only unqualified column names are supported in projections and predicates.
Parentheses do not introduce general expression support; they only wrap the
single supported predicate.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
select_statement ::=
    SELECT select_item_list FROM table_name where_clause_opt.
select_statement ::=
    SELECT STAR FROM table_name where_clause_opt.

where_clause_opt ::= .
where_clause_opt ::= WHERE predicate.

predicate ::= predicate_atom.
predicate ::= LPAREN predicate RPAREN.

predicate_atom ::= qualified_identifier comparison_operator predicate_integer_value.
predicate_atom ::= qualified_identifier IS NULL.
predicate_atom ::= qualified_identifier IS NOT NULL.

comparison_operator ::= EQUAL.
comparison_operator ::= NULL_SAFE_EQUAL.
comparison_operator ::= NOT_EQUAL.
comparison_operator ::= LESS.
comparison_operator ::= LESS_EQUAL.
comparison_operator ::= GREATER.
comparison_operator ::= GREATER_EQUAL.

predicate_integer_value ::= INTEGER.
predicate_integer_value ::= PLUS INTEGER.
predicate_integer_value ::= MINUS INTEGER.
```

The grammar admits `qualified_identifier` on the predicate left side only so
the analyzer can reject table-qualified predicate columns with a clear
unsupported diagnostic. The supported semantic subset requires a single
unqualified descriptor column.

## Schema And Table Resolution

`SELECT ... FROM t WHERE ...` uses the connection's selected schema. If no
selected schema exists, the statement fails with MySQL error `1046`, SQLSTATE
`3D000`, and message `No database selected`.

`SELECT ... FROM db.t WHERE ...` resolves `db` against existing MyLite schema
descriptors regardless of the selected schema. Unknown schemas fail with MySQL
error `1049`, SQLSTATE `42000`, and message `Unknown database '<schema>'`.

The source table descriptor must exist and must have
`kind == MYLITE_CATALOG_TABLE_KIND_BASE`. Unknown tables fail with MySQL error
`1146`, SQLSTATE `42S02`, and message
`Table '<schema>.<table>' doesn't exist`. Unsupported object kinds are rejected
with a deterministic unsupported diagnostic until view and temporary-table
descriptors are specified.

User-authored schema and table names beginning with `_mylite_`, using ASCII
case-insensitive comparison after identifier unquoting, are rejected before any
SQLite SQL is generated.

## Column Resolution

Projection column resolution preserves the row-values lifecycle behavior:

- `*` expands to all descriptor columns in catalog ordinal order;
- explicit projection names resolve case-insensitively against descriptor
  column names;
- result column names are the descriptor names of selected columns;
- duplicate projected column names are allowed and produce duplicate result
  columns;
- unknown projection names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'field list'`;
- table-qualified projection names remain unsupported.

Predicate column resolution uses the same descriptor list:

- the supported predicate column must be one unqualified descriptor column name;
- unknown predicate names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'where clause'`;
- table-qualified predicate names are rejected with a deterministic unsupported
  diagnostic before SQLite SQL is generated.

The current catalog stores `(table_id, name)` with SQLite binary uniqueness.
MyLite analysis compares the supported identifier subset with ASCII
case-insensitive folding, matching previous baseline column-resolution policy.

## Predicate Semantics

For supported comparison predicates, MyLite converts the right-hand integer
literal to the predicate column's logical integer domain before SQLite binding.
Rows are selected when the comparison is true. Rows for which the comparison
would evaluate to SQL `NULL` are not selected.

Supported comparison behavior:

| Predicate | Semantics in this slice |
| --- | --- |
| `col = n` | Selects non-`NULL` column values equal to `n`. |
| `col <=> n` | With a non-`NULL` integer right operand, selects the same rows as `col = n`; `NULL` column values do not match. |
| `col <> n`, `col != n` | Selects non-`NULL` column values not equal to `n`. |
| `col < n`, `col <= n`, `col > n`, `col >= n` | Selects non-`NULL` column values satisfying the integer comparison. |
| `col IS NULL` | Selects rows where the descriptor column is SQL `NULL`. |
| `col IS NOT NULL` | Selects rows where the descriptor column is not SQL `NULL`. |

`col = NULL` and `col <> NULL` are intentionally not admitted in this phase.
MySQL's three-valued comparison behavior for those forms is recorded in the
expectation script, but MyLite defers comparison-to-`NULL` syntax until a
broader expression and three-valued-logic slice specifies it fully.

## Predicate Literal Conversion

Supported predicate values are converted by MyLite before SQLite binding:

- an unsigned magnitude is parsed from the integer token without locale;
- optional unary `+` keeps the value positive;
- optional unary `-` negates the magnitude when the target type permits it;
- unsupported literal or expression nodes fail before SQLite execution.

The supported predicate ranges match the current row-values physical encoding:

| Logical type | Supported predicate literal range |
| --- | --- |
| `INT` | `-2147483648` through `2147483647` |
| `INT UNSIGNED` | `0` through `4294967295` |
| `BIGINT` | `-9223372036854775808` through `9223372036854775807` |
| `BIGINT UNSIGNED` | `0` through `9223372036854775807` |

MySQL can evaluate some comparisons between a column and a literal outside the
column's assignment range. MyLite deliberately rejects predicate literals
outside the supported descriptor range in this phase with MySQL error `1264`,
SQLSTATE `22003`, and a MyLite-specific message
`Out of range value for column '<column>' in WHERE`. This avoids silently
claiming full expression conversion before wider integer comparison semantics
and unsigned 64-bit physical storage are specified.

Warnings are not generated by supported in-range predicates.

## Physical SQLite Handling

Generated SQL is built only from descriptors:

- table names come from `_mylite_catalog_tables.physical_name`;
- column names come from MyLite column descriptors;
- every SQLite identifier is double-quoted with embedded double quotes doubled;
- comparison values are bound through `sqlite3_bind_int64()`;
- generated SQL contains no interpolated user literals.

The generated query shape is:

```sql
SELECT "<projection_col1>", "<projection_col2>", ...
FROM "<physical_name>"
WHERE "<predicate_col>" <op> ?1
```

For `IS NULL` and `IS NOT NULL`, the generated predicate uses `IS NULL` or
`IS NOT NULL` and has no bound parameter.

`<=>` with a supported non-`NULL` integer right operand is lowered to a physical
integer equality predicate because the selected row set is identical for this
slice: `NULL` column values do not match a non-`NULL` right operand. A future
slice that admits right-hand `NULL`, arbitrary expressions, or selected
predicate values must re-evaluate this lowering.

SQLite is used as public-API physical row storage and scan execution. No SQLite
fork hook is needed for this descriptor-limited integer/`NULL` subset.

## Result Behavior

Successful filtered table `SELECT` returns a text result through the existing
public result API:

- `column_count` equals the selected descriptor column count;
- `row_count` equals the number of filtered rows read;
- `affected_rows == 0`;
- `warning_count == 0`;
- column names come from selected descriptors;
- integer values are decimal strings;
- SQL `NULL` values return `NULL` from `mylite_result_value_text()`.

This phase does not claim protocol-grade metadata, column flags, charsets,
origin metadata, expression labels, or full MySQL type codes.

## Read Ordering

MySQL does not guarantee a general row order for table scans without
`ORDER BY`. This phase does not add `ORDER BY`, and compatibility docs must not
claim general ordering compatibility.

Tests should prefer predicates that return one deterministic row. Where a test
uses a multi-row filtered scan over a generated table, it documents the narrow
observed MySQL 8.4.9 and SQLite row-storage behavior instead of treating that
order as a general compatibility contract.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported select form or predicate shape | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Unknown table | `MYLITE_ERROR` | `1146`, `42S02`, `Table '<schema>.<table>' doesn't exist` |
| Reserved schema name | `MYLITE_ERROR` | `1102`, `42000`, `Incorrect database name '<name>'` |
| Reserved table name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` |
| Unknown projection column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'field list'` |
| Unknown predicate column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'where clause'` |
| Unsupported predicate expression | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Unsupported predicate literal | `MYLITE_ERROR` | `1064`, `42000`, deterministic syntax or unsupported message |
| Unsupported predicate operator | `MYLITE_ERROR` | `1064`, `42000`, deterministic syntax or unsupported message |
| Predicate literal outside supported descriptor range | `MYLITE_ERROR` | `1264`, `22003`, `Out of range value for column '<column>' in WHERE` |
| Unsupported object kind | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Physical SQLite failure | `MYLITE_ERROR` | `1105`, `HY000`, deterministic internal failure message |
| Allocation failure | `MYLITE_NOMEM` | `MYLITE_NOMEM`, `HY001`, allocation message |

No public ABI changes are planned, so existing public execution/result misuse
behavior is preserved.

## MySQL 8.4.9 Runtime Observations

The following behavior was checked on 2026-05-07 against the official
`mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --force --show-warnings
```

Observed behavior used by this feature:

| SQL | MySQL 8.4.9 observation |
| --- | --- |
| `SELECT i, nn FROM numbers WHERE i = 1` | Returns the matching row and no warnings. |
| `SELECT id FROM single_values WHERE id <> 2` | Returns the row where `id` is not `2`. |
| `SELECT id FROM single_values WHERE id != 1` | Returns no rows. |
| `SELECT i FROM numbers WHERE i < 0` | Returns the negative row. |
| `SELECT i FROM numbers WHERE i <= -2` | Returns the `-2` row. |
| `SELECT i FROM numbers WHERE i > 2147483646` | Returns the `2147483647` row. |
| `SELECT i FROM numbers WHERE i >= 2147483647` | Returns the `2147483647` row. |
| `SELECT i FROM numbers WHERE i <=> 1` | Returns the row where `i = 1`. |
| `SELECT id FROM null_probe WHERE n IS NULL` | Returns only rows where `n` is SQL `NULL`. |
| `SELECT id FROM null_probe WHERE n IS NOT NULL` | Returns only rows where `n` is not SQL `NULL`. |
| `SELECT i FROM numbers WHERE n = NULL` | Returns no rows. MyLite defers this syntax in this phase. |
| `SELECT i FROM numbers WHERE n <> NULL` | Returns no rows. MyLite defers this syntax in this phase. |
| `SELECT i FROM numbers WHERE (i = +1)` | Parenthesized single predicate and unary plus are accepted. |
| Boundary comparisons for `INT`, `INT UNSIGNED`, `BIGINT`, and the signed-64-bit subset of `BIGINT UNSIGNED` | Return expected matching rows with no warnings. |
| Unknown projection column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'field list'`. |
| Unknown predicate column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'where clause'`. |
| Unknown table in filtered `SELECT` | Error `1146`, SQLSTATE `42S02`. |
| Unknown schema-qualified filtered `SELECT` | Error `1049`, SQLSTATE `42000`. |
| Literal-on-left comparisons, table-qualified predicate columns, expression predicates, string predicate literals, `AND`, `ORDER BY`, and `LIMIT` | Accepted by MySQL for ordinary queries; intentionally rejected by MyLite in this phase. |

The reproducible probe script for this phase is
`packages/libmylite/tests/mysql_baseline_select_where_lifecycle_expectations.sh`.

## Compatibility Status

This feature moves only the exact supported subset to partial support after
implementation:

- `SELECT`: descriptor-driven single persistent base-table `SELECT *` and
  unqualified projection reads with one supported `WHERE` predicate;
- `WHERE`: limited one-column integer/`NULL` predicates only;
- comparison operators `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`:
  supported only inside descriptor-driven filtered table `SELECT` predicates;
- `IS NULL` and `IS NOT NULL`: supported only inside descriptor-driven filtered
  table `SELECT` predicates;
- numeric literals: decimal integer literals with optional unary sign as
  supported predicate inputs.

Full `WHERE`, full comparison operators, expression-level operator support,
general expression evaluation, `ORDER BY`, `LIMIT`, joins, aliases, grouping,
subqueries, protocol-grade metadata, and arbitrary SQLite pass-through remain
unsupported.

## Tests

Add a fast plain C test under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.select_where_lifecycle`.

Coverage must include:

- parser/AST acceptance for filtered wildcard and explicit projection selects;
- parser/AST acceptance for every admitted comparison operator, `IS NULL`,
  `IS NOT NULL`, unary `+`, unary `-`, and parenthesized predicates;
- successful filtered `SELECT *` and `SELECT col, col2` over `INT`, `INTEGER`,
  `BIGINT`, `INT UNSIGNED`, `INTEGER UNSIGNED`, and `BIGINT UNSIGNED` columns
  within the supported physical range;
- equality and each admitted comparison operator;
- nullable-column `IS NULL` and `IS NOT NULL`;
- schema-qualified and unqualified target table resolution, no selected schema,
  unknown schema, unknown table, and reserved `_mylite_*` source names;
- projection column names, row count, text values, `NULL` values, warning
  count, and affected-row semantics;
- duplicate projected columns with a filtered predicate;
- unknown predicate columns and unknown projected columns;
- unsupported predicate syntax and clauses rejected deterministically:
  table-qualified predicate columns, expression predicates, literal-on-left
  comparisons, `col = NULL`, `col <> NULL`, string/decimal/float/hex/bit
  literals, functions, parameters, `AND`, `OR`, `ORDER BY`, `LIMIT`, joins,
  aliases, grouping, and subqueries;
- predicate literal out-of-range diagnostics for the supported descriptor
  ranges;
- reopen persistence, table rename visibility, and drop behavior;
- physical SQLite payload behavior without touching the MyLite preamble;
- independent file-backed handles with independent filtered row state;
- zero-initialized cleanup for any new planner/result objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, basic create/drop lifecycle, table rename lifecycle, row values
  lifecycle, client-data, and registration tests still pass.

## Build Integration

Add any new runtime/analyzer/planner/catalog SQL execution sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.select_where_lifecycle$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.basic_table_lifecycle|runtime\.table_rename_lifecycle|runtime\.row_values_lifecycle)$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_select_where_lifecycle_expectations.sh
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor-driven physical filtering, comparison-conversion
correctness, file-format safety, VFS preservation, zero-init safety, cleanup on
failure, scope control, compatibility-matrix accuracy, and test relevance.
