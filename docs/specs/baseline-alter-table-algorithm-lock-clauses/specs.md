# Baseline ALTER TABLE ALGORITHM/LOCK Clauses

This phase adds the narrow `ALTER TABLE` option-tail syntax most often emitted by
schema migration tools:

```sql
ALTER TABLE t ADD COLUMN c INT, ALGORITHM=INSTANT, LOCK=DEFAULT
ALTER TABLE t DROP INDEX k, ALGORITHM=INPLACE, LOCK=NONE
ALTER TABLE t RENAME INDEX old_k TO new_k, LOCK=NONE
ALTER TABLE child DROP FOREIGN KEY fk, ALGORITHM=INPLACE, LOCK=NONE
ALTER TABLE t FORCE, ALGORITHM=COPY
```

It is a compatibility gate over already-supported MyLite single-action
`ALTER TABLE` paths. It does not add MySQL's online DDL scheduler, metadata-lock
behavior, concurrent DML behavior, optimizer behavior, or general multi-action
ALTER support.

## References And Evidence

- Official MySQL 8.4 `ALTER TABLE` documentation:
  `https://dev.mysql.com/doc/refman/8.4/en/alter-table.html`
- MySQL 8.4.9 runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_alter_table_algorithm_lock_clauses_expectations.sh`.

Observed MySQL 8.4.9 behavior for this slice:

- `ALGORITHM` and `LOCK` are comma-separated `ALTER TABLE` options.
- `ALGORITHM=DEFAULT`, `ALGORITHM=INSTANT`, `ALGORITHM=INPLACE`, and
  `ALGORITHM=COPY` are the relevant values for `ALTER TABLE`.
- `LOCK=DEFAULT`, `LOCK=NONE`, `LOCK=SHARED`, and `LOCK=EXCLUSIVE` are the
  relevant values.
- `LOCK` without a preceding comma after another alter action is a syntax error.
- `ALGORITHM=INSTANT` with `LOCK=NONE`, `LOCK=SHARED`, or `LOCK=EXCLUSIVE`
  fails with `1221 / HY000`.
- Common online secondary-index and foreign-key metadata operations accept
  `ALGORITHM=INPLACE, LOCK=NONE`.
- `ALTER TABLE ... FORCE, ALGORITHM=COPY` accepts and reports copied rows in
  MySQL. MyLite's existing `FORCE` path remains the source of row-count
  semantics for this slice.
- Unsupported algorithm/action combinations outside this slice remain rejected
  deterministically rather than silently accepted.

## Scope

Admitted option tails are limited to existing MyLite persistent-base-table
single-action ALTER statements where MyLite already has descriptor-driven
execution:

- `ADD COLUMN`;
- `DROP COLUMN`;
- `RENAME COLUMN`;
- `ADD PRIMARY KEY`;
- `DROP PRIMARY KEY`;
- `ADD INDEX` / `ADD KEY` / `ADD UNIQUE` / `ADD FULLTEXT`;
- `DROP INDEX` / `DROP KEY`;
- `RENAME INDEX` / `RENAME KEY`;
- `ADD FOREIGN KEY`;
- `DROP FOREIGN KEY`;
- `FORCE`.

Each option tail is comma-separated and may contain one or more `ALGORITHM` or
`LOCK` options:

```sql
ALTER TABLE t <single_action>, ALGORITHM = value
ALTER TABLE t <single_action>, LOCK = value
ALTER TABLE t <single_action>, ALGORITHM = value, LOCK = value
```

The equals sign is optional where MySQL admits it.

Out of scope:

- `ALTER TABLE t ALGORITHM=...` or `ALTER TABLE t LOCK=...` with no primary
  action;
- `ALTER TABLE t <single_action> LOCK=...` without a comma;
- options before the action;
- options mixed with a second non-option alter action;
- `ONLINE`, `WAIT`, `NOWAIT`, `WITH/WITHOUT VALIDATION`, partition options,
  storage-engine-specific online DDL scheduling, or concurrency enforcement;
- full MySQL algorithm/action matrix parity;
- temporary tables, views, triggers, privilege semantics, and metadata-lock
  instrumentation beyond existing MyLite behavior.

## Ownership Boundaries

- Public API: no ABI or public result-object changes.
- Statement context: unchanged; existing statement-time and implicit-commit
  behavior for supported DDL remains authoritative.
- Parser/AST: accepts a restricted option tail and stores normalized option
  values on the existing ALTER statement AST node without changing existing
  child indexes.
- Analyzer/runtime: validates the option/action combination, then dispatches to
  the existing descriptor-driven ALTER execution path.
- Catalog: remains the source of logical table, column, index, and foreign-key
  descriptors. Algorithm and lock options do not become catalog metadata.
- Result builder: successful options use the existing non-row DDL result shape,
  affected rows, and warnings from the underlying ALTER action.
- Storage/VFS/SQLite: no new SQLite fork patch. Existing physical rebuilds,
  physical indexes, and file-backed `.mylite` invariants are reused.

## Grammar

Independently authored MyLite grammar sketch:

```lemon
alter_table_drop_index_statement ::=
    ALTER TABLE table_name DROP INDEX identifier alter_table_option_tail_opt.

alter_table_option_tail_opt ::= .
alter_table_option_tail_opt ::= COMMA alter_table_algorithm_lock_option_list.

alter_table_algorithm_lock_option_list ::= alter_table_algorithm_lock_option.
alter_table_algorithm_lock_option_list ::=
    alter_table_algorithm_lock_option_list COMMA alter_table_algorithm_lock_option.

alter_table_algorithm_lock_option ::= ALGORITHM equal_opt alter_algorithm_value.
alter_table_algorithm_lock_option ::= LOCK equal_opt alter_lock_value.

alter_algorithm_value ::= DEFAULT.
alter_algorithm_value ::= INSTANT_IDENTIFIER.
alter_algorithm_value ::= INPLACE_IDENTIFIER.
alter_algorithm_value ::= COPY_IDENTIFIER.

alter_lock_value ::= DEFAULT.
alter_lock_value ::= NONE.
alter_lock_value ::= SHARED_IDENTIFIER.
alter_lock_value ::= EXCLUSIVE_IDENTIFIER.
```

The real MyLite grammar applies the same `alter_table_option_tail_opt` only to
the admitted single-action ALTER productions listed in Scope. It intentionally
does not lower the entire MySQL `alter_option` list into a general multi-action
AST.

## Semantics

`ALGORITHM` and `LOCK` are compatibility assertions. MyLite accepts them only
when doing so does not change the existing MyLite logical result for the
underlying ALTER action.

Successful execution:

- validates the target through the existing ALTER action path;
- ignores online-concurrency scheduling because MyLite has no MySQL server
  metadata-lock scheduler;
- preserves existing affected-row and warning behavior for the underlying
  action;
- preserves `.mylite` preamble and shifted SQLite payload invariants;
- mutates catalog rows only when the underlying ALTER action already mutates
  catalog rows.

Duplicate option names are accepted by the parser with the later value winning.
The slice does not claim full MySQL duplicate-option behavior; tests use one
algorithm and one lock option.

## Validation Policy

The runtime rejects invalid option values and unsupported combinations before
running the underlying ALTER action.

General validation:

- unknown algorithm value: syntax-style error `1064 / 42000`;
- unknown lock value: syntax-style error `1064 / 42000`;
- `ALGORITHM=INSTANT` with `LOCK=NONE`, `LOCK=SHARED`, or `LOCK=EXCLUSIVE`:
  `1221 / HY000`;
- unsupported algorithm/action pairs: deterministic unsupported diagnostic,
  or a MySQL-compatible algorithm diagnostic when verified by this phase.

Action policy for this phase:

- `ADD COLUMN`, `DROP COLUMN`, and `RENAME COLUMN`: allow `DEFAULT`, `INSTANT`,
  and `COPY`; reject `INPLACE` unless a later feature verifies a safe narrower
  subset.
- `ADD INDEX`, `DROP INDEX`, `RENAME INDEX`, `ADD FOREIGN KEY`, and
  `DROP FOREIGN KEY`: allow `DEFAULT`, `INPLACE`, and `COPY`; reject `INSTANT`.
- `ADD PRIMARY KEY`, `DROP PRIMARY KEY`, and `FORCE`: allow `DEFAULT` and
  `COPY`; reject `INSTANT` and `INPLACE`.
- Omitted `ALGORITHM` means no algorithm assertion.
- All lock values are accepted except the `ALGORITHM=INSTANT` conflict above.

This action matrix is intentionally narrower than MySQL. For example, some
`CHANGE`/`MODIFY` and metadata-only forms can use online algorithms in MySQL,
but they remain out of this phase so MyLite does not over-accept combinations
that it has not verified.

## Diagnostics

- Syntax errors and malformed option placement: existing parser `1064 / 42000`.
- Unknown schema/table, reserved `_mylite_*` targets, wrong object kind, unknown
  columns/indexes/foreign keys, duplicate names, key conflicts, and physical
  failures: existing underlying ALTER diagnostics.
- `ALGORITHM=INSTANT` with non-default lock: `1221 / HY000` and the verified
  MySQL message shape.
- `ALGORITHM=INSTANT` on secondary-index or foreign-key option paths:
  `1845 / 0A000` with the verified MySQL message shape.
- Unsupported algorithm/action combinations not covered by a MySQL-shaped error:
  MyLite-specific unsupported diagnostic.
- Allocation failures: existing `MYLITE_NOMEM`.

Supported successful statements have `warning_count == 0`.

## Tests

Add a focused runtime test under `packages/libmylite/tests/` and a MySQL 8.4.9
expectation script covering:

- parser acceptance for supported comma-separated option tails;
- parser rejection for no-comma tails, standalone option-only ALTER, and
  option tails mixed with a second action;
- `ADD COLUMN ... ALGORITHM=INSTANT, LOCK=DEFAULT`;
- `DROP COLUMN ... ALGORITHM=DEFAULT, LOCK=NONE`;
- `DROP INDEX ... ALGORITHM=INPLACE, LOCK=NONE`;
- `RENAME INDEX ... LOCK=NONE`;
- `ADD INDEX ... ALGORITHM=INPLACE, LOCK=NONE`;
- `DROP FOREIGN KEY ... ALGORITHM=INPLACE, LOCK=NONE`;
- `FORCE, ALGORITHM=COPY`;
- `DROP PRIMARY KEY, ALGORITHM=COPY, LOCK=EXCLUSIVE`;
- invalid `ALGORITHM=INSTANT, LOCK=NONE`;
- invalid `ALGORITHM=INSTANT` on `DROP INDEX`;
- persistence/reopen for at least one physical-mutation action with options;
- preservation of MyLite preamble for an option-bearing physical rebuild.

Existing ALTER, catalog, file-format, parser, and lifecycle tests remain part of
the required verification.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/sql-indexes-constraints.md` if needed for index DDL option
  wording.

Use partial wording. Do not claim full online DDL, full algorithm/action parity,
metadata-lock scheduling, concurrent DML guarantees, standalone option-only
ALTER support, or multi-action ALTER support.
