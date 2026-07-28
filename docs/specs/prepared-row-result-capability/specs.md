# Prepared Row-Result Capability And Replay

## Purpose

Every prepared statement that can produce rows must own one explicit, typed
result capability. The capability controls prepare, first execution, reset,
re-execution, and finalization. It replaces the current split between a
static `SELECT`-only flag and a second runtime column-count test.

This specification closes follow-up finding `PREP-02`. It extends, but does
not weaken, the lazy lifecycle defined by
`docs/specs/prepared-statement-lazy-prepare/specs.md`.

## Implementation status

The specification and MySQL 8.4.9 expectations are frozen before the runtime
change. The audited implementation incorrectly classifies prepared `SHOW`
statements as commands during reset even though execution materializes rows.
The retained materialized rowset is consequently replayed after reset instead
of executing the statement again.

## Authorities

- MySQL 8.4 prepared statements and permitted statement families:
  <https://dev.mysql.com/doc/refman/8.4/en/sql-prepared-statements.html>
- MySQL 8.4 prepared-statement caching and automatic reprepare:
  <https://dev.mysql.com/doc/refman/8.4/en/statement-caching.html>
- MySQL 8.4 `SHOW` statement family:
  <https://dev.mysql.com/doc/refman/8.4/en/show.html>
- MySQL 8.4 `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4 `EXPLAIN` and `DESCRIBE`:
  <https://dev.mysql.com/doc/refman/8.4/en/explain.html>
- MySQL 8.4.9 runtime fixture:
  `packages/libmylite/tests/mysql_prepared_row_result_capability_expectations.sh`

The design is independently authored from public documentation, observed
MySQL 8.4.9 behavior, and MyLite's existing statement and result APIs.

## MySQL 8.4.9 observations

The pinned MySQL 8.4.9 runtime was probed through mysqli native prepared
statements. The same prepared handle was executed, its result was exhausted,
the referenced schema or session was mutated, and the handle was executed
again.

- `SHOW TABLES` included a table created between executions.
- `DESCRIBE alpha` included a column added between executions.
- `EXPLAIN SELECT ... WHERE value = 20` changed from a table scan to the
  newly created `idx_value` index.
- `SHOW VARIABLES LIKE 'sql_mode'` returned the newly assigned session value.

The normalized observations were:

```text
show1=alpha
show2=alpha,beta
describe1=id,value
describe2=id,value,marker
explain1.key=NULL
explain2.key=idx_value
variables1=
variables2=ANSI_QUOTES
```

The MySQL manual also states that metadata changes to referenced tables or
views are detected and cause automatic reprepare on the next execution.

## Typed result capability

The parsed top-level statement has one capability:

| Capability | Meaning | Current statement families |
| --- | --- | --- |
| `NONE` | The statement cannot return a rowset through the current execution surface | DDL, DML, transaction, assignment, and other command statements |
| `STREAMING_QUERY` | Rows are described at prepare and streamed from a first-step SQLite execution program | Supported simple `SELECT` statements |
| `MATERIALIZED_QUERY` | Query-shaped rows are produced into a MyLite-owned result before the first row is exposed | Compound and parenthesized queries and `EXPLAIN` |
| `MATERIALIZED_UTILITY` | Synthetic or administrative rows are produced into a MyLite-owned result | `SHOW`, `DESCRIBE`, `ANALYZE`, `CHECK`, `CHECKSUM`, `OPTIMIZE`, and `REPAIR` families |
| `DYNAMIC` | Whether rows are returned depends on the invoked object or nested execution | `CALL` and any future dynamic row-producing surface |

One central classifier maps the typed AST to this capability. Prepare,
execution dispatch, result completion, reset, and finalization must consume
that classifier or the capability stored from it. They must not maintain
independent lists of row-producing statement kinds.

The capability describes ownership and lifecycle, not whether one particular
execution happens to return zero rows. A materialized result with zero rows
still has a row-producing capability and still owns result metadata.

For a `DYNAMIC` statement, the execution result determines whether that
execution produced rows. Reset must conservatively release both possible
row-result and command-completion state.

## Prepare and first execution

Prepare parses, validates, classifies, captures parameter and resolution
context, and retains reusable analysis where supported. It produces no rows
and retains no execution-owned result.

On first `mylite_stmt_step()`:

- `STREAMING_QUERY` revalidates retained analysis, lowers the current plan,
  opens the execution transaction, and streams rows;
- `MATERIALIZED_QUERY` and `MATERIALIZED_UTILITY` dispatch the statement once
  and take ownership of the returned result before exposing its first row;
- `DYNAMIC` dispatches once and records whether that execution returned
  columns;
- `NONE` dispatches once and exposes only command completion.

A runtime column count may describe the result of `DYNAMIC` execution. It
must not override the static ownership capability of other families.

## Reset contract

`mylite_stmt_reset()` releases all state owned by the current execution:

- the active or reusable SQLite execution program;
- buffered or materialized row storage and its row cursor;
- execution-owned column metadata and metadata-analysis context;
- reusable and completed result objects;
- current row, done, and execution-started flags;
- row count, affected rows, found rows, insert id, completion publication,
  and execution warnings;
- execution-specific binding conversion state.

Reset retains:

- normalized SQL and the parsed statement;
- parameter descriptors and caller-owned binding values copied into the
  statement;
- captured prepare-time schema and parsing context;
- reusable semantic analysis that still passes the normal generation,
  session, and parameter-type invalidation checks.

Reset before first step succeeds without creating an execution. Reset after
an error makes the handle eligible for another execution. Repeated reset is
idempotent. Reset does not end an explicit user transaction.

For every row-producing capability, reset must discard the complete current
rowset even when the rowset was only partly consumed. No row, column metadata,
warning, or completion object from one execution may be visible in the next.

## Re-execution and invalidation

The first step after reset follows the same execution-start sequence as the
original first step. It produces a new rowset against current schema, data,
and execute-time session state.

- Compatible DDL causes normal revalidation or reanalysis and fresh output.
- Incompatible DDL reports the current execute-time diagnostic.
- Session-dependent utilities read the current execute-time session value.
- Data mutations are visible according to the transaction and snapshot rules
  in effect for that execution.
- Stable streaming queries may reuse analysis and lowering caches only after
  their invalidation keys pass.
- Materialized and synthetic results themselves are never cached across
  reset.

Metadata exposed through the statement API belongs to the current prepared or
executing state. Execution-owned metadata is invalidated by reset. Statically
described prepare metadata may be retained or rebuilt, but it must be
refreshed before rows from a changed compatible schema are exposed.

## Finalization and failure

`mylite_stmt_finalize()` performs reset-equivalent execution cleanup and then
releases the retained statement, analysis, bindings, SQL, and parse storage.
It is valid after prepare, a partial rowset, completion, reset, or execution
failure.

If materialization fails, the temporary result is released exactly once and
the statement retains no rowset. An allocation failure during materialization,
reset, revalidation, or re-execution must leave finalization safe. Repeated
reset and finalize paths must have no leak, double free, stale cursor, or
use-after-free.

## Diagnostics and warnings

Prepare diagnostics retain the boundary specified by lazy prepare.
Execution diagnostics and warnings belong to one execution. Reset removes
statement-owned completion and warning state without restoring diagnostics
from an earlier execution. A later successful or failed execution publishes
only its own result through the unified completion path.

## Parser and grammar impact

No grammar change is required. `DESCRIBE` continues to use its existing AST
representation. The change introduces an execution-layer capability derived
from the typed AST; it does not infer behavior from SQL text.

## SQLite, storage, and ABI impact

The implementation uses the existing public SQLite prepare, reset, finalize,
and transaction APIs. It changes no SQLite fork code, VFS behavior, catalog
format, `.mylite` file format, or public ABI.

The capability is internal. Public result metadata retains the ownership,
lifetime, and invalidation rules documented for the existing statement API.

## Performance requirements

- Classification is computed once per parsed statement or by a constant-time
  AST-kind switch.
- Reset is linear only in resources that the execution actually owns.
- Stable streaming statements preserve reusable analysis and warm-plan
  behavior.
- Materialized row capacity may be reused only when ownership is unambiguous;
  row contents and metadata may never survive reset.
- No new dependency or SQLite fork patch is permitted.

## Test plan

### Native C

- reproduce stale `SHOW TABLES` replay by creating a table between executions
  of one buffered prepared handle;
- repeat `DESCRIBE` after adding a column;
- repeat `EXPLAIN` after compatible and incompatible schema changes;
- repeat a session-dependent `SHOW` after changing its session value;
- mutate table data between executions of streaming and materialized queries;
- reset before first step, after partial consumption, after completion, and
  after execution error;
- call reset repeatedly and finalize from every lifecycle state;
- assert column count, names, types, values, row order, completion state,
  warnings, and connection diagnostics for the current execution only.

### Adapter and differential

- run the MySQL 8.4.9 fixture through mysqli and PDO native prepares;
- run the equivalent mysqli and PDO MyLite suites with the same statement
  handle and intervening schema or session mutation;
- retain exact MySQL-shaped failure diagnostics for incompatible re-execution.

### Qualification

- run focused Debug and Release cursor tests;
- run focused ASan/UBSan and allocator-failpoint tests over reset,
  re-execution, failure, and finalize;
- run all native prepared, result, diagnostics, transaction, and profile
  suites;
- run the full native and PHP suites;
- run compatibility-manifest validation, clang-format, and clang-tidy gates;
- confirm the production ABI and artifact-size gates remain unchanged.
