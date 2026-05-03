# SHOW diagnostics

## Scope

This slice implements MySQL-compatible diagnostic introspection for the current
MyLite handle diagnostics:

- `SHOW WARNINGS`
- `SHOW ERRORS`
- `SHOW COUNT(*) WARNINGS`
- `SHOW COUNT(*) ERRORS`
- `SHOW WARNINGS LIMIT row_count`
- `SHOW WARNINGS LIMIT offset, row_count`
- `SHOW WARNINGS LIMIT row_count OFFSET offset`
- The same three `LIMIT` forms for `SHOW ERRORS`

`SHOW WARNINGS` displays all stored conditions in the current diagnostics area.
This slice stores warning, error, and the currently implemented note
conditions. MyLite does not yet implement the `sql_notes` system variable, so
note generation is limited to compatibility paths that explicitly record a
MySQL note.

`SHOW ERRORS` displays only stored error conditions. Existing MyLite warning
records remain visible to `SHOW WARNINGS` and hidden from `SHOW ERRORS`.

`SHOW COUNT(*) WARNINGS` returns the count of stored warning, error, and note
conditions. `SHOW COUNT(*) ERRORS` returns only stored error conditions. MyLite
does not yet maintain a separate generated-but-not-stored diagnostics counter,
so the count variants report stored conditions. This matches the current MyLite
diagnostics storage model and can be expanded when `max_error_count` and
complete MySQL condition storage are implemented.

## MySQL 8.4.9 Research

Official MySQL 8.4 documentation for `SHOW WARNINGS` and `SHOW ERRORS`
documents the accepted forms, the shared `LIMIT` syntax, and the distinction
between diagnostic and nondiagnostic statements:

- `SHOW WARNINGS [LIMIT [offset,] row_count]`
- `SHOW COUNT(*) WARNINGS`
- `SHOW ERRORS [LIMIT [offset,] row_count]`
- `SHOW COUNT(*) ERRORS`

The same documentation states that `SHOW WARNINGS` reports errors, warnings,
and notes from the current session diagnostics; `SHOW ERRORS` reports only
errors; diagnostic `SHOW` statements do not clear the message list; and
`SELECT @@warning_count` / `SELECT @@error_count` are nondiagnostic statements
that clear it.

Runtime probes were run against local MySQL 8.4.9 container
`mylite-mysql-849`.

Observed result shapes:

- `SHOW WARNINGS` columns: `Level`, `Code`, `Message`
- `SHOW ERRORS` columns: `Level`, `Code`, `Message`
- `SHOW COUNT(*) WARNINGS` column: `@@session.warning_count`
- `SHOW COUNT(*) ERRORS` column: `@@session.error_count`

Observed lifecycle and syntax:

- Empty diagnostics produce an empty `SHOW WARNINGS` / `SHOW ERRORS` result and
  count variants return `0`.
- `SELECT 1/0` produces one warning row with level `Warning`, code `1365`, and
  message `Division by 0`; `SHOW ERRORS` remains empty.
- A missing-table `SELECT` error produces one `Error` row with code `1146`.
  `SHOW WARNINGS`, `SHOW ERRORS`, count variants, and all supported
  `SHOW ERRORS LIMIT` forms preserve and report that error until a subsequent
  successful nondiagnostic statement, such as `SELECT 1`, clears it.
- Unknown-column errors produce an `Error` row with code `1054`.
- Missing savepoint errors produce an `Error` row with code `1305`.
- `SHOW WARNINGS LIKE 'x'`, `SHOW ERRORS WHERE Code = 1`, and
  `SHOW COUNT(*) WARNINGS LIMIT 1` are syntax errors.
- `SHOW WARNINGS LIMIT -1` and `SHOW WARNINGS LIMIT '1'` are syntax errors.
- `DROP TABLE IF EXISTS no_such` produces one note row with level `Note`, code
  `1051`, and message `Unknown table 'schema.no_such'`; `SHOW ERRORS` remains
  empty. `SHOW WARNINGS LIMIT 0`, `LIMIT 1, 0`, `LIMIT 1, 1`, and
  `LIMIT 0 OFFSET 1` return no rows, while `LIMIT 18446744073709551615` returns
  the one stored condition.
- `CREATE TABLE IF NOT EXISTS existing_table (...)` produces note `1050`.
- `CREATE DATABASE IF NOT EXISTS existing_schema` produces note `1007`.

## Syntax

MyLite should accept only the MySQL-supported diagnostic forms in this slice.
`LIKE` and `WHERE` are not part of the MySQL syntax for these statements and
must remain syntax errors.

Intended MyLite Lemon-syntax grammar:

```lemon
show_diagnostics_statement(A) ::= SHOW(T) diagnostic_condition_kind(K)
        opt_diagnostic_limit(L). {
    A = mylite_sql_parser_make_show_diagnostics_statement(state, T, K, L);
}

show_diagnostics_statement(A) ::= SHOW(T) COUNT LP STAR RP diagnostic_condition_kind(K). {
    A = mylite_sql_parser_make_show_diagnostics_count_statement(state, T, K);
}

diagnostic_condition_kind(A) ::= WARNINGS(T). {
    A = mylite_sql_parser_make_diagnostic_condition_kind(
        T, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_WARNINGS);
}

diagnostic_condition_kind(A) ::= ERRORS(T). {
    A = mylite_sql_parser_make_diagnostic_condition_kind(
        T, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_ERRORS);
}

opt_diagnostic_limit(A) ::= . {
    A = NULL;
}

opt_diagnostic_limit(A) ::= limit_clause(B). {
    A = B;
}
```

`limit_clause` is the existing MyLite `LIMIT row_count`,
`LIMIT offset, row_count`, and `LIMIT row_count OFFSET offset` grammar. Its
literal unsigned integer validation is reused, including acceptance of
`18446744073709551615` and rejection of negative, string, decimal, expression,
`NULL`, marker, and overflow bounds.

## AST And Runtime Semantics

Add two AST statement nodes:

- `show_diagnostics_statement`
- `show_diagnostics_count_statement`

Both statements carry a condition kind:

- `warnings`: include all stored conditions
- `errors`: include only error-level conditions

The row statement optionally carries the existing normalized `limit_clause`
child, whose first child is the offset and second child is the row count. An
omitted `LIMIT` has offset `0` and no row-count cap. A count statement has no
`LIMIT` child.

At runtime, these statements produce SQLite-backed result sets generated from
the current handle-owned diagnostics area:

- Rows have `Level`, `Code`, and `Message` columns, in stored condition order.
- Warning conditions use level text `Warning`.
- Error conditions use level text `Error`.
- Note conditions use level text `Note`.
- Count statements return a single integer row with the exact MySQL column
  names listed above.

Diagnostic `SHOW` statements must not clear or replace diagnostics during
prepare or step. This is important because MyLite prepares statements before
execution; preparing `SHOW WARNINGS` after a warning must not erase the warning
before the `SHOW` statement can read it.

Successful nondiagnostic statements clear the current diagnostics before their
own execution and then store any warnings, errors, or notes they generate.
Failed parse, prepare, or execute attempts clear the previous diagnostics and
store error conditions for the failure when MyLite has a concrete MySQL error
code for the path. Existing validation paths that already record a condition
with the current error message are promoted to error-level conditions,
preserving their MySQL code. When an internal path has no specific MySQL error
code, this slice records generic server error code `1105` with the handle error
message.

## Metadata

`SHOW WARNINGS` and `SHOW ERRORS` metadata:

| Column | Type |
| --- | --- |
| `Level` | text |
| `Code` | integer |
| `Message` | text |

`SHOW COUNT(*) WARNINGS` metadata:

| Column | Type |
| --- | --- |
| `@@session.warning_count` | integer |

`SHOW COUNT(*) ERRORS` metadata:

| Column | Type |
| --- | --- |
| `@@session.error_count` | integer |

## Tests

Parser tests cover:

- Accepted row and count forms for both warnings and errors.
- `LIMIT row_count`, `LIMIT offset, row_count`, and
  `LIMIT row_count OFFSET offset` on row forms.
- Rejection of `LIKE`, `WHERE`, count-form `LIMIT`, negative limits, string
  limits, and malformed count syntax.

Runtime tests cover:

- Empty diagnostics row and count result shapes.
- Existing expression warning generation through `SELECT 1/0`.
- Existing note generation through `DROP TABLE IF EXISTS` on a missing table,
  `CREATE TABLE IF NOT EXISTS` on an existing table, and
  `CREATE DATABASE IF NOT EXISTS` on an existing schema.
- Existing DML/DDL warning generation, including deprecated `REPLACE DELAYED`
  and duplicate-index warnings where available in the current runtime.
- `SHOW WARNINGS` includes warnings and errors; `SHOW ERRORS` includes only
  error-level conditions, including mapped missing-table, unknown-column, and
  missing-savepoint errors.
- LIMIT/OFFSET edge cases including zero-row windows and
  `18446744073709551615`.
- Diagnostic `SHOW` preservation across repeated row/count forms.
- Clearing after a successful nondiagnostic statement.
- Error diagnostics after a failed statement.

## Deferred Work

- `GET DIAGNOSTICS` and stacked diagnostics.
- SQL `@@warning_count`, `@@error_count`, `max_error_count`, and `sql_notes`
  variables.
- Broader note-level condition generation outside the paths implemented in this
  slice.
- Separate generated-condition counters when `max_error_count` caps stored
  condition rows.
- Complete MySQL numeric error-code and SQLSTATE propagation for every MyLite
  error path.
- Protocol OK/ERR packet warning count and diagnostics integration.
