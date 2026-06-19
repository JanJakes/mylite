# Baseline Multi-Aggregate SELECT

## Scope

Extend MyLite's current ungrouped aggregate envelope from one aggregate select
item to multiple aggregate select items in the same `SELECT` list.

This slice covers:

- no-source and `DUAL` aggregate rows for currently supported no-source
  aggregate forms;
- one descriptor-backed table source with optional source alias and index hints;
- current baseline `WHERE`;
- current baseline `LIMIT`;
- select-item aliases;
- mixed supported aggregate families in one result row:
  - `COUNT(*)`
  - `COUNT(column)`
  - `COUNT(literal)`
  - `COUNT(DISTINCT column)`
  - `MIN(expr)` / `MAX(expr)`
  - `SUM(expr)` / `AVG(expr)`
  - `BIT_AND(expr)` / `BIT_OR(expr)` / `BIT_XOR(expr)`
  - `GROUP_CONCAT(...)`
  - `STD()` / `STDDEV()` / `STDDEV_POP()` / `STDDEV_SAMP()`
  - `VAR_POP()` / `VAR_SAMP()` / `VARIANCE()`

The slice does not add executable aggregate windows, `DISTINCT` for
`SUM()`/`AVG()`/`GROUP_CONCAT()`, broad expression aggregation outside the
current row-scalar aggregate argument envelope, derived-source mixed
aggregates, joins for ungrouped mixed aggregates, `HAVING` without `GROUP BY`,
or full MySQL grouping semantics.

## Compatibility Authority

Official MySQL 8.4 documentation defines aggregate functions as operating over
sets of values, notes that aggregates ignore `NULL` values unless stated
otherwise, and states that a statement with aggregate functions and no
`GROUP BY` clause is equivalent to grouping all rows together.

Reference: <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>

Runtime probes for this slice are executed against `mysql:8.4.9` in the
`mylite-mysql-849` container. Observed behavior that defines the baseline:

- A mixed aggregate select list without `GROUP BY` returns one result row for
  the whole qualifying row set.
- `COUNT(*)` returns the number of qualifying rows.
- `COUNT(column)` returns the number of qualifying non-`NULL` values.
- `COUNT(NULL)` returns `0`.
- Empty qualifying row sets still return one aggregate row; `COUNT()` returns
  `0`, neutral bitwise aggregates return their neutral values, and nullable
  numeric/string aggregates return `NULL`.
- `LIMIT 0` suppresses the single aggregate row; an offset beyond the single
  aggregate row also suppresses it.
- Tableless aggregate select lists return one row for supported expressions.

## MyLite Syntax

No grammar changes are required. The parser already produces individual
aggregate AST nodes for each supported aggregate function. The runtime planner
must accept a select list where every select item is one supported aggregate
projection in the current ungrouped aggregate envelope.

Independent Lemon-shape snippet:

```lemon
select_statement ::= SELECT select_modifier_opt aggregate_select_list from_opt where_opt limit_opt.
aggregate_select_list ::= aggregate_select_item.
aggregate_select_list ::= aggregate_select_list COMMA aggregate_select_item.
aggregate_select_item ::= aggregate_expression alias_opt.
```

## Runtime Semantics

MyLite plans a multi-aggregate statement as one SQLite aggregate query so the
underlying table is scanned once. The plan has:

- one optional descriptor-backed source;
- one optional predicate;
- one optional limit;
- an ordered list of aggregate result items.

Each result item owns the existing per-aggregate argument plan: descriptor
column, row-scalar argument, `COUNT()` literal, `GROUP_CONCAT()` ordering and
separator, optional select-item alias, and current aggregate-projection
arithmetic where already supported.

`AVG()` remains a MyLite-formatted result: SQLite computes `SUM(arg)` and
`COUNT(arg)` internally, and MyLite formats the single MySQL-facing result
cell. The multi-aggregate row reader maps variable-width internal SQLite
columns back to one MyLite result cell per select item.

No SQLite fork hook is required. This is a MyLite planner, SQL-rendering, and
result-mapping extension over public SQLite aggregate execution.

## Diagnostics

Unsupported mixed aggregate statements must fail before catalog mutation or
SQLite execution with deterministic MyLite diagnostics:

- mixed aggregate select lists must contain only supported aggregate
  projections;
- one-table mixed aggregates support only descriptor-backed table sources;
- optional clauses remain limited to the current `WHERE`, `LIMIT`, and
  `SELECT ... INTO` envelope;
- unsupported aggregate arguments continue to use each aggregate family's
  existing diagnostics.

## Tests

The MySQL expectation script records:

- mixed count/numeric/bitwise/statistical/string aggregate results;
- alias-compatible result values;
- `WHERE` filtering;
- empty qualifying row-set behavior;
- tableless aggregate rows;
- `LIMIT` suppression of the single aggregate row.

The C runtime test covers:

- result column names and aliases;
- mixed one-table aggregate results;
- mixed tableless aggregate results;
- `WHERE` and `LIMIT`;
- current diagnostic boundaries for scalar/aggregate mixing and unsupported
  sources.
