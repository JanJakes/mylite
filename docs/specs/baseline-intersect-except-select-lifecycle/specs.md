# Baseline INTERSECT / EXCEPT Select Lifecycle

## Status

This feature specifies a narrow `INTERSECT` / `EXCEPT` query-expression slice
for currently supported `SELECT` statements. It extends the existing
descriptor-safe compound `UNION` lifecycle with two additional set operators
without broadening branch syntax, query-expression precedence handling, or
SQLite pass-through.

This is not full MySQL set-operation support. The slice admits unparenthesized
top-level chains of supported `SELECT` query blocks connected by only
`INTERSECT`, `INTERSECT DISTINCT`, `INTERSECT ALL`, `EXCEPT`,
`EXCEPT DISTINCT`, or `EXCEPT ALL`. The implementation rejects mixed
set-operator chains because MySQL gives `INTERSECT` different precedence than
`UNION` and `EXCEPT`, while MyLite's existing compound AST is a left-to-right
chain.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- Baseline `UNION` lifecycle:
  `docs/specs/baseline-union-select-lifecycle/specs.md`
- Baseline insert-select `UNION` source lifecycle:
  `docs/specs/baseline-insert-select-union-source/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, set operations:
  https://dev.mysql.com/doc/refman/8.4/en/set-operations.html
- MySQL 8.4 Reference Manual, `UNION`:
  https://dev.mysql.com/doc/refman/8.4/en/union.html
- MySQL 8.4 Reference Manual, `INTERSECT`:
  https://dev.mysql.com/doc/refman/8.4/en/intersect.html
- MySQL 8.4 Reference Manual, `EXCEPT`:
  https://dev.mysql.com/doc/refman/8.4/en/except.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_intersect_except_select_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `INTERSECT` and `INTERSECT DISTINCT` return distinct rows from the left query
  expression that also occur in the right query expression.
- `INTERSECT ALL` applies multiset semantics. Duplicate rows are retained only
  up to the count present on both sides.
- `EXCEPT` and `EXCEPT DISTINCT` return distinct rows from the left query
  expression that do not occur in the right query expression.
- `EXCEPT ALL` applies multiset semantics. Duplicate rows from the left side
  are removed only up to the count present on the right side.
- `NULL` equals `NULL` for set-operation duplicate, intersection, and
  difference comparisons.
- Nonbinary character rows use the effective string collation. Under the
  default collation, observed ASCII case variants such as `'a'` and `'A'` are
  equivalent. Binary casts remain bytewise and therefore keep `'a'` and `'A'`
  distinct.
- Result column names come from the first query block.
- A column-count mismatch fails with error `1222`, SQLSTATE `21000`, and
  message `The used SELECT statements have a different number of columns`.
- `ROW_COUNT()` after a successful set-operation result-set statement is `-1`,
  and supported in-range queries leave `@@warning_count == 0`.
- A top-level `ORDER BY` or `LIMIT` after the final query block applies to the
  whole set operation in MySQL, but is deferred for this MyLite slice.
- An unparenthesized query block-local `ORDER BY` or `LIMIT` before a set
  operator is a syntax error in MySQL. Parenthesized query expressions can
  carry branch-local clauses, but parenthesized query expressions are deferred
  here.
- MySQL gives `INTERSECT` higher precedence than `UNION` and `EXCEPT`; `UNION`
  and `EXCEPT` group left to right when parentheses do not override them. This
  slice rejects mixed set operators instead of misrepresenting that precedence.
- MySQL supports `TABLE`, `VALUES`, parenthesized query expressions, global
  ordering/limiting, full type aggregation, full collation semantics, and
  expression-rich query blocks. They are out of scope for this baseline.

## Scope

The implementation must add:

- parser and AST support for compound select terms using `INTERSECT`,
  `INTERSECT DISTINCT`, `INTERSECT ALL`, `EXCEPT`, `EXCEPT DISTINCT`, and
  `EXCEPT ALL`;
- execution for homogeneous top-level `INTERSECT` chains and homogeneous
  top-level `EXCEPT` chains over currently supported standalone `SELECT`
  branches;
- branch execution through the existing MyLite `SELECT` analyzer and runtime
  paths, preserving current scalar, descriptor-backed, aggregate, grouped,
  `WHERE`, alias, and metadata behavior for supported standalone branches;
- the same branch shape restrictions as the current `UNION` slice:
  no branch `ORDER BY`, branch `LIMIT`, select options,
  `SQL_CALC_FOUND_ROWS`, or locking clauses;
- column-count validation after branch execution and before applying the set
  operator;
- full-row comparison where `NULL` equals `NULL`, supported nonbinary string
  outputs use the current ASCII case-insensitive collation subset, and binary
  outputs remain bytewise, with comparison mode determined from every branch
  before the first set operation is applied;
- distinct and multiset semantics for `INTERSECT` and `EXCEPT`;
- result column names and metadata copied from the first query block;
- public result behavior matching existing result-set conventions:
  `affected_rows == 0`, row result set present, and warning count copied from
  statement diagnostics;
- deterministic unsupported diagnostics for mixed set operators and other
  deferred set-operation surfaces;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- mixed `UNION` / `INTERSECT` / `EXCEPT` precedence;
- `INTERSECT` or `EXCEPT` inside `INSERT ... SELECT`, `REPLACE ... SELECT`,
  `CREATE TABLE ... SELECT`, subqueries, `EXISTS`, `IN`, DML assignments, or
  arbitrary expression positions;
- parenthesized query expressions, branch-local clauses inside parentheses,
  or `TABLE` / `VALUES` set-operation operands;
- compound-level `ORDER BY`, compound-level `LIMIT`, alias or ordinal
  resolution for compound ordering, or global locking clauses;
- broad type aggregation across branches, full collation aggregation,
  widened display lengths, coercion metadata, or protocol-grade metadata
  merging;
- streaming set-operation execution through SQLite, temporary SQLite tables for
  duplicate removal, optimizer pushdown, or SQLite fork patches;
- arbitrary SQLite SQL pass-through.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, `ROW_COUNT()` state,
  found-row state, and successful result-set finalization. A compound
  `INTERSECT` or `EXCEPT` statement is one top-level result-set statement.
- Lexer/parser/AST own syntax admission, source spans, and the set operator
  attached to each compound term. Parser code remains independent of runtime,
  catalog, storage, and SQLite.
- Runtime compound execution owns branch sequencing, mixed-operator rejection,
  column-count validation, all-branch row comparison mode selection, row
  comparison, multiset handling, result metadata copying from the first branch,
  and final public result assembly.
- Existing branch analyzers/planners own schema resolution, descriptor
  authority, expression support, predicate planning, generated SQLite SQL,
  parameter binding, and physical execution for each individual `SELECT`.
- Catalog descriptors remain authoritative for descriptor-backed branches.
  `INTERSECT` and `EXCEPT` must not mutate catalog rows, descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- SQLite continues to own physical row storage and branch-local execution for
  existing translated `SELECT` plans. This slice combines already materialized
  MyLite branch results in C memory and applies the same current ASCII
  case-folding rule as MyLite's registered default string collation for
  supported comparisons; it does not require a SQLite fork hook.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Read-only set-operation statements must not write either region.

## Supported SQL Grammar

Supported top-level homogeneous subsets:

```sql
select_statement
INTERSECT [DISTINCT | ALL] select_statement
[INTERSECT [DISTINCT | ALL] select_statement] ...

select_statement
EXCEPT [DISTINCT | ALL] select_statement
[EXCEPT [DISTINCT | ALL] select_statement] ...
```

`select_statement` means the currently supported MyLite standalone `SELECT`
syntax, subject to the compound restrictions in this spec. Omitting the
modifier is equivalent to `DISTINCT`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar shape and is independently
authored for this project:

```lemon
compound_select_statement(A) ::= select_statement(S) set_term_list(T). {
    A = mylite_sql_parser_make_compound_select_statement(state, S, T);
}

set_term_list(A) ::= set_term(T). {
    A = mylite_sql_parser_make_set_term_list(state, T);
}
set_term_list(A) ::= set_term_list(L) set_term(T). {
    A = mylite_sql_parser_append_set_term(state, L, T);
}

set_term(A) ::= UNION(U) set_modifier_opt(M) select_statement(S). {
    A = mylite_sql_parser_make_set_term(
        state, U, MYLITE_SQL_AST_SET_OPERATOR_UNION, M, S);
}
set_term(A) ::= INTERSECT(I) set_modifier_opt(M) select_statement(S). {
    A = mylite_sql_parser_make_set_term(
        state, I, MYLITE_SQL_AST_SET_OPERATOR_INTERSECT, M, S);
}
set_term(A) ::= EXCEPT(E) set_modifier_opt(M) select_statement(S). {
    A = mylite_sql_parser_make_set_term(
        state, E, MYLITE_SQL_AST_SET_OPERATOR_EXCEPT, M, S);
}

set_modifier_opt(A) ::= . {
    A = MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
}
set_modifier_opt(A) ::= DISTINCT. {
    A = MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
}
set_modifier_opt(A) ::= ALL. {
    A = MYLITE_SQL_AST_UNION_MODIFIER_ALL;
}
```

The existing AST node names may continue to say `UNION_TERM` for ABI-local
compatibility with previous internal code, but each term must carry an explicit
set operator so runtime code does not infer the operator from node names.

## Semantics

### Branch Execution

Branches execute from left to right through the existing `SELECT` runtime after
mixed-operator rejection. Branch diagnostics are statement diagnostics for the
one top-level compound statement. If any branch fails, the compound statement
fails, no partial public result is returned, and the first failing branch's
diagnostic is preserved.

After all branches have executed successfully, MyLite derives the row-comparison
mode for each column from every branch expression and every branch result
metadata record before applying the first set operation. This prevents later
binary metadata from changing equality rules after earlier rows have already
been deduplicated or intersected.

Each branch must return the same number of result columns as the first branch.
If a later branch returns a different count, MyLite reports the
MySQL-compatible `1222 / 21000` diagnostic and returns no result.

### Result Metadata

The public compound result uses column labels and available metadata from the
first branch. This matches the observed MySQL label rule and keeps metadata
authority local to the branch that already resolved descriptor columns.

This slice does not merge branch types, display lengths, flags, charsets,
collations, nullability, table names, or origin metadata. If later branch
values exceed first-branch display metadata, the value bytes are still returned
unchanged, but metadata remains first-branch metadata until a broader
type-aggregation feature is specified.

### Row Equality

All supported set-operator comparisons use full-row equality. Every column must
compare equal for two rows to be equal. Two `NULL` cells compare equal. A
`NULL` cell and a non-`NULL` cell compare different.

Non-`NULL` cells compare by byte length and byte content, except columns that
are known to use the current supported nonbinary string collation subset. Those
columns compare by ASCII case-insensitive bytes. If any branch expression or
branch result metadata for a column is binary, the column remains bytewise for
this baseline.

### INTERSECT

For `INTERSECT` and `INTERSECT DISTINCT`, MyLite first treats the accumulated
left result as a distinct row set, then keeps rows that also occur in the right
branch. The right branch only needs to contain at least one equal row.

For `INTERSECT ALL`, MyLite treats the accumulated left result and the right
branch as multisets. A left row is kept only when a still-unmatched equal right
row exists. This preserves the current accumulated-left row order for kept
rows, but SQL callers must not rely on a stable order without a future
compound-level `ORDER BY` feature.

### EXCEPT

For `EXCEPT` and `EXCEPT DISTINCT`, MyLite first treats the accumulated left
result as a distinct row set, then keeps rows that do not occur in the right
branch.

For `EXCEPT ALL`, MyLite treats the accumulated left result and the right
branch as multisets. A left row is removed only when a still-unmatched equal
right row exists. This preserves the current accumulated-left row order for
remaining rows, but SQL callers must not rely on a stable order without a
future compound-level `ORDER BY` feature.

### Operator Chains

Homogeneous chains apply each operator to the accumulated result and the next
right branch in source order. This is sufficient for the admitted
`INTERSECT ... INTERSECT ...` and `EXCEPT ... EXCEPT ...` subsets.

Mixed chains are rejected with a deterministic unsupported diagnostic before
runtime execution. This includes any mix of `UNION`, `INTERSECT`, and
`EXCEPT`. A later feature must add the proper MySQL query-expression
precedence model before admitting mixed chains.

### Branch Clause Restrictions

The implementation may parse a `select_statement` branch that contains
`ORDER BY`, `LIMIT`, or a locking clause because those clauses already belong
to the standalone `SELECT` grammar. Compound execution must reject those branch
shapes for now, rather than silently treating them as global compound clauses.

## Diagnostics

Supported diagnostics:

- parser syntax errors use the existing parse-error path;
- branch column-count mismatch returns `1222 / 21000` with MySQL's set-operation
  column-count message;
- unsupported non-`SELECT` branch shapes report a MyLite unsupported diagnostic;
- branch `SQL_CALC_FOUND_ROWS`, select options, `ORDER BY`, `LIMIT`, or locking
  clauses report deterministic MyLite unsupported diagnostics;
- mixed set operators report a deterministic MyLite unsupported diagnostic;
- `INTERSECT` or `EXCEPT` used as an `INSERT ... SELECT` compound source
  reports a deterministic MyLite unsupported diagnostic;
- branch planner, descriptor, schema, table, expression, conversion,
  allocation, and SQLite failures are reported by the existing branch
  execution paths.

Successful in-range supported statements produce zero warnings and return a
row result set. `affected_rows` remains `0` for the result object and the
session `ROW_COUNT()` value follows existing successful result-set statement
behavior (`-1`).

## Physical SQLite Handling

No generated SQLite set-operation SQL is required for top-level result-set
execution. Each branch runs through the existing MyLite planner and SQLite
execution path for supported standalone `SELECT` statements. MyLite then
combines the materialized branch results in its result builder after deriving
per-column comparison modes from all branch metadata.

The current implementation is intentionally simple and performs O(n*m)
row-comparison work for duplicate, intersection, and difference checks. That is
acceptable for this baseline because the existing `UNION DISTINCT` path already
has the same shape and this feature is correctness-first. A later performance
slice can introduce hash tables or SQLite temporary tables after specifying
type/collation aggregation and memory limits.

This feature does not require public SQLite extension APIs beyond those already
used by branch execution, and it does not require a SQLite fork hook.

## File Format and Persistence

`INTERSECT` and `EXCEPT` are read-only. They must not change `.mylite` preamble
bytes, SQLite payload bytes, catalog rows, descriptor versions, descriptor
caches, catalog generation, or `sqlite_schema_generation`.

Existing persisted table rows inserted before close/reopen remain queryable by
the new set operators after reopen. Independent file-backed handles must see
only their own file state.

## Test Plan

Add MySQL-runtime expectation coverage and C runtime/parser coverage for:

- scalar no-source and `FROM DUAL` `INTERSECT`, `INTERSECT DISTINCT`,
  `INTERSECT ALL`, `EXCEPT`, `EXCEPT DISTINCT`, and `EXCEPT ALL`;
- duplicate, multiset, `NULL`, default nonbinary string collation, and binary
  bytewise behavior;
- descriptor-backed table branches with integer and string columns;
- result labels and public result metadata inherited from the first branch;
- column-count mismatch diagnostics;
- branch `ORDER BY`, branch `LIMIT`, select options, `SQL_CALC_FOUND_ROWS`, and
  locking-clause unsupported diagnostics;
- mixed operator chains rejected deterministically;
- global `ORDER BY` and global `LIMIT` documented as accepted by MySQL but
  deferred in MyLite;
- `INSERT ... SELECT` with `INTERSECT` or `EXCEPT` rejected deterministically;
- successful result state: row result set, `affected_rows == 0`,
  `ROW_COUNT() == -1`, `warning_count == 0`, and no mutation of table rows;
- reopen persistence and independent file-backed handles;
- existing lexer, parser, `UNION`, `INSERT ... SELECT UNION`, result metadata,
  descriptor SELECT, runtime handle, VFS, storage, and full workflow tests.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/sql-query-expressions.md` only for the exact supported
subset. Keep full query-expression precedence, parenthesized set expressions,
global ordering/limiting, `TABLE`/`VALUES` operands, nested set operations, and
full type/collation aggregation documented as deferred.
