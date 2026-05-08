# Baseline Filesystem Character Set System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed file-name
character-set placeholder: `@@character_set_filesystem`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability, quoted-name
     behavior, diagnostics, and statement-diagnostics interactions.
   - Specify fixed MyLite value `binary` for no-scope, `session`, `local`, and
     `global` forms.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `character_set_filesystem` to the existing system-variable resolver.
   - Return fixed value `binary` for all supported scopes.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, catalog, or
     SQLite integration.

3. Runtime tests
   - Extend the focused character-set system variable runtime test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, and wider forms relevant to this
     slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/character-sets.md`.
   - Update `docs/compatibility/sql-file-output.md`.
   - Do not claim general `binary` support, `SET`, mutable variable state,
     `SHOW VARIABLES`, server-side file operations, or file-name conversion.

6. Verification
   - Run focused CTest entries for parser and runtime system variables.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables,
  `character_sets_dir`, `character_set_system`,
  `default_collation_for_utf8mb4`, string storage/conversion/collation
  semantics, `LOAD DATA`, `SELECT ... INTO OUTFILE`, `LOAD_FILE()`,
  `SHOW VARIABLES`, `INFORMATION_SCHEMA` charset tables, table-backed
  evaluation, aliases, clauses, arbitrary expressions, SQLite SQL, catalog
  mutations, or SQLite fork patches.
