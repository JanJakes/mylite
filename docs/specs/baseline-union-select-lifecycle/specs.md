# Baseline Union Select Lifecycle

## Status

This feature specifies a narrow `UNION` / `UNION ALL` query-expression slice
for currently supported `SELECT` statements. It builds on `mylite_execute()`,
statement context, the MyLite SQL parser, existing scalar and descriptor-backed
`SELECT` execution, descriptor-owned row storage, and current result metadata.

This is not full MySQL set-operation support. The slice admits unparenthesized
chains of `SELECT` query blocks connected by `UNION`, `UNION DISTINCT`, or
`UNION ALL`. The individual query blocks are delegated to the existing MyLite
`SELECT` implementation, with additional compound-statement restrictions where
MySQL set-expression behavior is not yet implemented.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline insert select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `UNION`:
  https://dev.mysql.com/doc/refman/8.4/en/union.html
- MySQL 8.4 Reference Manual, set operations:
  https://dev.mysql.com/doc/refman/8.4/en/set-operations.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_union_select_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `UNION` and `UNION DISTINCT` remove duplicate result rows.
- `UNION ALL` preserves duplicate result rows.
- Duplicate removal uses the effective nonbinary string collation for supported
  scalar character-cast and descriptor string-column outputs. With the current
  MyLite collation surface, observed ASCII case variants such as `'a'` and
  `'A'` are duplicate-equivalent under the default collation, while binary
  casts and mixed character/binary columns remain bytewise distinct.
- In a mixed chain, a distinct `UNION` removes duplicates from the accumulated
  result to its left and the right query block for that operator; later
  `UNION ALL` terms may add duplicates again.
- Result column names come from the first query block.
- A column-count mismatch fails with error `1222`, SQLSTATE `21000`, and
  message `The used SELECT statements have a different number of columns`.
- `ROW_COUNT()` after a successful `UNION` result-set statement is `-1`, and
  supported in-range queries leave `@@warning_count == 0`.
- A top-level `ORDER BY` or `LIMIT` after the final query block applies to the
  whole set operation in MySQL, but is deferred for this MyLite slice.
- An unparenthesized query block-local `ORDER BY` before a set operator is a
  syntax error in MySQL. Parenthesized query expressions can carry branch-local
  `ORDER BY` / `LIMIT`, but parenthesized query expressions are deferred here.
- MySQL supports `TABLE`, `VALUES`, `INTERSECT`, `EXCEPT`, parenthesized query
  expressions, global ordering/limiting, full type aggregation, collation
  semantics, and expression-rich query blocks. They are out of scope for this
  baseline.

## Scope

The implementation must add:

- parser and AST support for a top-level compound select statement made from
  two or more existing `SELECT` query blocks connected by `UNION`,
  `UNION DISTINCT`, or `UNION ALL`;
- support for chains longer than two terms;
- execution of each branch through the existing MyLite `SELECT` analyzer and
  runtime paths, preserving current scalar, descriptor-backed, aggregate,
  grouped, `WHERE`, alias, and metadata behavior for supported standalone
  branches;
- column-count validation after branch execution and before appending branch
  rows to the public result;
- MySQL-compatible duplicate removal for `UNION` / `UNION DISTINCT` using
  full-row equality where `NULL` equals `NULL`, nonbinary descriptor string
  columns and supported scalar nonbinary string outputs use the current MyLite
  case-insensitive ASCII collation, and binary or explicitly mixed
  character/binary outputs remain bytewise;
- duplicate preservation for `UNION ALL`;
- mixed-chain behavior where a distinct operator deduplicates the accumulated
  output and the current right branch, while later `UNION ALL` terms append
  rows without deduplication;
- result column names and metadata copied from the first query block;
- public result behavior matching existing result-set conventions:
  `affected_rows == 0`, row result set present, and warning count copied from
  statement diagnostics;
- `ROW_COUNT()` session behavior inherited from the existing top-level
  result-set completion path;
- deterministic unsupported diagnostics for set-operation surfaces deferred by
  this slice;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `INTERSECT`, `EXCEPT`, `TABLE`, standalone `VALUES`, CTEs, derived tables, or
  parenthesized query expressions;
- global compound `ORDER BY`, global compound `LIMIT`, branch-local
  `ORDER BY`/`LIMIT`/locking clauses inside parentheses, or alias/ordinal
  resolution for compound-level ordering;
- `UNION` inside `INSERT ... SELECT`, `REPLACE ... SELECT`,
  `CREATE TABLE ... SELECT`, subqueries, `EXISTS`, `IN`, DML assignments, or
  arbitrary expression positions;
- broad type aggregation across branches, full collation aggregation beyond the
  current nonbinary ASCII string-collation surface, coercion metadata, widened
  display lengths, or protocol-grade metadata merging;
- streaming set-operation execution through SQLite, temporary SQLite tables for
  duplicate removal, optimizer pushdown, or SQLite fork patches;
- arbitrary SQLite SQL pass-through.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, `ROW_COUNT()` state,
  found-row state, and successful result-set finalization. A compound `UNION`
  statement is one top-level result-set statement.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Runtime compound execution owns branch sequencing, column-count validation,
  duplicate handling, current nonbinary string-collation tracking for duplicate
  checks, result metadata copying from the first branch, and final public result
  assembly.
- Existing branch analyzers/planners own schema resolution, descriptor
  authority, expression support, predicate planning, generated SQLite SQL,
  parameter binding, and physical execution for each individual `SELECT`.
- Catalog descriptors remain authoritative for descriptor-backed branches.
  `UNION` must not mutate catalog rows, descriptor versions, descriptor caches,
  catalog generation, or `sqlite_schema_generation`.
- SQLite continues to own physical row storage and branch-local execution for
  the existing translated `SELECT` plans. This slice combines already
  materialized MyLite branch results in C memory and applies the same current
  ASCII case-folding rule as MyLite's registered default string collation for
  supported duplicate checks; it does not require a SQLite fork hook.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Read-only `UNION` statements must not write either region.

## Supported SQL Grammar

Supported top-level subset:

```sql
select_statement
UNION [DISTINCT | ALL] select_statement
[UNION [DISTINCT | ALL] select_statement] ...
```

`select_statement` means the currently supported MyLite standalone `SELECT`
syntax, subject to the compound restrictions in this spec. `UNION` without a
modifier is equivalent to `UNION DISTINCT`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar shape and is independently
authored for this project:

```lemon
statement(A) ::= compound_select_statement(B). {
    A = B;
}

compound_select_statement(A) ::= select_statement(S) union_term_list(T). {
    A = mylite_sql_parser_make_compound_select_statement(state, S, T);
}

union_term_list(A) ::= union_term(T). {
    A = mylite_sql_parser_make_union_term_list(state, T);
}
union_term_list(A) ::= union_term_list(L) union_term(T). {
    A = mylite_sql_parser_append_union_term(state, L, T);
}

union_term(A) ::= UNION(U) union_modifier_opt(M) select_statement(S). {
    A = mylite_sql_parser_make_union_term(state, U, M, S);
}

union_modifier_opt(A) ::= . {
    A = MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
}
union_modifier_opt(A) ::= DISTINCT. {
    A = MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
}
union_modifier_opt(A) ::= ALL. {
    A = MYLITE_SQL_AST_UNION_MODIFIER_ALL;
}
```

The implementation may parse a `select_statement` branch that contains
`ORDER BY`, `LIMIT`, or a locking clause because those clauses already belong
to the standalone `SELECT` grammar. Compound execution must reject those branch
shapes for now, rather than silently treating them as global compound clauses.

## Semantics

### Branch Execution

Branches execute from left to right through the existing `SELECT` runtime.
Branch diagnostics are statement diagnostics for the one top-level compound
statement. If any branch fails, the compound statement fails, no partial public
result is returned, and the first failing branch's diagnostic is preserved.

Each branch must return the same number of result columns as the first branch.
If a later branch returns a different count, MyLite reports the MySQL-compatible
`1222 / 21000` diagnostic and returns no result.

### Result Metadata

The public compound result uses column labels and available metadata from the
first branch. This matches the observed MySQL label rule and keeps metadata
authority local to the branch that already resolved descriptor columns.

This slice does not merge branch types, display lengths, flags, charsets,
collations, nullability, table names, or origin metadata. If later branch
values exceed first-branch display metadata, the value bytes are still returned
unchanged, but metadata remains first-branch metadata until a broader
type-aggregation feature is specified.

### Duplicate Handling

For `UNION` and `UNION DISTINCT`, duplicate detection compares every column in
the row tuple. Two `NULL` cells compare equal. A `NULL` cell and a non-`NULL`
cell compare different. Non-`NULL` cells compare by byte length and byte
contents as exposed by the branch result.

For `UNION ALL`, rows from the right branch are appended without duplicate
checking.

For mixed chains, the running output is the accumulated left side:

- A distinct term appends rows from the right branch only if no equal row is
  already present in the running output, and also leaves the running output
  deduplicated.
- An `ALL` term appends every right-branch row.

The result order for supported deterministic tests follows execution order:
rows kept from earlier branches remain before rows kept from later branches.
MySQL does not guarantee a broader unordered-set row order without a global
`ORDER BY`; this slice must not document a broader ordering guarantee.

### Unsupported Clauses

This slice rejects:

- any compound branch with `ORDER BY` or `LIMIT`, because MySQL's
  branch-local form requires parenthesized query expressions and MyLite does
  not yet support compound-level ordering/limiting;
- any compound branch with a locking clause;
- `SQL_CALC_FOUND_ROWS` in any branch;
- `HIGH_PRIORITY`, `STRAIGHT_JOIN`, `SQL_SMALL_RESULT`, `SQL_BIG_RESULT`,
  `SQL_BUFFER_RESULT`, or `SQL_NO_CACHE` in any branch until modifier
  interaction for compound statements is specified;
- `UNION` inside statement forms that currently accept only a plain
  `select_statement`.

Plain branch `DISTINCT` / `DISTINCTROW` is still delegated to the standalone
branch implementation, so its existing limitations remain in force.

## Diagnostics

- Syntax errors use the existing parser diagnostic path.
- Unsupported compound query-expression grammar returns a deterministic
  MyLite unsupported diagnostic naming the unsupported clause or shape.
- Branch-local `ORDER BY`, `LIMIT`, locking clauses, or select options return
  deterministic unsupported diagnostics before branch execution.
- Branch execution failures use the existing branch diagnostic: missing default
  schema, unknown schema, unknown table, unknown column, unsupported projection,
  unsupported predicate, unsupported aggregate, allocation failure, physical
  SQLite failure, or any existing branch-specific error.
- Column-count mismatch returns MySQL-compatible `1222 / 21000` with message
  `The used SELECT statements have a different number of columns`.
- Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
  diagnostic.
- Public API misuse is unchanged because no public surface changes.

Supported in-range compound queries produce no warnings unless an admitted
branch shape already produces a warning. The initial supported branch set
rejects branch modifiers that would produce statement-modifier warnings.

## Performance And Storage

Existing MyLite `SELECT` execution already materializes public result rows in a
`mylite_result`. This slice executes each branch into an internal result and
copies row cells into the final result. `UNION ALL` adds only append-copy work
on top of the branch results. `UNION DISTINCT` performs duplicate checks over
the accumulated result rows.

This is acceptable for the baseline because it does not add a less efficient
path than the current public result materialization model. A later optimizer
slice can translate supported descriptor-only compounds into SQLite compound
queries or use SQLite temporary storage for deduplication. That future work
must preserve MyLite descriptor authority and MySQL metadata/diagnostics.

`UNION` is read-only. It must not change `.mylite` preamble bytes, SQLite
schema, user rows, catalog descriptors, session defaults, or transaction state
except for ordinary result-set diagnostics and `ROW_COUNT()` / `FOUND_ROWS()`
session accounting.

## Test Plan

Fast C tests must cover:

- parser AST shape for two-term and three-term `UNION`, `UNION DISTINCT`, and
  `UNION ALL`;
- scalar no-source and `FROM DUAL` `UNION` / `UNION ALL`;
- descriptor-backed table branches over integer, `NULL`, and supported string
  values already handled by current `SELECT`;
- first-branch column labels and metadata preservation;
- duplicate removal including `NULL` rows and duplicate multi-column rows;
- duplicate preservation for `UNION ALL`;
- mixed `ALL` and distinct chains without overclaiming unordered set order;
- column-count mismatch diagnostic;
- branch diagnostics for missing default schema, unknown table, and unknown
  column;
- deterministic rejection of branch `ORDER BY`, branch/global `LIMIT` parsing
  shapes, locking clauses, select options, parenthesized query expressions,
  `TABLE`, `VALUES`, `INTERSECT`, `EXCEPT`, CTEs, and `UNION` in
  `INSERT ... SELECT`;
- affected rows, warning count, result row presence, and `ROW_COUNT()` state;
- reopen persistence proving the read-only compound query does not mutate rows;
- independent file-backed handles with independent branch state;
- zero-initialized cleanup for any new AST/result helper paths.

The MySQL expectation script must cover every user-visible behavior introduced
by this phase, including successful scalar/table unions, duplicate behavior,
metadata labels, mixed `ALL`/distinct behavior, column-count errors, and
deferred ordering/limiting syntax or support decisions.
