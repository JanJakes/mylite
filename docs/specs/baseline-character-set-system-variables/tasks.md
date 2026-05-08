# Baseline Character Set System Variables Tasks

Add a narrow read-only scalar system-variable slice for the baseline connection
character-set placeholders:

- `@@character_set_client`
- `@@character_set_connection`
- `@@character_set_results`
- `@@collation_connection`

## Scope Checklist

1. Research and expectations
   - Verify MySQL 8.4.9 behavior with `--default-character-set=utf8mb4`.
   - Record result labels, values, accepted scopes, quoted-name behavior,
     quoted-scope rejection, unknown-variable diagnostics, warning count, error
     count, and row-count behavior.
   - Add `packages/libmylite/tests/mysql_baseline_character_set_system_variables_expectations.sh`.

2. Parser and AST
   - Reuse the existing `SYSTEM_VARIABLE` expression grammar.
   - Add parser tests only if existing generic system-variable coverage is
     insufficient for the newly supported forms.

3. Runtime implementation
   - Generalize the system-variable resolver so one path can resolve
     diagnostics count variables and charset/collation variables.
   - Support no-scope, `session`, `local`, and `global` forms for the four
     charset/collation variables.
   - Return values from `mylite_session_state`.
   - Preserve source-span result labels.
   - Preserve existing diagnostics count variable behavior and diagnostics.
   - Preserve nondiagnostic scalar-select statement lifecycle semantics.

4. Runtime tests
   - Add `runtime_character_set_system_variables_test.c`.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, mixed
     scalar variables, `ROW_COUNT()`, diagnostics clearing, unknown variables,
     quoted scopes, independent handles, file-backed preamble preservation, and
     unchanged catalog/schema generations.
   - Register the test in `packages/libmylite/CMakeLists.txt` with a dotted
     CTest name.

5. Compatibility documentation
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/character-sets.md`.
   - Update `docs/compatibility/collations.md`.
   - Do not claim `SET`, `SET NAMES`, conversion, string types, full
     collations, or `SHOW VARIABLES`.

6. Verification
   - `cmake --build --preset dev`
   - Focused CTest for parser and runtime scalar/system-variable tests.
   - MySQL expectation script for this feature.
   - `cmake --workflow --preset check`
   - `git diff --check`

## Out Of Scope

- `SET`, `SET NAMES`, `SET CHARACTER SET`, mutable character-set state, or
  protocol charset negotiation.
- `SHOW VARIABLES`, Performance Schema, or `INFORMATION_SCHEMA` variable or
  charset/collation tables.
- Variables other than the four listed in this feature.
- String type support, string conversion, collation coercibility, or comparison
  behavior.
- SQLite metadata reads, catalog mutations, storage mutations, and SQLite fork
  patches.
