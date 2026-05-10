# Baseline Insert Modifier Lifecycle

## Status

This feature specifies a narrow modifier slice for already-supported
descriptor-driven `INSERT` statements. It builds on `INSERT ... VALUES`,
`INSERT ... SET`, `INSERT ... SELECT`, statement context diagnostics, warning
records, descriptor-owned integer and `NULL` conversion, and SQLite physical
row storage.

The slice admits one optional priority/delayed modifier immediately after
`INSERT` for the currently supported `VALUES`, `SET`, and descriptor-backed
`SELECT` forms:

- `LOW_PRIORITY` is accepted as an embedded no-op.
- `HIGH_PRIORITY` is accepted as an embedded no-op.
- `DELAYED` is accepted as a deprecated no-op and records the MySQL 8.4 warning
  that the statement was converted to ordinary `INSERT`.

The slice also aligns the `INSERT ... VALUES` grammar with the already
implemented `INSERT ... SET` and `INSERT ... SELECT` forms by admitting
optional `INTO` for the existing no-modifier values path.

This feature does not add `IGNORE`, `ON DUPLICATE KEY UPDATE`, partition
selection, duplicate-key handling, generated ids, delayed queues, or storage
engine scheduling behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline insert set lifecycle:
  `docs/specs/baseline-insert-set-lifecycle/specs.md`
- Baseline insert select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline replace modifier lifecycle:
  `docs/specs/baseline-replace-modifier-lifecycle/specs.md`
- Baseline show warnings diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- Baseline row count function:
  `docs/specs/baseline-row-count-function/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, `INSERT DELAYED`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-delayed.html
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
`packages/libmylite/tests/mysql_baseline_insert_modifier_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `INSERT LOW_PRIORITY INTO ... VALUES`, `INSERT LOW_PRIORITY INTO ... SET`,
  and `INSERT LOW_PRIORITY INTO ... SELECT` all succeed for ordinary InnoDB
  tables.
- `INSERT HIGH_PRIORITY INTO ... VALUES`, `INSERT HIGH_PRIORITY INTO ... SET`,
  and `INSERT HIGH_PRIORITY INTO ... SELECT` all succeed for ordinary InnoDB
  tables.
- `LOW_PRIORITY` and `HIGH_PRIORITY` do not change affected-row counts, stored
  rows, result shape, or warning count in the observed supported forms.
- `INSERT LOW_PRIORITY table VALUES ...`, `INSERT HIGH_PRIORITY table SET ...`,
  and `INSERT HIGH_PRIORITY table SELECT ...` work without `INTO`.
- `INSERT table VALUES ...` works without `INTO`.
- `INSERT DELAYED INTO ... VALUES`, `INSERT DELAYED INTO ... SET`, and
  `INSERT DELAYED INTO ... SELECT` execute as ordinary `INSERT` statements.
  The dedicated `INSERT ... SELECT` manual page documents only priority
  modifiers, but MySQL 8.4.9 accepts `DELAYED` for this form and emits the same
  delayed-conversion warning.
- Successful `DELAYED` forms report the ordinary affected-row count and
  `@@warning_count == 1` immediately after the statement.
- Immediately after a `DELAYED` statement, `SHOW WARNINGS` reports warning
  code `3005` with message `INSERT DELAYED is no longer supported. The
  statement was converted to INSERT.`
- If a `DELAYED` statement later fails ordinary value validation, MySQL records
  the delayed-warning row and the error row in `SHOW WARNINGS`.
- MySQL rejects repeated or mixed priority/delayed modifier words, such as
  `INSERT LOW_PRIORITY HIGH_PRIORITY ...`, with syntax error `1064`.
- MySQL accepts `INSERT LOW_PRIORITY IGNORE ...`, but `IGNORE` changes error
  demotion semantics and remains outside this slice.
- MySQL rejects modifier placement after `INTO`, such as
  `INSERT INTO LOW_PRIORITY table ...`, with syntax error `1064`.

## Scope

The implementation must add:

- parser and AST support for one optional `INSERT` modifier token:
  `LOW_PRIORITY`, `HIGH_PRIORITY`, or `DELAYED`;
- modifier support on existing supported `INSERT ... VALUES`,
  `INSERT ... SET`, and `INSERT ... SELECT` forms only;
- optional `INTO` for the existing `INSERT ... VALUES` form, matching the
  existing optional-`INTO` behavior for `INSERT ... SET` and
  `INSERT ... SELECT`;
- `LOW_PRIORITY` and `HIGH_PRIORITY` as no-ops that produce no warnings;
- `DELAYED` as a no-op that appends one warning before ordinary planning and
  execution;
- warning code `3005`, SQLSTATE `HY000`, level `Warning`, and the MySQL-shaped
  delayed conversion message;
- result warning-count propagation for successful delayed statements;
- delayed warning preservation when ordinary `INSERT` validation later fails;
- existing descriptor-driven planning, conversion, physical SQL generation,
  statement atomicity, and affected-row behavior unchanged;
- MySQL-runtime expectation artifacts and focused C tests for each admitted
  statement form.

## Non-Goals

This feature must not implement:

- `IGNORE`, warning demotion, non-strict conversion, duplicate-key discard, or
  per-row warning accounting;
- `ON DUPLICATE KEY UPDATE`, primary/unique key descriptors, generated values,
  auto-increment, `LAST_INSERT_ID()` changes, or protocol insert-id metadata;
- partition selection, table aliases, row aliases, `RETURNING`, `INSERT ...
  TABLE`, row constructors, `VALUE` synonym, `DEFAULT` DML values, CTEs, joins,
  arbitrary expressions, or arbitrary SQLite pass-through;
- multiple modifier words, modifier aliases, modifiers after `INTO`, or
  arbitrary modifier ordering;
- storage-engine priority scheduling, table-lock behavior, delayed queues,
  background writes, privilege semantics, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement dispatch,
  result-handle ownership, public misuse behavior, and failure cleanup.
- Lexer/parser/AST own syntax admission for the optional modifier and store it
  as AST metadata independent of runtime, catalog, storage, and SQLite.
- Runtime execution owns translating `LOW_PRIORITY` and `HIGH_PRIORITY` to
  no-ops and appending the deprecated delayed warning before ordinary
  planning/execution.
- Statement context and diagnostics own warning storage, previous diagnostics,
  `@@warning_count`, `SHOW WARNINGS`, and result warning-count propagation.
- Existing insert planners own descriptor-driven target/source resolution,
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
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO]
    table_name [(column_name[, ...])]
    VALUES (value[, ...])[, ...]

INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO]
    table_name
    SET column_name = value[, ...]

INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO]
    table_name [(column_name[, ...])]
    SELECT ...
```

The `VALUES`, `SET`, and `SELECT` tails remain exactly the subsets already
implemented by their respective lifecycle specs.

`table_name` remains the existing one-part or two-part descriptor target name.
Unqualified names use the selected/default schema policy; schema-qualified
names use the named schema. Reserved `_mylite_*` target names, unknown schemas,
unknown tables, unsupported object kinds, unknown columns, nullability errors,
integer range errors, and physical SQLite failures keep the existing
descriptor-driven `INSERT` diagnostics.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
insert_modifier_opt(A) ::= . {
    A = NULL;
}
insert_modifier_opt(A) ::= LOW_PRIORITY(T). {
    A = mylite_sql_parser_make_insert_low_priority_modifier(state, T);
}
insert_modifier_opt(A) ::= HIGH_PRIORITY(T). {
    A = mylite_sql_parser_make_insert_high_priority_modifier(state, T);
}
insert_modifier_opt(A) ::= DELAYED(T). {
    A = mylite_sql_parser_make_insert_delayed_modifier(state, T);
}

insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T)
    insert_column_list_opt(C) VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T)
    insert_column_list_opt(C) VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M);
}

insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M);
}

insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M);
}
```

The parser must append the optional modifier as a trailing statement child, as
the `REPLACE` modifier implementation does, so existing target/source/value
child indexes remain stable.

## Runtime Semantics

`LOW_PRIORITY` has no user-visible effect beyond syntax acceptance for the
supported embedded storage path. It must not alter locks, transaction shape,
affected rows, warning count, generated SQLite SQL, descriptor conversion, or
statement atomicity.

`HIGH_PRIORITY` has no user-visible effect beyond syntax acceptance for the
supported embedded storage path. It must not alter locks, transaction shape,
affected rows, warning count, generated SQLite SQL, descriptor conversion, or
statement atomicity.

`DELAYED` must append one warning before ordinary statement planning. If the
ordinary `INSERT` path succeeds, the result object reports warning count `1`,
`@@warning_count` reads `1`, and `SHOW WARNINGS` exposes the warning until the
next statement replaces the diagnostics snapshot according to the existing
statement-context policy. If ordinary planning or execution fails, the
diagnostics area contains both the delayed warning and the error condition.

The warning text is:

```text
INSERT DELAYED is no longer supported. The statement was converted to INSERT.
```

All supported insert forms still return through the existing public result API
conventions for non-row statements. They produce no result columns or rows,
set affected rows to the inserted-row count already defined by the underlying
insert lifecycle, and set warning count to `0` except for `DELAYED`.

## Physical SQLite Handling

The modifier must not change generated physical SQLite SQL. The existing
insert planners continue to generate descriptor-built statements such as:

```sql
INSERT INTO "<physical_table_name>" ("col1", "col2", ...) VALUES (?1, ?2, ...)
```

or the existing `INSERT ... SELECT` temp-table and final insert shapes.

Generated SQLite identifiers remain quoted. Values, defaults, predicates, and
limits remain bound parameters where the existing insert/select paths require
parameters. Logical integer and `NULL` conversion remains MyLite-owned before
binding or validation. The `.mylite` preamble and shifted SQLite payload
invariants are unchanged.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- repeated or mixed modifiers;
- unsupported modifier placement, such as after `INTO`;
- unsupported `IGNORE` combinations;
- `DELAYED` warning allocation failure;
- all existing `INSERT ... VALUES`, `INSERT ... SET`, and
  `INSERT ... SELECT` diagnostics unchanged after modifier handling.

Successful `LOW_PRIORITY` and `HIGH_PRIORITY` statements must have warning
count `0`. Successful `DELAYED` statements must have warning count `1`.

Unsupported `IGNORE` remains rejected because it requires warning demotion and
conversion semantics outside this baseline. MySQL accepts
`INSERT LOW_PRIORITY IGNORE ...`; MyLite must keep that combination out of the
supported surface until `IGNORE` is specified and implemented.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, using a new
`runtime_insert_modifier_lifecycle` test if that keeps coverage clearer than
expanding existing insert lifecycle tests. Add a MySQL-runtime expectation
script for modifier behavior.

Coverage must include:

- `LOW_PRIORITY` on `VALUES`, `SET`, and `SELECT` forms;
- `HIGH_PRIORITY` on `VALUES`, `SET`, and `SELECT` forms;
- `DELAYED` on `VALUES`, `SET`, and `SELECT` forms;
- optional `INTO` for `INSERT ... VALUES` with and without a modifier;
- affected rows, `ROW_COUNT()`, result warning count, `@@warning_count`, and
  stored rows for each successful form;
- `SHOW WARNINGS` result for delayed statements;
- delayed warning retained with a subsequent ordinary validation error;
- schema-qualified and unqualified target table resolution through the
  existing insert paths;
- unchanged descriptor-driven defaults, nullability, integer range, and
  statement-atomicity behavior;
- syntax rejection for repeated/mixed modifier words, unsupported `IGNORE`,
  and unsupported modifier placement;
- existing insert values, insert set, insert select, parser, diagnostics, show
  warnings, row count, statement-context, and replace modifier tests continue
  to pass.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` must describe only
this limited modifier support. They must not claim full `INSERT`, `IGNORE`,
`ON DUPLICATE KEY UPDATE`, partitions, delayed queues, table-lock priority
scheduling, generated ids, full `INSERT ... SELECT`, or warning demotion.
