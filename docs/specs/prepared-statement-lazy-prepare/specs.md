# Prepared-Statement Lazy Prepare

## Purpose

MyLite preparation validates and describes a statement without executing it.
In particular, preparing a parameterless table `SELECT` must not retain a read
transaction, choose a data snapshot, install an active cursor, or prevent
another command or connection from writing. Execution-owned state begins only
when `mylite_stmt_step()` first starts the statement.

This specification closes follow-up finding `PREP-01`. The adjacent
row-producing classifier and materialized-result reset work is tracked by
`PREP-02`; it must preserve the lifecycle boundary defined here.

## Authorities

- MySQL 8.4 prepared statements:
  <https://dev.mysql.com/doc/refman/8.4/en/sql-prepared-statements.html>
- MySQL 8.4 C API prepared-statement interface:
  <https://dev.mysql.com/doc/c-api/8.4/en/c-api-prepared-statement-interface.html>
- MySQL 8.4 `mysql_stmt_prepare()`:
  <https://dev.mysql.com/doc/c-api/8.4/en/mysql-stmt-prepare.html>
- MySQL 8.4 `mysql_stmt_execute()`:
  <https://dev.mysql.com/doc/c-api/8.4/en/mysql-stmt-execute.html>
- MySQL 8.4 `mysql_stmt_reset()`:
  <https://dev.mysql.com/doc/c-api/8.4/en/mysql-stmt-reset.html>
- MySQL 8.4 prepared-statement caching and automatic reprepare:
  <https://dev.mysql.com/doc/refman/8.4/en/statement-caching.html>
- MySQL 8.4.9 runtime fixture:
  `packages/libmylite/tests/mysql_prepared_statement_lazy_prepare_expectations.sh`

The design is independently authored from public documentation, observed
MySQL 8.4.9 behavior, and MyLite's existing statement API.

## MySQL 8.4.9 observations

The pinned MySQL 8.4.9 runtime was probed through mysqli and PDO with native
prepares (`PDO::ATTR_EMULATE_PREPARES = false`). Both adapters showed the same
behavior:

- preparing a constant or table `SELECT`, with or without parameters, leaves
  no InnoDB transaction for the preparing connection;
- another command can run on the same connection before execution;
- another connection can update the referenced table and perform compatible
  DDL before execution;
- the first execution observes changes committed after prepare;
- compatible table metadata changes are handled by automatic reprepare;
- a missing table is diagnosed by prepare, while a table dropped after a
  successful prepare is diagnosed by execute.

The recorded probe output is:

```text
mysqli_constant_trx=0
mysqli_table_trx=0
mysqli_after_intervening=1:changed,2:two
mysqli_param_trx=0
mysqli_param_value=changed
mysqli_missing_prepare=1146/42S02
pdo_constant_trx=0
pdo_table_trx=0
pdo_after_intervening=1:changed,2:pdo
pdo_param_trx=0
pdo_param_value=pdo
pdo_missing_prepare=42S02/1146
```

The transaction observations query `INFORMATION_SCHEMA.INNODB_TRX` for the
connection id immediately after prepare.

## Lifecycle model

### Parse time

`mylite_prepare()` owns normalized SQL, parses exactly one statement under the
prepare-time lexer modes, classifies its result capability, assigns parameter
slots, and rejects unsupported prepared statement kinds. Syntax and statement
shape errors are prepare-time diagnostics.

No result rows, affected-row changes, insert id changes, found-row changes,
warnings from expression evaluation, or data mutations occur during parsing.

### Analyze time

Analysis resolves the prepare-time default schema, referenced objects, output
metadata, parameter contexts, and the subset of the plan that is independent
of execution values. It may use a bounded internal catalog read, but any
internal transaction is completed before `mylite_prepare()` returns.

Analysis must not:

- retain a SQLite read transaction or snapshot;
- retain a busy SQLite virtual machine that has been stepped;
- install `database->active_cursor`;
- publish statement completion state;
- consume next-transaction characteristics;
- evaluate row-producing expressions.

Object lookup and function-arity errors that MySQL reports from prepare remain
prepare-time errors. Parameter conversion, constraint, and row-dependent
errors remain execute-time errors.

### Prepare return

On success, the statement owns:

- normalized SQL and its prepare-time parsing modes;
- the parsed or reproducible syntax representation;
- parameter slots and owned bindings;
- captured default-schema and character-set/collation resolution context;
- result-column metadata when the statement has a statically describable
  result;
- reusable analysis that is still valid for the current catalog, SQLite
  schema, parameter types, and relevant session key.

It owns no execution transaction, snapshot, active cursor, current row, or
completed materialized rowset. The database remains available for another
prepare or direct command.

### First step

The first `mylite_stmt_step()`:

1. verifies that every parameter is bound;
2. establishes the statement context and execution statement id;
3. rechecks catalog, SQLite-schema, parameter-type, and relevant session
   generations;
4. reparses or reanalyzes when an invalidation key changed;
5. applies the statement's transaction boundary;
6. starts the execution-owned read or write transaction;
7. prepares and binds the SQLite execution program, or invokes the
   materialized command dispatcher;
8. installs an active cursor only when unread streaming rows remain;
9. returns `MYLITE_ROW`, `MYLITE_DONE`, or the execute-time diagnostic.

The data snapshot is therefore no earlier than first step. A compatible schema
change between prepare and first step is reflected by reanalysis. An
incompatible change reports the new MySQL-shaped diagnostic without using the
stale plan.

### Reset

`mylite_stmt_reset()` ends the current execution. It finalizes or returns every
execution-owned SQLite program, releases materialized rows and the active
cursor, and commits or rolls back the internal statement transaction as
appropriate. It does not end an explicit user transaction.

Reset preserves owned SQL, prepare-time resolution context, parameter
bindings, and reusable analysis. It clears current-row and completion state.
The next step follows the same first-step invalidation sequence and obtains a
new snapshot. Reset before first step is a successful no-op on execution state.

### Finalize

`mylite_stmt_finalize()` performs reset-equivalent execution cleanup, removes
the statement from the connection's live-statement registry, releases
analysis, metadata, bindings, SQL, and parse storage, and consumes the handle.
Finalizing a never-executed statement has no connection-state side effects.

## Statement-family transitions

| Family | Prepare/analyze | First step |
| --- | --- | --- |
| Constant `SELECT` | Parse, validate, and describe; do not evaluate | Evaluate and produce the row |
| Streaming table `SELECT` | Resolve objects and output metadata; no retained read transaction or SQLite execution program | Revalidate, start the read transaction, lower/bind, and begin streaming |
| Parameterized `SELECT` | Same as the matching constant/table form, with unbound parameter descriptors | Require bindings, reanalyze on relevant type changes, then execute |
| Buffered or internally materialized row producer | Resolve and describe without retaining result rows | Materialize under the execution transaction before returning the first row |
| DML | Resolve targets and parameter contexts without mutation | Acquire the writer boundary, revalidate, mutate, and publish completion |
| DDL and transaction/control statements | Parse and validate the preparable surface without applying it | Apply implicit boundaries and execute once |
| `SHOW`, `EXPLAIN`, and other synthetic row producers | Classify and describe without retaining a replayable rowset | Produce a fresh rowset for each execution |
| SQL-level `PREPARE` handler | Retain the same native lazy statement and captured resolution context | SQL `EXECUTE` binds user variables and starts native execution |

`PREP-02` supplies the single typed row-producing classification and reset
mechanics required by the synthetic/materialized rows in this table.

## Schema and session invalidation

Prepare captures lexer modes, default schema, client/connection character sets,
and literal collation because they determine how SQL text and unqualified
objects are interpreted. Ordinary execute-time session variables and bound
values are read at execution unless another feature specification explicitly
marks them as prepare-time.

Before first step and every execution after reset, MyLite compares the retained
analysis with:

- catalog generation;
- SQLite schema generation;
- relevant session-analysis key;
- parameter type families for type-sensitive plan locations.

A mismatch discards execution artifacts and reanalyzes from owned SQL under the
captured parse modes. It never executes a stale SQLite program first.

## Diagnostics and state ownership

- A failed prepare returns no statement handle and leaves no transaction or
  cursor behind.
- A successful prepare does not publish row count, found rows, affected rows,
  insert id, or execution warnings.
- Intervening commands own their connection diagnostics. Finalizing a
  never-executed prepared statement does not restore or overwrite older
  diagnostics.
- First-step and later fetch failures are statement execution failures and are
  published through the normal unified completion path.
- Reset clears the current execution error state without changing the retained
  SQL or bindings, matching the documented MySQL reset model within MyLite's
  public ABI.

## Parser and grammar impact

No grammar change is required. Existing prepared-parameter syntax and AST
nodes remain authoritative. This feature changes runtime lifecycle and
ownership only.

## SQLite and storage impact

The implementation uses public SQLite transaction, prepare, bind, reset, and
finalize APIs. SQLite execution preparation for a streaming table query is
deferred to first step; prepare-time metadata analysis uses MyLite catalog
descriptors and releases its bounded catalog read before returning.

No SQLite fork patch, VFS change, catalog-format change, or `.mylite` file
format change is permitted for this feature.

## Performance requirements

- Stable repeated executions retain parsed/analyzed state and existing warm
  plan-cache behavior.
- Prepare must not read or materialize table rows.
- A parameterless table `SELECT` must not be parsed twice merely to become
  lazy.
- Deferring the SQLite execution program must not add work to later row steps
  beyond the first step.
- Reset and finalize remain allocation-free on the normal retained-capacity
  path.

## Test plan

### Native C

- constant and table `SELECT` with zero parameters remain in SQLite autocommit
  after prepare;
- parameterized constant and table `SELECT` have the same lifecycle;
- another prepare and direct command succeed on the same handle before first
  step;
- a second handle can commit DML and compatible DDL before first step;
- first step observes the post-prepare committed rows and schema;
- the active-cursor command restriction begins only after first step;
- missing objects fail prepare, while incompatible post-prepare schema changes
  fail execution with stable diagnostics;
- reset before execution, reset after a row, exhaustion, early finalize,
  explicit transactions, and `autocommit=0` preserve their documented
  ownership boundaries.

### MySQL and PHP

- run the mysqli/PDO native-prepare fixture against the pinned MySQL 8.4.9
  image;
- run the corresponding MyLite mysqli and PDO adapter tests;
- compare prepare/execute diagnostics and visible PHP values;
- run the five application baselines at the Phase 2 closure commit.

### Quality gates

- Release and Debug focused cursor/prepared suites;
- ASan/UBSan and LSan focused suites;
- TSan multi-handle writer test;
- allocation failpoints across prepare-time analysis and first-step lowering;
- full native and PHP adapter suites before closing Phase 2.

## Compatibility status

The public native and PHP prepared-statement surfaces remain yellow because
protocol-level metadata, the complete preparable statement inventory, and
other Phase 2 adapter gaps are tracked separately. The lazy prepare lifecycle
described here is a required invariant within the supported subset.
