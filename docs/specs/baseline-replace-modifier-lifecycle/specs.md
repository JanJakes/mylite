# Baseline Replace Modifier Lifecycle

## Status

This feature specifies a narrow modifier slice for already-supported no-key
`REPLACE` statements. It builds on `REPLACE ... VALUES`, `REPLACE ... SET`,
`REPLACE ... SELECT`, statement context diagnostics, warning records, and
descriptor-driven physical inserts.

The slice admits `LOW_PRIORITY` and `DELAYED` immediately after `REPLACE` for
the currently supported `VALUES`, `SET`, and descriptor-backed `SELECT` forms.
`LOW_PRIORITY` is accepted as an embedded no-op. `DELAYED` is accepted as a
deprecated no-op and records the MySQL 8.4 warning that the statement was
converted to ordinary `REPLACE`.

This feature does not add duplicate-key replacement semantics. MyLite still has
no primary-key or unique-key descriptors, so all supported `REPLACE` forms
remain insert-equivalent no-key operations.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline replace values lifecycle:
  `docs/specs/baseline-replace-values-lifecycle/specs.md`
- Baseline replace set lifecycle:
  `docs/specs/baseline-replace-set-lifecycle/specs.md`
- Baseline replace select lifecycle:
  `docs/specs/baseline-replace-select-lifecycle/specs.md`
- Baseline show warnings diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `REPLACE`:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_replace_modifier_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `REPLACE LOW_PRIORITY INTO ... VALUES`, `REPLACE LOW_PRIORITY INTO ... SET`,
  and `REPLACE LOW_PRIORITY INTO ... SELECT` all succeed for ordinary tables.
- `LOW_PRIORITY` does not change affected-row counts, result shape, stored
  rows, or warning count in the observed supported forms.
- `REPLACE DELAYED INTO ... VALUES`, `REPLACE DELAYED INTO ... SET`, and
  `REPLACE DELAYED INTO ... SELECT` all execute as ordinary `REPLACE`.
- Successful `DELAYED` forms report the ordinary affected-row count and
  `@@warning_count == 1`.
- Immediately after a `DELAYED` statement, `SHOW WARNINGS` reports warning
  code `3005` with message `REPLACE DELAYED is no longer supported. The
  statement was converted to REPLACE.`
- If a `DELAYED` statement later fails ordinary validation, MySQL records the
  delayed-warning row and the error row in `SHOW WARNINGS`.
- MySQL rejects multiple modifier words such as
  `REPLACE LOW_PRIORITY DELAYED ...` and
  `REPLACE DELAYED LOW_PRIORITY ...` with syntax error `1064`.
- MySQL supports wider `REPLACE` surfaces outside this slice, including
  partition selection and duplicate-key delete-before-insert semantics.

## Scope

The implementation must add:

- parser and AST support for one optional `REPLACE` modifier token:
  `LOW_PRIORITY` or `DELAYED`;
- modifier support on existing supported `REPLACE ... VALUES`,
  `REPLACE ... SET`, and `REPLACE ... SELECT` forms only;
- `LOW_PRIORITY` as a no-op that produces no warnings;
- `DELAYED` as a no-op that appends one warning before ordinary planning and
  execution;
- warning code `3005`, SQLSTATE `HY000`, level `Warning`, and the MySQL-shaped
  delayed conversion message;
- result warning-count propagation for successful delayed statements;
- delayed warning preservation when ordinary `REPLACE` validation later fails;
- existing descriptor-driven planning, conversion, physical SQL generation,
  statement atomicity, and no-key affected-row behavior unchanged;
- MySQL-runtime expectation artifacts and focused C tests for each admitted
  statement form.

## Non-Goals

This feature must not implement:

- primary-key or unique-key descriptors, duplicate-key lookup,
  delete-before-insert replacement, or replacement affected-row counts that
  include deleted rows;
- `INSERT` priority/delayed modifiers;
- `IGNORE`, `HIGH_PRIORITY`, multiple `REPLACE` modifiers, modifier aliases,
  modifiers after `INTO`, or arbitrary modifier ordering;
- partition selection, table aliases, row aliases, `RETURNING`, `REPLACE ...
  TABLE`, or wider source/query forms;
- warning demotion, non-strict SQL modes, protocol status flags, privilege
  semantics, delayed queues, background writes, storage-engine scheduling
  changes, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement dispatch,
  result-handle ownership, public misuse behavior, and failure cleanup.
- Lexer/parser/AST own syntax admission for the optional modifier and store it
  as AST metadata independent of runtime, catalog, storage, and SQLite.
- Runtime execution owns translating `LOW_PRIORITY` to no-op and appending the
  deprecated delayed warning before ordinary planning/execution.
- Statement context and diagnostics own warning storage, previous diagnostics,
  `@@warning_count`, `SHOW WARNINGS`, and result warning-count propagation.
- Existing replace planners own descriptor-driven target/source resolution,
  value conversion, default filling, range/nullability diagnostics, and stable
  SQLite physical SQL generation. The modifier must not bypass those paths.
- Catalog and storage ownership does not change. The modifier must not mutate
  catalog rows, descriptor versions, catalog generation, SQLite schema
  generation, or the `.mylite` preamble.
- SQLite remains the physical row storage engine. The modifier must not rely on
  SQLite conflict algorithms, triggers, or fork hooks.

## Supported SQL Grammar

Supported subset:

```sql
REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name [(column_name[, ...])]
    VALUES (value[, ...])[, ...]

REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name
    SET column_name = value[, ...]

REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name [(column_name[, ...])]
    SELECT ...
```

The `VALUES`, `SET`, and `SELECT` tails remain exactly the subsets already
implemented by their respective lifecycle specs.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
replace_modifier_opt(A) ::= . {
    A = NULL;
}
replace_modifier_opt(A) ::= LOW_PRIORITY(T). {
    A = mylite_sql_parser_make_replace_low_priority_modifier(state, T);
}
replace_modifier_opt(A) ::= DELAYED(T). {
    A = mylite_sql_parser_make_replace_delayed_modifier(state, T);
}

replace_values_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T)
    insert_column_list_opt(C) VALUES insert_row_list(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V, M);
}

replace_set_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_replace_set_statement(state, R, T, S, M);
}

replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S, M);
}
```

The optional-`INTO` alternatives use the same modifier nonterminal. Multiple
modifiers are not admitted.

## Runtime Semantics

`LOW_PRIORITY` has no user-visible effect beyond syntax acceptance for the
supported embedded storage path. It must not alter locks, transaction shape,
affected rows, warning count, or generated SQLite SQL.

`DELAYED` must append one warning before ordinary statement planning. If the
ordinary `REPLACE` path succeeds, the result object reports warning count `1`,
`@@warning_count` reads `1`, and `SHOW WARNINGS` exposes the warning until the
next statement replaces the diagnostics snapshot according to the existing
statement-context policy. If ordinary planning or execution fails, the
diagnostics area contains both the delayed warning and the error condition.

The warning text is:

```text
REPLACE DELAYED is no longer supported. The statement was converted to REPLACE.
```

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- repeated or mixed modifiers;
- unsupported modifier placement, such as after `INTO`;
- unsupported modifiers such as `HIGH_PRIORITY` and `IGNORE`;
- `DELAYED` warning allocation failure;
- all existing `REPLACE ... VALUES`, `REPLACE ... SET`, and
  `REPLACE ... SELECT` diagnostics unchanged after modifier handling.

Successful `LOW_PRIORITY` statements must have warning count `0`. Successful
`DELAYED` statements must have warning count `1`.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, extending existing
replace lifecycle tests where that keeps coverage local. Add a MySQL-runtime
expectation script for the modifier behavior.

Coverage must include:

- `LOW_PRIORITY` on `VALUES`, `SET`, and `SELECT` forms;
- `DELAYED` on `VALUES`, `SET`, and `SELECT` forms;
- affected rows, `ROW_COUNT()`, result warning count, `@@warning_count`, and
  stored rows for each successful form;
- `SHOW WARNINGS` result for delayed statements;
- delayed warning retained with a subsequent ordinary validation error;
- syntax rejection for repeated/mixed modifier words, unsupported
  `HIGH_PRIORITY`, unsupported `IGNORE`, and unsupported modifier placement;
- unchanged no-key insert-equivalent behavior;
- existing replace values, replace set, replace select, parser, diagnostics,
  show warnings, row count, and statement-context tests continue to pass.
