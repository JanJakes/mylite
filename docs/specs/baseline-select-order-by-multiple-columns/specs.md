# Baseline SELECT ORDER BY Multiple Columns

## Status

This feature specifies a narrow expansion of descriptor-backed `SELECT`
ordering:

```sql
SELECT select_list FROM table_source
  [WHERE predicate]
  ORDER BY order_key [ASC | DESC], order_key [ASC | DESC] ...
  [LIMIT ...]
```

It builds on the existing descriptor-driven table `SELECT`, row-scalar table
select, joined-source select, alias-aware single-key `ORDER BY`, string and
temporal ordering, and `LIMIT`/`OFFSET` slices. The initial implementation
supports multiple descriptor column order keys for `SELECT` statements only.
Single-table `UPDATE`, single-table `DELETE`, aggregate-local
`GROUP_CONCAT(... ORDER BY ...)`, and grouped-aggregate `ORDER BY` keep their
current one-key limits until separate write or aggregate slices specify and
test wider behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline qualified columns:
  `docs/specs/baseline-select-qualified-columns/specs.md`
- Select item alias behavior:
  `docs/specs/baseline-select-item-alias/specs.md`
- Inner and left join select envelopes:
  `docs/specs/baseline-inner-join-select/specs.md`,
  `docs/specs/baseline-left-join-select/specs.md`
- String ordering:
  `docs/specs/baseline-string-order-lifecycle/specs.md`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, sorting rows:
  https://dev.mysql.com/doc/refman/8.4/en/sorting-rows.html
- MySQL 8.4 Reference Manual, `NULL` values:
  https://dev.mysql.com/doc/refman/8.4/en/null-values.html
- MySQL 8.4 Reference Manual, alias handling:
  https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against local MySQL 8.4.9 using
`/opt/homebrew/opt/mysql@8.4/bin/mysql` and
`/tmp/mylite-mysql-849.jsgoZE/mysql.sock`:

- `ORDER BY g, n DESC, id` is valid and applies keys from left to right. Later
  keys break ties from earlier keys.
- Omitted direction means ascending for each key independently.
- `ASC` and `DESC` may be mixed in one `ORDER BY` list.
- `NULL` values sort before non-`NULL` values for ascending keys and after
  non-`NULL` values for descending keys.
- `ORDER BY g ASC, id DESC LIMIT 3` applies ordering before limiting.
- `ORDER BY missing, id` fails with `1054 / 42S22`,
  `Unknown column 'missing' in 'order clause'`.
- Unqualified selected aliases are considered for `ORDER BY`. An unqualified
  alias can shadow a source column name. Duplicate matching aliases produce
  `1052 / 23000`, `Column 'x' in order clause is ambiguous`.
- MySQL accepts some wider forms such as ordinals, expressions, multiple keys
  with `DISTINCT` when each expression is compatible with the distinct select
  list, table-qualified columns, and aggregate result ordering. These remain
  outside this slice except where the existing single-key select envelope
  already admits qualified descriptor columns.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_select_order_by_multiple_columns_expectations.sh`.

## Scope

The implementation must add:

- parser and AST support for a comma-separated `ORDER BY` item list on
  `SELECT` statements;
- descriptor-driven multiple `ORDER BY` keys for ordinary table-backed
  `SELECT` statements, table-backed row-scalar selects, and the existing
  descriptor-backed table select source used by `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, and `REPLACE ... SELECT`;
- one or more order keys, each a currently supported descriptor column
  reference;
- unqualified order keys, selected item aliases, and currently supported
  source-qualified order keys for single-source and joined-source selects;
- optional per-key `ASC` and `DESC`, with omitted direction meaning ascending;
- current integer, `BIT`, `YEAR`, `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, and
  ASCII nonbinary string descriptor order families;
- existing descriptor column resolution and current case-insensitive alias
  matching behavior;
- existing selected/default schema and source resolution behavior;
- composition with the existing baseline `WHERE` subset and existing select
  `LIMIT`/`OFFSET` forms;
- generated SQLite SQL with one quoted descriptor expression per order key; and
- deterministic diagnostics for unknown names, unsupported order key types, and
  deferred wider forms.

Existing one-key behavior for `UPDATE`, `DELETE`, aggregate-local ordering,
grouped aggregate ordering, and `SELECT DISTINCT` remains intentionally narrow.

## Non-Goals

This feature must not implement:

- expression order keys, ordinal order keys, string-literal order constants,
  function order keys, arithmetic order keys, parameters, arbitrary expression
  evaluation, collations, or `ORDER BY RAND()`;
- multiple `ORDER BY` keys for single-table `UPDATE`, single-table `DELETE`,
  joined `UPDATE`/`DELETE`, `GROUP_CONCAT`, or grouped aggregate result
  ordering;
- a broader `DISTINCT` order-key model beyond the existing one-column distinct
  slice;
- ordering for descriptor families that are still unsupported for one-key
  ordering, including decimal, approximate, `ENUM`, `SET`, JSON, and binary
  string order keys outside `BIT`;
- complete UCA 9.0 collation parity for string sorting;
- optimizer-index selection, filesort emulation, materialized temp tables, or
  MyLite-side row sorting; or
- SQLite fork patches, custom SQLite virtual tables, or new public API.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public argument
  validation, result-handle ownership, statement row-count state, and cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful ordered selects remain row-result statements with the
  existing result API behavior and no catalog mutation.
- Lexer/parser/AST own syntax admission and source spans for comma-separated
  select order lists. They remain independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code resolves each order key against selected aliases and
  MyLite catalog descriptors, validates the descriptor type family, and stores
  a planned order item list.
- The catalog remains authoritative for schema, table, source alias, and column
  descriptors. SQLite metadata is not consulted for logical names.
- Runtime SQL generation lowers only validated descriptor columns to SQLite
  physical SQL, quotes every generated identifier, and binds existing predicate
  and limit values through prepared statements.
- SQLite owns physical row scanning and sorting for the generated SQL. MyLite
  does not materialize the row set to sort it in memory.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Ordered selects must not modify catalog rows, user rows, descriptor caches,
  `catalog_generation`, `sqlite_schema_generation`, or the file preamble.

## Supported SQL Grammar

Supported subset:

```sql
select_statement ::=
    SELECT select_modifier_opt select_list from_clause
      where_clause_opt group_by_clause_opt having_clause_opt
      select_order_clause_opt limit_clause_opt select_locking_clause_opt

select_order_clause_opt ::= .
select_order_clause_opt ::= ORDER BY select_order_item_list

select_order_item_list ::= select_order_item
select_order_item_list ::= select_order_item_list COMMA select_order_item

select_order_item ::= qualified_identifier order_direction_opt

order_direction_opt ::= .
order_direction_opt ::= ASC
order_direction_opt ::= DESC
```

`qualified_identifier` is the existing MyLite identifier reference node. This
feature does not admit ordinal integer literals, arbitrary expressions, string
literals, function calls, or parameters as order items.

The shared one-key `order_clause_opt` used by `UPDATE`, `DELETE`, and
aggregate-local ordering remains unchanged.

## Resolution Semantics

For each order item, MyLite resolves names in this order:

1. If the order key is an unqualified identifier, compare it against selected
   item aliases using the current case-insensitive alias matching rule. A single
   matching alias resolves to that selected descriptor column and source index.
   Multiple matching aliases are ambiguous.
2. If no alias resolves, resolve the key as a descriptor column reference using
   the current source context.
3. For a single unaliased source, unqualified, table-qualified, and
   schema-table-qualified descriptor references follow the existing qualified
   column policy.
4. For joined sources, the existing joined-source qualifier policy applies.
   Ambiguous unqualified descriptor names remain errors.

Schema-qualified and unqualified table/source resolution is unchanged from the
current select implementation. Missing default schema, unknown schema, unknown
table, and reserved `_mylite_*` logical names keep the existing diagnostics.

Current descriptor catalog matching remains MyLite's existing identifier policy:
logical descriptors are authoritative and compared through the current
catalog/source resolution helpers. This feature does not change identifier
case-sensitivity or collation rules.

## Ordering Semantics

Ordering is lexicographic by order item:

- key 1 is compared first;
- key 2 compares only rows tied on key 1;
- later keys compare only rows tied on all earlier keys;
- omitted direction is `ASC`;
- `ASC` sorts `NULL` before non-`NULL`;
- `DESC` sorts `NULL` after non-`NULL`;
- duplicate values across all order keys do not get a MyLite-defined
  additional tie-breaker; their relative order is not part of this slice's
  compatibility contract;
- `ORDER BY` without `LIMIT` has no extra visible effect beyond row order;
- `ORDER BY` with `LIMIT` uses the ordered row set before applying row count and
  offset.

MyLite delegates the validated order list to SQLite's `ORDER BY` over physical
columns. SQLite's default `NULL` placement matches the verified MySQL behavior
for admitted ascending and descending keys, and existing registered MyLite
collations are used for admitted ASCII nonbinary string descriptor order keys.

## Generated SQLite Shape

For an ordinary single-source select, the generated shape is:

```sql
SELECT "selected_column", ...
FROM "_mylite_user_table_<table_id>"
WHERE ...
ORDER BY "order_col_1" ASC, "order_col_2" DESC, ...
LIMIT ? OFFSET ?
```

For joined or alias-required plans, the generated order expressions are source
qualified using MyLite's internal source aliases:

```sql
ORDER BY "_mylite_s0"."left_key" ASC, "_mylite_s1"."right_key" DESC
```

Every generated identifier is quoted. Predicate, limit, and offset values
remain bound parameters. Order keys themselves are descriptor-selected
identifiers, never interpolated literals.

No SQLite fork patch is needed. This is a MyLite wrapper/translation feature
using standard SQLite SQL generated from MyLite descriptors.

## Diagnostics

This feature preserves existing diagnostics where they already exist:

- syntax errors for unsupported order item grammar such as ordinals,
  expressions, string literals, functions, parameters, and trailing commas;
- `1054 / 42S22` for unknown order columns where the current descriptor
  resolver can produce MySQL-compatible unknown-column diagnostics;
- ambiguity diagnostics for ambiguous selected aliases or ambiguous joined
  unqualified descriptor columns;
- current MyLite unsupported-feature diagnostics for unsupported order
  descriptor families;
- current default-schema, unknown-schema, unknown-table, reserved-name, and
  unsupported-object diagnostics;
- allocation failure returns `MYLITE_NOMEM`; and
- physical SQLite failures surface through the existing execution diagnostic
  path.

For deferred multi-key contexts:

- `SELECT DISTINCT` with more than one order key returns a deterministic
  MyLite unsupported diagnostic;
- grouped aggregate `ORDER BY` with more than one key returns a deterministic
  MyLite unsupported diagnostic;
- `UPDATE`, `DELETE`, and aggregate-local order grammars remain one-key
  grammars, so comma-separated keys in those contexts keep their existing
  syntax-error behavior.

Supported in-range statements report warning count `0`, return row results
through the existing public result conventions, and leave affected-row behavior
unchanged for row-result statements.

## Performance And Storage

This feature should stay close to SQLite's execution path:

- MyLite resolves and validates order metadata before execution;
- MyLite does not copy rows into an intermediate C array for sorting;
- SQLite receives a standard `ORDER BY` list and can use its own planner,
  indexes, temp sorting, or scan strategy;
- MyLite does not add indexes, helper tables, triggers, or file-format changes;
  and
- `.mylite` file preamble and shifted SQLite payload invariants remain
  untouched.

## Test Plan

Fast C tests under `packages/libmylite/tests/` must cover:

- parser acceptance of one, two, and three select order keys;
- parser rejection of ordinals, expressions, functions, parameters, and
  trailing commas;
- default ascending, explicit `ASC`, explicit `DESC`, and mixed directions;
- nullable integer ordering and `NULL` placement for ascending and descending
  first and later keys;
- duplicate first-key values broken by later keys;
- ties across all keys without overclaiming tie order;
- `ORDER BY ... LIMIT` and `ORDER BY ... LIMIT ... OFFSET`;
- selected alias resolution and duplicate alias ambiguity;
- unqualified and source-qualified order keys for single-source and joined
  selects;
- unknown first and later order columns;
- unsupported order families on later keys;
- table-backed row-scalar select ordering;
- `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and `REPLACE ... SELECT`
  source ordering when they reuse the descriptor-backed table select plan;
- `SELECT DISTINCT` multi-key rejection;
- grouped aggregate multi-key rejection;
- existing `UPDATE`, `DELETE`, and `GROUP_CONCAT` comma-order rejections;
- reopen persistence and independent-handle behavior through ordered reads; and
- no catalog generation, SQLite schema generation, or preamble mutation from
  successful ordered selects.

The MySQL expectation script must verify the user-visible ordering behavior,
`NULL` placement, limit interaction, alias ambiguity, and unknown-column
diagnostics against MySQL 8.4.9.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` `ORDER BY` notes for the exact multi-key `SELECT` subset;
- `docs/compatibility/sql-query-expressions.md` `ORDER BY` notes to remove the
  blanket multiple-key exclusion for descriptor-backed selects; and
- related DML docs only if they need to clarify that `UPDATE` and `DELETE`
  remain one-key order contexts.

Do not overclaim full MySQL ordering, expression ordering, collations, DML
write ordering, aggregate-local multi-key ordering, or optimizer behavior.
