# Baseline UPDATE IGNORE Modifier

## Summary

This phase extends the current descriptor-driven single-table `UPDATE` slice
with MySQL's single-table priority and ignore modifiers:

```sql
UPDATE [LOW_PRIORITY] [IGNORE] table_name
SET assignment_column = assignment_value[, ...]
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

The goal is narrow: accept `LOW_PRIORITY` as a no-op for embedded storage, and
make `UPDATE IGNORE` apply MySQL-compatible warning adjustment for the
assignment conversions that MyLite already owns for supported single-table
updates. This slice does not implement MySQL's duplicate-key row skipping for
updates that assign primary-key, unique-key, or `AUTO_INCREMENT` columns; those
`UPDATE IGNORE` forms are rejected before physical SQL is generated.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-update-multiple-assignments/specs.md`
  - `docs/specs/baseline-update-constant-arithmetic-assignment/specs.md`
  - `docs/specs/baseline-update-unix-timestamp-arithmetic/specs.md`
  - `docs/specs/baseline-update-scalar-subquery-assignment/specs.md`
  - `docs/specs/baseline-insert-ignore-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_update_ignore_modifier_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL Runtime Observations

MySQL 8.4.9 establishes these expectations for the supported surface:

- Single-table `UPDATE LOW_PRIORITY ...` is accepted. In MyLite's embedded
  storage model it has no visible scheduling effect.
- Single-table `UPDATE LOW_PRIORITY IGNORE ...` is accepted. The reverse order
  `UPDATE IGNORE LOW_PRIORITY ...` is a syntax error.
- Under strict SQL mode, `UPDATE IGNORE ... SET not_null_int = NULL` stores the
  descriptor implicit integer value `0`, reports one warning per matched row,
  and reports changed-row affected counts.
- `UPDATE IGNORE` string truncation records one warning per matched row even
  when the adjusted value is equal to the old value. Affected rows still count
  changed rows only.
- Out-of-range numeric assignments are clipped to the target type range with
  one warning per matched row.
- Invalid temporal assignments to non-null temporal columns are adjusted to the
  corresponding zero temporal value with one warning per matched row.
- `UPDATE IGNORE ... SET column = DEFAULT` for a non-null column without an
  explicit default stores the descriptor implicit value and warns once per
  matched row.
- `ORDER BY ... LIMIT` limits both mutation and adjustment warnings to the
  rows selected for update. `LIMIT 0` changes no rows and produces no
  conversion warnings.
- Duplicate-key conflicts during `UPDATE IGNORE` skip conflicting rows and
  produce warnings in MySQL. MyLite deliberately defers this behavior for this
  phase because it requires row-by-row conflict demotion for key writes.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to expose a non-row
  result object for successful updates and owns public misuse behavior.
- Statement context: owns statement diagnostics, warning records,
  `affected_rows`, and transaction boundaries. `LOW_PRIORITY` does not change
  statement context state. `IGNORE` changes conversion policy only inside the
  current update statement.
- Lexer/parser/AST: admits `LOW_PRIORITY` and `IGNORE` immediately after
  `UPDATE` in the MySQL order for single-table updates. The parser records
  modifier nodes but does not resolve tables, keys, or type conversion.
- Analyzer/planner: resolves target table, assignments, predicates, ordering,
  and limits through MyLite descriptors. It determines whether `IGNORE`
  adjustment is enabled and rejects unsupported key assignment demotion before
  physical SQL is built.
- Catalog: remains authoritative for logical schemas, table identity, object
  kind, physical table names, columns, keys, auto-increment, generated columns,
  and foreign keys. This phase must not mutate catalog descriptors, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful supported updates return the existing non-row
  statement result with `column_count == 0`, `row_count == 0`, changed-row
  `affected_rows`, and the statement warning count.
- SQLite physical storage: owns row mutation inside generated MyLite physical
  tables. MyLite continues to build physical `UPDATE` statements from stable
  physical table names and quoted identifiers, with assignment/predicate/limit
  values bound as parameters. This phase does not require a SQLite fork patch.
- Storage/VFS/file format: unchanged. Updates affect only the shifted SQLite
  payload and do not touch the `.mylite` preamble.

## Supported SQL

This phase supports the current single-table update envelope plus the optional
modifier sequence:

```sql
UPDATE [LOW_PRIORITY] [IGNORE] table_name
SET assignment_list
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

The underlying update surface remains the current one:

- persistent or shadowing session temporary base tables;
- unqualified or schema-qualified target table names using the selected/default
  schema policy;
- one or more distinct unqualified assignment columns, with the existing
  multi-assignment restrictions;
- assignment values already admitted by the current single-table `UPDATE`
  implementation, including literals, `DEFAULT`, supported current temporal
  values, supported arithmetic subsets, supported `UNIX_TIMESTAMP()`
  arithmetic, and supported scalar subquery assignments;
- the existing baseline descriptor `WHERE` predicate subset;
- the existing single descriptor-column `ORDER BY` subset and `LIMIT row_count`
  subset.

`LOW_PRIORITY` is accepted only as an AST modifier and no-op runtime flag. It
does not affect locking, scheduling, or physical SQL.

`IGNORE` is accepted only for supported single-table updates where every
assignment target is a non-key, non-`AUTO_INCREMENT` descriptor column or a
generated-column `DEFAULT` no-op already admitted by the current update path.
If `IGNORE` is present and any assignment targets a primary-key part, unique-key
part, or `AUTO_INCREMENT` column, MyLite rejects the statement before
execution with a deterministic unsupported-feature diagnostic.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's supported grammar, not MySQL's full grammar:

```lemon
update_statement ::=
    UPDATE update_modifier_pair update_table_source SET update_assignment_list
    where_clause_opt order_clause_opt update_limit_clause_opt.

update_modifier_pair ::= .
update_modifier_pair ::= LOW_PRIORITY.
update_modifier_pair ::= IGNORE.
update_modifier_pair ::= LOW_PRIORITY IGNORE.
```

`UPDATE IGNORE LOW_PRIORITY ...` is intentionally not admitted.
Joined/multi-table updates keep their existing grammar and do not admit these
modifiers in this phase.

## Semantics

Planning:

1. Resolve the target table using the existing selected/default schema policy.
   Schema-qualified targets use the named schema. Reserved `_mylite_*` names
   are rejected before physical SQL is generated.
2. Resolve assignment, predicate, and ordering columns through MyLite
   descriptors and current descriptor case-sensitivity/collation rules.
3. If `LOW_PRIORITY` is present, record that it was accepted but do not alter
   execution.
4. If `IGNORE` is present, record `ignore_errors = true` in the update plan.
5. If `ignore_errors` is true and an executable assignment targets a primary
   key, unique key, or `AUTO_INCREMENT` column, reject the statement before
   mutation. MyLite does not generate `UPDATE OR IGNORE` or depend on SQLite's
   conflict algorithm for this slice.

Conversion:

- Supported `UPDATE IGNORE` conversion uses the same descriptor-owned target
  conversion functions as ordinary updates, but with `ignore_errors = true`.
- Explicit `NULL` into a supported `NOT NULL` target is adjusted to the
  descriptor implicit value and records one warning for each matched row that
  the update attempts to assign.
- Explicit `DEFAULT` for a no-explicit-default non-null target is adjusted to
  the descriptor implicit value and records one warning per matched row.
- Integer, decimal, approximate, `YEAR`, `TIME`, temporal, binary string, `BIT`,
  and nonbinary string range/truncation/invalid-value adjustment follows the
  existing insert-ignore and non-strict conversion policy for the currently
  admitted update assignment value subset.
- `LIMIT 0` and no-match updates do not evaluate value adjustment and therefore
  do not record conversion warnings.

Ordering and limiting:

- `ORDER BY` and `LIMIT` reuse the current update selection behavior. Any
  matched-row warning multiplication applies only to the physical rows selected
  by the current update plan after `WHERE`, ordering, and limiting.
- For tied order keys without an additional sort key, this phase does not
  claim which tied rows MySQL or MyLite updates.

Result reporting:

- Successful updates report MySQL-style changed-row affected counts, not
  matched-row counts.
- No-op adjusted assignments can still produce warnings while reporting
  `affected_rows == 0`.
- Supported in-range updates without adjustment report `warning_count == 0`.
- `SHOW WARNINGS` exposes adjustment warnings already recorded by the existing
  diagnostics area.

## Diagnostics

This phase preserves existing diagnostics for syntax errors, missing default
schema, unknown schema, unknown target table, reserved `_mylite_*` targets,
unsupported object kinds, unknown assignment/predicate/order columns,
unsupported assignment values, unsupported predicates/order/limit forms,
integer or temporal conversion errors outside `IGNORE`, `NULL` into `NOT NULL`
outside an adjustable path, duplicate-key errors outside the deferred
`UPDATE IGNORE` key-write subset, foreign-key violations, physical SQLite
failures, allocation failures, and public API misuse.

New or changed diagnostics:

- `UPDATE IGNORE LOW_PRIORITY ...`: parser syntax error.
- `UPDATE IGNORE` with key or auto-increment assignment: deterministic
  MyLite-specific unsupported diagnostic before execution.
- `UPDATE IGNORE` value adjustment warnings reuse the existing MySQL-compatible
  warning codes/messages emitted by MyLite's DML conversion layer for the
  corresponding conversion class.

## Performance And SQLite Handling

`LOW_PRIORITY` and `IGNORE` are planner flags. They do not add a new physical
storage layer and do not fork SQLite. The generated physical SQL remains the
current descriptor-built `UPDATE` shape. MyLite binds adjusted values and
predicate/limit parameters with prepared statements rather than interpolating
SQL literals.

Because duplicate-key skip behavior is deferred, MyLite does not perform a
row-by-row materialized update loop for this slice. Existing ordered/limited
update planning may still use the current row-id selection strategy, keeping
the row-id invariant internal to physical storage.

## Tests

Add a fast C runtime test focused on the modifier behavior and update the parser
tests. Coverage must include:

- parser acceptance for `UPDATE LOW_PRIORITY`, `UPDATE IGNORE`, and
  `UPDATE LOW_PRIORITY IGNORE`;
- parser rejection for `UPDATE IGNORE LOW_PRIORITY`;
- `LOW_PRIORITY` no-op execution with zero warnings;
- strict-mode `UPDATE IGNORE` adjustment for `NULL` into `NOT NULL`, no-default
  `DEFAULT`, string truncation, integer range clipping, and invalid temporal
  assignment;
- multiple non-key assignments with warning counts across assignments and
  matched rows;
- no-op adjusted values reporting changed-row affected counts;
- `WHERE`, `ORDER BY`, and `LIMIT` interactions, including `LIMIT 0`;
- warning records exposed by `SHOW WARNINGS`;
- schema-qualified and unqualified target resolution through existing update
  tests;
- deterministic rejection for deferred key or auto-increment assignment under
  `UPDATE IGNORE`;
- close/reopen persistence for adjusted values;
- unchanged public result shape for successful non-row updates.

Run:

1. `packages/libmylite/tests/mysql_baseline_update_ignore_modifier_expectations.sh`
2. `cmake --build --preset dev`
3. Focused parser/runtime CTest entries for update behavior.
4. `cmake --workflow --preset check`

## Out Of Scope

- MySQL duplicate-key row skipping for `UPDATE IGNORE` key assignments.
- Joined or multi-table `UPDATE` modifiers.
- `DELETE IGNORE`, `LOAD DATA IGNORE`, or broader DML ignore behavior.
- Aliases, partitions, CTEs, arbitrary expressions, correlated subqueries,
  triggers, privilege semantics, full optimizer scheduling, or SQLite fork
  patches.
