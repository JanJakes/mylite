# Baseline SELECT DISTINCT Rowsets

## Goal

Extend the current one-column `SELECT DISTINCT` baseline into a narrow
descriptor-backed rowset distinct path for common table reads:

```sql
SELECT DISTINCT user_id, meta_key FROM wp_usermeta ORDER BY user_id, meta_key;
SELECT DISTINCT * FROM wp_options;
```

This phase keeps `DISTINCT` on the existing single-table descriptor `SELECT`
path. It does not add joined distinct, grouped distinct, scalar/tableless
distinct, expression distinct, or arbitrary SQLite pass-through.

## Sources

- Official MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Official MySQL 8.4 Reference Manual, `DISTINCT` optimization:
  <https://dev.mysql.com/doc/refman/8.4/en/distinct-optimization.html>
- Existing MyLite descriptor `SELECT`, ordering, limiting, alias, and
  row-value specifications under `docs/specs/`.
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_select_distinct_rowsets_expectations.sh`.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `DISTINCT` removes duplicate result rows after projection; all selected
  columns participate in row equality.
- `DISTINCTROW` is a synonym for `DISTINCT`.
- `NULL` values compare equal to other `NULL` values for duplicate-row
  elimination.
- Under the default `utf8mb4_0900_ai_ci` collation, nonbinary string columns
  compare case-insensitively for duplicate elimination; in the verified ASCII
  subset, `'Alpha'` and `'alpha'` collapse to one distinct value.
- `ORDER BY` runs after duplicate elimination for the visible output rows.
  MySQL 8.4.9 rejects `DISTINCT` queries whose `ORDER BY` expressions are not
  present in the select list with error 3065 / `HY000`.
- `LIMIT` applies to the unique rows returned by the distinct operation.
- Successful supported `SELECT DISTINCT` statements set `ROW_COUNT()` to `-1`
  and produce zero warnings.
- `SELECT DISTINCT` with no table source and `SELECT DISTINCT ... FROM DUAL`
  are valid MySQL scalar query forms, but they belong to a different MyLite
  execution path and are deferred here.

## Supported Surface

MyLite supports:

- `SELECT DISTINCT ... FROM table_name`;
- `SELECT DISTINCTROW ... FROM table_name`;
- unqualified and schema-qualified persistent base-table names, and the current
  shadowing session temporary table behavior already used by descriptor
  `SELECT`;
- optional table aliases on the single table source;
- one or more explicit descriptor columns in the projection list;
- visible `*` wildcard projection;
- visible qualified wildcard projection for the one source:
  `table.*`, `schema.table.*`, or `alias.*` according to the existing
  qualified wildcard policy;
- optional existing `WHERE` predicate subset;
- optional existing `ORDER BY` descriptor-column subset, limited to selected
  columns or selected-column aliases;
- optional existing `LIMIT` and `OFFSET` forms;
- the current no-op select modifiers already admitted beside `DISTINCT`, except
  `SQL_CALC_FOUND_ROWS`;
- selected descriptor columns whose stored values have MyLite-owned equality
  semantics compatible with this baseline:
  - integer-family columns, including `BOOL`/`BOOLEAN` aliases;
  - `YEAR`;
  - canonical `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
  - ASCII nonbinary `CHAR`, `VARCHAR`, and baseline `TEXT` family columns using
    MyLite's registered `utf8mb4_0900_ai_ci` ASCII collation subset.

Result-column metadata and public result values continue to come from MyLite's
descriptor planner and result builder, not from SQLite column-name inference.

## Deferred Surface

This slice intentionally does not support:

- `DISTINCT` over joined sources;
- scalar/tableless `SELECT DISTINCT`, `FROM DUAL` distinct, aggregate distinct
  query blocks, grouped distinct, `SQL_CALC_FOUND_ROWS DISTINCT`, `UNION`
  branch changes, subqueries, CTEs, derived tables, or `TABLE`;
- expression, literal, function, user-variable, system-variable, ordinal, or
  string-literal projection items;
- `DISTINCT` over exact decimal, approximate numeric, binary string, BLOB,
  `BIT`, `ENUM`, `SET`, `JSON`, spatial, generated-expression, or unsupported
  descriptor families;
- non-ASCII string collation parity, explicit per-expression collations, or
  binary collation overrides;
- `ORDER BY` keys that are not selected, `FIELD()` order keys, expression order
  keys, ordinal order keys, or broad MySQL representative-row behavior for
  duplicate groups;
- optimizer hints, optimizer row-scan stopping guarantees, temporary-table
  implementation details, protocol-grade metadata beyond existing MyLite
  result descriptors, or privilege semantics.

## Grammar

The parser already admits the required top-level select modifier and projection
forms. This phase relies on the existing independent MyLite grammar shape:

```lemon
select_duplicate_modifier_opt(A) ::= DISTINCT.
select_duplicate_modifier_opt(A) ::= DISTINCTROW.

select_item(A) ::= expression(E) select_item_alias_opt(B).
select_item(A) ::= STAR(T).
select_item(A) ::= qualified_identifier(Q) DOT STAR(T).
```

The runtime-supported subset is narrower:

```lemon
distinct_select(A) ::=
    SELECT DISTINCT distinct_projection FROM single_descriptor_table
    where_opt order_by_selected_columns_opt limit_opt.

distinct_projection(A) ::= distinct_column_item.
distinct_projection(A) ::= distinct_projection COMMA distinct_column_item.
distinct_projection(A) ::= STAR.
distinct_projection(A) ::= qualified_wildcard.

distinct_column_item(A) ::= descriptor_column select_item_alias_opt.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Route `SELECT DISTINCT` / `DISTINCTROW` with a single descriptor table source
   through the existing descriptor `SELECT` planner.
2. Resolve the source with the current selected/default schema policy.
3. Reject reserved `_mylite_*` schemas and table names through the same
   descriptor table resolution guards used by ordinary `SELECT`.
4. Resolve wildcard and explicit projection columns from MyLite catalog
   descriptors. Invisible columns are excluded from wildcard expansion and may
   still be referenced explicitly, following existing projection behavior.
5. Validate every selected column against the supported descriptor families.
6. Resolve predicates and order keys through descriptors. Every order key must
   resolve to one selected descriptor column or selected-column alias, matching
   MySQL 8.4.9's selected-expression restriction for `DISTINCT` query blocks
   while keeping expression order keys deferred.
7. Reject `SQL_CALC_FOUND_ROWS` with `DISTINCT`; existing `FOUND_ROWS()` state
   is not changed by this unsupported combination.

Execution:

1. Generate SQLite SQL from descriptors and stable physical table names.
2. Emit `SELECT DISTINCT` and the descriptor projection list. For nonbinary
   string columns, append `COLLATE "utf8mb4_0900_ai_ci"` to the projected
   expression so SQLite duplicate elimination uses MyLite's registered ASCII
   collation instead of bytewise equality.
3. Quote every generated SQLite identifier.
4. Bind predicate, ordering helper, and limit values through existing prepared
   statement binding paths.
5. Let SQLite perform table scanning, filtering, distinct duplicate removal,
   ordering, and limiting. MyLite does not materialize the full rowset in memory
   for duplicate elimination.
6. Convert SQLite rows into the existing public result object using MyLite
   descriptor metadata. The successful result has result rows and columns,
   `affected_rows == 0`, `warning_count == 0`, and a following `ROW_COUNT()`
   reads `-1`.

## Equality and Ordering

- A duplicate row is removed when every selected value in the row equals the
  corresponding selected value in an earlier duplicate candidate under the
  supported type's baseline equality.
- `NULL` equals `NULL` for duplicate elimination.
- Integer and `YEAR` equality follows their stored canonical integer/text
  baseline.
- `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` equality follows canonical stored
  values for this slice.
- ASCII nonbinary string equality uses the registered `utf8mb4_0900_ai_ci`
  collation subset. It is case-insensitive for ASCII letters and preserves
  MySQL's current `NO PAD`-style trailing-space significance in the baseline
  collation implementation.
- Ordering, when admitted, follows the existing descriptor `ORDER BY` semantics.
  Ties without additional selected order keys do not get a new deterministic
  guarantee.

## Diagnostics

Use existing MyLite diagnostics unless listed otherwise:

- syntax errors and malformed select modifiers: existing parse diagnostics;
- missing default schema, unknown schema, unknown table, reserved MyLite schema
  or table names, unsupported object kind, and unknown projection/predicate/order
  columns: existing descriptor `SELECT` diagnostics;
- scalar/tableless or `DUAL` distinct:
  `SELECT DISTINCT supports only descriptor-backed table reads`;
- joined-source distinct:
  `joined SELECT does not yet support DISTINCT`;
- unsupported projection expression:
  `SELECT supports only descriptor table columns`;
- unsupported selected descriptor family:
  `SELECT DISTINCT supports only integer, YEAR, DATE, TIME, DATETIME, TIMESTAMP, or nonbinary string descriptor columns`;
- unsupported distinct order key:
  `SELECT DISTINCT supports ORDER BY only on selected columns`;
- `SQL_CALC_FOUND_ROWS` with distinct:
  `SQL_CALC_FOUND_ROWS supports only non-distinct descriptor-backed table SELECT`;
- allocation failure: existing out-of-memory diagnostic;
- physical SQLite failure: existing SQLite-to-MyLite execution diagnostic.

## Storage and Architecture

This phase does not change public headers, the `.mylite` preamble, catalog
descriptor schema, descriptor versions, storage records, VFS behavior, or the
SQLite fork. It is a MyLite planner/SQL-generation change on top of public
SQLite prepared statements and the existing registered collation.

Ownership remains:

- public API: unchanged result-set API and execution entry points;
- statement context: unchanged statement lifecycle and diagnostics area;
- parser/AST: existing `DISTINCT` modifier and projection nodes;
- analyzer/planner: resolves table/columns/order/predicate through descriptors
  and validates the supported distinct families;
- catalog: descriptor authority only, no mutation;
- result builder: descriptor-owned metadata and row readback;
- storage/VFS: unchanged file format and physical payload invariants;
- SQLite: physical scan/filter/distinct/order/limit execution with MyLite-built
  SQL, quoted identifiers, bound values, and registered collation.

## Tests

Add a focused runtime test binary for distinct rowsets and update existing
distinct rejection tests that become supported. Coverage must include:

- `DISTINCT` and `DISTINCTROW`;
- multi-column explicit projection;
- wildcard and qualified wildcard projection over visible columns;
- `NULL` duplicate elimination;
- integer, `YEAR`, temporal, and ASCII nonbinary string values;
- case-insensitive ASCII string duplicate elimination;
- aliases and selected-column `ORDER BY`;
- `WHERE`, `LIMIT`, and `OFFSET`;
- close/reopen persistence;
- unsupported scalar/`DUAL`, joined, expression/literal projection,
  unsupported descriptor families, non-selected order keys, and
  `SQL_CALC_FOUND_ROWS DISTINCT`;
- MySQL 8.4.9 expectation script for the user-visible behavior admitted by this
  phase.

## Compatibility Updates

Update `COMPATIBILITY.md` and `docs/compatibility/sql-query-expressions.md` to
describe limited single-table rowset `DISTINCT` support. Do not claim broad
joined, grouped, scalar, expression, full collation, optimizer, or
`SQL_CALC_FOUND_ROWS` distinct behavior.
