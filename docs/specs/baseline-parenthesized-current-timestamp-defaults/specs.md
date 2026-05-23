# Baseline Parenthesized Current Timestamp Defaults

## Summary

This phase closes a narrow default-expression gap in the existing
`CURRENT_TIMESTAMP` default slice. MyLite already supports zero-fractional
`CURRENT_TIMESTAMP` / `NOW()` defaults for `DATETIME` and `TIMESTAMP`
descriptors, and it already supports parenthesized generated `CURRENT_DATE` and
`CURRENT_TIME` defaults. This phase admits the same parenthesized generated
current-timestamp shape:

```sql
CREATE TABLE t (
  dt DATETIME DEFAULT (CURRENT_TIMESTAMP),
  ts TIMESTAMP DEFAULT (NOW())
);
```

The feature remains descriptor-owned and deliberately limited. It does not add
general default-expression evaluation, fractional temporal defaults, UTC
timestamp defaults, or parenthesized `ON UPDATE` syntax.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite slices:
  - `docs/specs/baseline-current-timestamp-defaults/specs.md`
  - `docs/specs/baseline-current-date-time-defaults/specs.md`
  - `docs/specs/baseline-character-expression-defaults/specs.md`
  - `docs/specs/baseline-integer-expression-defaults/specs.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/sql-table-ddl.md`
  - `docs/compatibility/type-system-literals-conversion.md`
- Official MySQL 8.4 Reference Manual:
  - automatic `TIMESTAMP` and `DATETIME` initialization:
    <https://dev.mysql.com/doc/refman/8.4/en/timestamp-initialization.html>
  - data type default values:
    <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
  - `CREATE TABLE`:
    <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_parenthesized_current_timestamp_defaults_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used for this slice:

- `DATETIME DEFAULT (CURRENT_TIMESTAMP)`, `DATETIME DEFAULT
  (CURRENT_TIMESTAMP())`, and `DATETIME DEFAULT (NOW())` are accepted.
- `TIMESTAMP DEFAULT (NOW())` is accepted.
- Parenthesized `LOCALTIME`, `LOCALTIME()`, `LOCALTIMESTAMP`, and
  `LOCALTIMESTAMP()` defaults are accepted and normalize like `NOW()`.
- Nested parentheses such as `DEFAULT ((NOW()))` are accepted.
- `SHOW CREATE TABLE` renders the admitted parenthesized current-timestamp
  defaults as `DEFAULT (now())`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report `now()` as the visible
  default and `DEFAULT_GENERATED` as the extra metadata.
- Omitted-column `INSERT` materializes the statement timestamp. All automatic
  values in one row use the same statement time.
- `ALTER TABLE ... ADD COLUMN ... DEFAULT (NOW())` backfills existing rows with
  the alter statement timestamp.
- `ALTER TABLE ... ALTER COLUMN ... SET DEFAULT (CURRENT_TIMESTAMP)` changes
  metadata only and affects future default materialization.
- `ALTER TABLE ... MODIFY COLUMN ... DEFAULT (LOCALTIMESTAMP)` and `CHANGE
  COLUMN ... DEFAULT (LOCALTIME())` preserve the generated default metadata.
- MySQL rejects `ON UPDATE (CURRENT_TIMESTAMP)` as a syntax error. Parentheses
  apply to default expressions in this phase, not to the `ON UPDATE` clause.
- MySQL accepts broader expression defaults such as `INT DEFAULT
  (CURRENT_TIMESTAMP)`, `DATETIME DEFAULT (UTC_TIMESTAMP())`, and fractional
  `DATETIME DEFAULT (CURRENT_TIMESTAMP(1))`. Those broader expression-default
  and fractional-temporal surfaces remain outside this MyLite phase.

## Supported SQL

Supported only for persistent and session-temporary table column definitions
where the existing host statement already accepts the target descriptor:

```sql
CREATE TABLE table_name (
  column_name DATETIME DEFAULT (current_timestamp_value)
);

ALTER TABLE table_name
  ADD COLUMN column_name TIMESTAMP DEFAULT (current_timestamp_value);

ALTER TABLE table_name
  MODIFY COLUMN column_name DATETIME DEFAULT (current_timestamp_value);

ALTER TABLE table_name
  CHANGE COLUMN old_name new_name TIMESTAMP DEFAULT (current_timestamp_value);

ALTER TABLE table_name
  ALTER [COLUMN] column_name SET DEFAULT (current_timestamp_value);
```

`current_timestamp_value` is the existing zero-fractional current-timestamp
value subset:

- `CURRENT_TIMESTAMP`
- `CURRENT_TIMESTAMP()`
- `NOW()`
- `LOCALTIME`
- `LOCALTIME()`
- `LOCALTIMESTAMP`
- `LOCALTIMESTAMP()`

Parentheses may be nested around that value because MyLite already has a
recursive expression-unwrapping helper. The semantic rule is still exactly one
current-timestamp value after unwrapping.

### MyLite Lemon-Syntax Snippet

The parser already has the grammar shape needed for this feature:

```lemon
column_default(A) ::= DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_column_default_value(state, D, V);
}

column_default_value(A) ::= LPAREN(L) expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, E, R);
}

expression(A) ::= current_timestamp_value(V). {
    A = V;
}
```

This phase changes semantic validation and finalization so a parenthesized
`current_timestamp_value` is accepted for `DATETIME` and `TIMESTAMP` defaults.
It does not add MySQL's full default-expression grammar.

## Semantics

Planning:

1. Resolve the table and column through the existing descriptor-owned DDL
   planning paths.
2. Identify a `DEFAULT (expression)` node.
3. Recursively unwrap parentheses around the expression.
4. Accept the default only when the unwrapped expression is the existing
   zero-fractional current-timestamp AST node and the target descriptor is
   `DATETIME` or `TIMESTAMP`.
5. Store the existing catalog default kind
   `MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP`.

The catalog remains the metadata authority. There is no new catalog field:
parenthesized and unparenthesized current-timestamp defaults share the same
logical default kind. MyLite renders these defaults with the current canonical
metadata text for the baseline current-timestamp slice. The visible MySQL
runtime distinction between `DEFAULT CURRENT_TIMESTAMP` and `DEFAULT (now())`
is not persisted in this phase because existing descriptors do not preserve
default-expression spelling.

Materialization reuses the existing statement-time path:

- omitted-column `INSERT`, explicit DML `DEFAULT`, `REPLACE`, and supported
  `UPDATE column = DEFAULT` materialize the statement timestamp;
- `ALTER TABLE ... ADD COLUMN` backfills existing rows with the statement
  timestamp at alter execution time;
- `CREATE TABLE ... LIKE`, reopen, rename, and descriptor copies preserve the
  logical generated current-timestamp default.

## Deferred Surface

This phase intentionally does not support:

- parenthesized `ON UPDATE (CURRENT_TIMESTAMP)` syntax;
- fractional temporal defaults such as `CURRENT_TIMESTAMP(1)`;
- `UTC_TIMESTAMP()` defaults;
- parenthesized current-timestamp defaults for non-temporal descriptors such as
  `INT DEFAULT (CURRENT_TIMESTAMP)`;
- arbitrary default expressions, column references, operators, subqueries,
  variables, parameters, stored functions, or loadable functions;
- generated columns, triggers, privilege semantics, or protocol-grade default
  expression spelling preservation.

## Diagnostics

- Parenthesized current-timestamp defaults on unsupported descriptors return
  the existing deterministic invalid-default diagnostic for the column.
- Unsupported current-timestamp arities return the current deterministic
  default-validation diagnostic used by MyLite's limited temporal default
  surface.
- Parenthesized `ON UPDATE (CURRENT_TIMESTAMP)` remains a parser syntax error.
- Unknown schema/table/column names, reserved names, allocation failures,
  physical SQLite failures, public API misuse, and file-format failures use the
  existing DDL/default diagnostics.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and result ownership conventions do
  not change.
- Statement context: unchanged. The feature reuses the current statement-time
  value and diagnostic/warning counters.
- Lexer/parser/AST: no new tokens or AST node kinds. Existing parenthesized
  expression and current-timestamp nodes are reused.
- Analyzer/planner: validates the exact default-expression subset and maps it
  to the existing current-timestamp default kind.
- Catalog: no schema bump. Existing default kind and descriptor-copy paths stay
  authoritative.
- Result and introspection builders: no new metadata fields. Existing current
  timestamp default rendering remains the MyLite compatibility contract.
- SQLite physical storage: no SQLite default expression or trigger dependency.
  MyLite binds materialized statement timestamp text through existing prepared
  statements and generated table-rebuild SQL.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants must remain untouched.

## Performance

Planning adds one recursive AST unwrap in default validation/finalization. DML
and table rebuilds stay on existing prepared-statement paths. No row data is
materialized in MyLite beyond the existing `ALTER TABLE ... ADD COLUMN`
backfill behavior for generated defaults, and no SQLite fork patch or
extension point is needed.

## Tests

Add MySQL-runtime expectation coverage for:

- create-table admission for every admitted parenthesized current-timestamp
  synonym;
- nested parentheses;
- `SHOW CREATE TABLE`, `SHOW COLUMNS`, and
  `INFORMATION_SCHEMA.COLUMNS` metadata;
- omitted-column insert materialization and status counters;
- `ALTER TABLE ... ADD COLUMN` backfill;
- `ALTER COLUMN SET DEFAULT`, `MODIFY COLUMN`, and `CHANGE COLUMN` metadata and
  future row behavior;
- `CREATE TABLE ... LIKE` preservation;
- parser rejection for parenthesized `ON UPDATE`;
- MySQL acceptance of broader expression-default forms that remain deferred in
  MyLite.

Add fast C runtime tests for:

- successful create/insert/select/default metadata;
- add-column backfill and alter-set-default future materialization;
- modify/change/like/reopen preservation;
- descriptor diagnostics for unsupported target types and unsupported
  expression forms;
- `.mylite` preamble preservation and independent handle statement-time state.
