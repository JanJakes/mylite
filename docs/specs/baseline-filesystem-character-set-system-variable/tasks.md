# Baseline Filesystem Character Set System Variable Tasks

Add the narrow scalar and assignment system-variable slice for MyLite's
file-name character-set metadata placeholder: `@@character_set_filesystem`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability, quoted-name
     behavior, diagnostics, and statement-diagnostics interactions.
   - Specify default MyLite value `binary`, session/local/unscoped assignment,
     and fixed-global limitations.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `character_set_filesystem` to the existing system-variable resolver.
   - Return default `binary`, session-local overrides, and fixed global
     readback.
   - Validate `SET` values against MySQL charset names and integer collation
     IDs, then store canonical session readback.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, catalog, or
     SQLite integration.

3. Runtime tests
   - Extend the focused character-set system variable runtime test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mutable session assignment, `SHOW VARIABLES` readback,
     mixed scalar reads, warning/error clearing, unknown names, assignment
     diagnostics, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, session mutability, collation-ID assignment, and wider forms
     relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/character-sets.md`.
   - Update `docs/compatibility/sql-file-output.md`.
   - Do not claim general `binary` support, mutable global variable state,
     server-side file operations, or file-name conversion.

6. Verification
   - Run focused CTest entries for parser and runtime system variables.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement mutable global state, startup options, persisted variables,
  `character_sets_dir`, `character_set_system`,
  `default_collation_for_utf8mb4`, string storage/conversion/collation
  semantics, `LOAD DATA`, `SELECT ... INTO OUTFILE`, `LOAD_FILE()`,
  `INFORMATION_SCHEMA` variable tables, table-backed evaluation, aliases,
  clauses, arbitrary expressions, SQLite SQL, catalog mutations, or SQLite fork
  patches.
