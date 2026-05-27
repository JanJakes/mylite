# Baseline Version System Variables Tasks

Add a narrow read-only scalar system-variable slice for MyLite version
identity:

- `@@version`
- `@@version_comment`

## Scope Checklist

1. Research and expectations
   - Verify MySQL 8.4.9 behavior for `@@version` and
     `@@version_comment`.
   - Record result labels, values, accepted scopes, rejected scopes,
     quoted-name behavior, quoted-scope rejection, unknown-variable
     diagnostics, warning count, error count, and row-count behavior.
   - Add
     `packages/libmylite/tests/mysql_baseline_version_system_variables_expectations.sh`.

2. Parser and AST
   - Reuse the existing `SYSTEM_VARIABLE` expression grammar.
   - Add parser tests only if existing generic system-variable coverage is
     insufficient for this slice.

3. Runtime implementation
   - Extend the system-variable resolver to recognize `version` and
     `version_comment`.
   - Support no-scope and `global` forms for both variables.
   - Reject `session` and `local` forms with MySQL error `1238`.
   - Return the SQL-visible MySQL 8.4.9 compatibility value for `@@version`.
   - Return the SQL-visible MySQL 8.4.9 community-server comment for
     `@@version_comment`.
   - Preserve source-span result labels.
   - Preserve existing diagnostics count and character-set variable behavior.
   - Preserve nondiagnostic scalar-select statement lifecycle semantics.

4. Runtime tests
   - Extend `runtime_version_function_test.c` or add a focused runtime test if
     that keeps the coverage clearer.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, mixed
     scalar variables/functions, `ROW_COUNT()`, diagnostics clearing, rejected
     session/local scope, unknown variables, quoted scopes, independent
     handles, file-backed preamble preservation, and unchanged catalog/schema
     generations.
   - Register any new test in `packages/libmylite/CMakeLists.txt` with a
     dotted CTest name.

5. Compatibility documentation
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/functions-system.md` to remove the old
     `@@version` non-goal from the `VERSION()` row.
   - Do not claim protocol handshake behavior, configurable server-version
     identity, Performance Schema variable tables, or changes to the existing
     fixed version-compile placeholder slice.

6. Verification
   - `cmake --build --preset dev`
   - Focused CTest for parser and runtime scalar/system-variable tests.
   - MySQL expectation script for this feature.
   - `cmake --workflow --preset check`
   - `git diff --check`

## Out Of Scope

- Protocol handshake version reporting, configurable server-version/build
  comment identity, or changes to the existing fixed version-compile
  placeholder slice.
- `SHOW VARIABLES`, Performance Schema, or `INFORMATION_SCHEMA` variable
  tables.
- Variables other than `version` and `version_comment`.
- `SET`, persisted variables, dynamic assignment, and privilege checks.
- General expression evaluation involving variables.
- SQLite metadata reads, catalog mutations, storage mutations, and SQLite fork
  patches.
