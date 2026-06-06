# Baseline Derived JOIN SELECT

## Status

Implemented a narrow descriptor-backed derived-table join slice for plain
`SELECT`. The motivating shape is WordPress user queries that left join a base
table to a grouped post-count derived table and order by the derived aggregate
alias.

## References

- MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4 Reference Manual, join syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/join.html>
- MySQL 8.4 Reference Manual, derived tables:
  <https://dev.mysql.com/doc/refman/8.4/en/derived-tables.html>

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 runtime behavior, and existing MyLite
architecture. It does not copy MySQL, MariaDB, Percona, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Captured by
`packages/libmylite/tests/mysql_baseline_derived_join_select_expectations.sh`:

- A derived table in a joined `FROM` source requires an alias.
- The derived table exposes selected output names to the outer query.
- `COUNT(*) AS post_count` in a grouped derived table can be referenced by the
  outer query as either `post_count` or `p.post_count`.
- A `LEFT OUTER JOIN` from a base table to the derived grouped source preserves
  base rows and evaluates the outer `ORDER BY post_count ASC` after the join.
- The WordPress-shaped predicate
  `WHERE ((post_type = 'post' AND (post_status = 'publish')))` inside the
  derived grouped source filters rows before grouping.

## Scope

In scope:

- plain descriptor-backed `SELECT` joins where a source is
  `(SELECT ... GROUP BY ...) alias`;
- required derived-table aliases in `alias` and `AS alias` forms;
- derived grouped sources planned through the existing grouped aggregate path;
- direct descriptor group projections from the derived source;
- identifier aliases on grouped derived output columns;
- one-column grouped aggregate outputs, including `COUNT(*) AS alias`, exposed
  through synthetic source descriptors;
- generated SQLite with stable physical table names and generated source
  aliases, without materializing the derived table in MyLite memory;
- existing joined `ON`, outer `WHERE`, `ORDER BY`, and result readback subsets.

Out of scope:

- CTEs, lateral derived tables, recursive derived sources, derived tables on
  unsupported DML paths, and materialized MyLite-owned temporary rowsets;
- derived output aliases that are string literals or exceed descriptor-name
  capacity;
- unaliased grouped aggregate outputs in derived-table source descriptors;
- `AVG()` and `GROUP_CONCAT()` as derived grouped outputs, because their current
  internal SQLite projection shape is not one outer-visible column;
- arbitrary derived-table projection expressions, windows, nested grouping
  features, optimizer hints, merge/materialization optimizer equivalence,
  privileges, locking, and protocol-grade origin metadata.

## Grammar

The parser already admits derived table sources. The target Lemon-syntax subset
for this slice is:

```lemon
table_source ::= table_name table_alias_opt table_index_hints_opt.
table_source ::= LPAREN select_statement RPAREN derived_table_alias.

derived_table_alias ::= identifier.
derived_table_alias ::= AS identifier.

joined_table_source ::= table_source join_operator table_source join_condition_opt.
joined_table_source ::= joined_table_source join_operator table_source join_condition_opt.
```

The planner, not the grammar, narrows this feature to descriptor-backed
currently supported `SELECT` and grouped aggregate source plans.

## Architecture

- Public API: unchanged. The feature enters through `mylite_execute()` and
  returns existing `mylite_result` objects.
- Parser/AST: existing `MYLITE_SQL_AST_FROM_DERIVED` nodes are reused.
- Planner: plain joined `SELECT` now dispatches derived source nodes to the
  derived-source planner instead of forcing every source through base-table
  resolution. Non-grouped derived sources keep using `planned_select`; grouped
  derived sources use `planned_grouped_aggregate`.
- Source descriptors: grouped derived outputs are copied into synthetic source
  descriptors. Direct group projections keep their descriptor type. Supported
  aggregate outputs use MySQL-shaped integer descriptor metadata and the
  identifier alias as the exposed column name.
- SQL generation: derived grouped sources render as nested SQLite `SELECT`
  statements with output aliases only when used as a derived source.
- Parameter binding: nested grouped predicates, grouping expressions, aggregate
  arguments, order keys, and limits bind through the same parameter index stream
  as other nested select sources.
- SQLite: no fork patch or SQLite-side compatibility logic is introduced.

## Compatibility Notes

This is not a general derived-table implementation. It is deliberately sized to
cover the WordPress `orderby => post_count` query shape while preserving the
existing MyLite descriptor-first architecture. Unsupported derived shapes return
MyLite diagnostics rather than falling through to arbitrary SQLite SQL.
