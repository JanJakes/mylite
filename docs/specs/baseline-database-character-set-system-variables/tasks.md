# Baseline Database Character Set System Variables Tasks

Add the narrow scalar system-variable slice for current database charset
defaults: `@@character_set_database` and `@@collation_database`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, selected-schema behavior,
     dropped-schema fallback, quoted-name behavior, diagnostics, and
     statement-diagnostics interactions.
   - Specify fixed MyLite values: `utf8mb4` and `utf8mb4_0900_ai_ci`.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `character_set_database` and `collation_database` to the existing
     system-variable resolver.
   - Return the fixed database defaults for no-scope, `session`, `local`, and
     `global` forms.
   - Preserve existing unknown-variable and quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, or SQLite
     integration.

3. Runtime tests
   - Extend the focused character-set system variable runtime test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`,
     `DATABASE()`, selected/dropped schema behavior, mixed diagnostics
     variables, warning/error clearing, unknown names, quoted-scope rejection,
     persistence, preamble preservation, unchanged generations, and
     independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, selected-schema behavior, and wider forms relevant to this
     slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update schema, character-set, and collation detail docs.
   - Do not claim `SET`, `CREATE DATABASE` options, `ALTER DATABASE`, mutable
     schema defaults, `SHOW VARIABLES`, conversion, string semantics, or
     collation comparisons.

6. Verification
   - Run focused CTest entries for parser and runtime system variables.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables,
  `CREATE DATABASE` charset/collation options, `ALTER DATABASE`,
  non-default character sets/collations, `SHOW VARIABLES`, client charset
  negotiation, wire-protocol metadata, string types, character conversion,
  collation coercibility, table-backed evaluation, aliases, clauses, arbitrary
  expressions, SQLite SQL, catalog mutations, or SQLite fork patches.
