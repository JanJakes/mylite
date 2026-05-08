# Baseline Server Character Set System Variables Tasks

Add the narrow scalar system-variable slice for MyLite's fixed server charset
defaults: `@@character_set_server` and `@@collation_server`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, quoted-name behavior,
     diagnostics, and statement-diagnostics interactions.
   - Specify fixed MyLite values: `utf8mb4` and `utf8mb4_0900_ai_ci`.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `character_set_server` and `collation_server` to the existing
     system-variable resolver.
   - Return the fixed server defaults for no-scope, `session`, `local`, and
     `global` forms.
   - Preserve existing unknown-variable and quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, catalog, storage, VFS, or
     SQLite integration.

3. Runtime tests
   - Extend the focused character-set system variable runtime test or add a
     similarly focused test binary.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, mixed
     diagnostics variables, warning/error clearing, unknown names,
     quoted-scope rejection, persistence, preamble preservation, unchanged
     generations, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, and wider forms relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update character-set and collation detail docs.
   - Do not claim `SET`, `SHOW VARIABLES`, mutable server defaults, database
     defaults, conversion, string semantics, or collation comparisons.

6. Verification
   - Run focused CTest entries for parser and runtime system variables.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables,
  `character_set_database`, `collation_database`, `character_set_system`,
  `SHOW VARIABLES`, client charset negotiation, wire-protocol metadata,
  string types, character conversion, collation coercibility, table-backed
  evaluation, aliases, clauses, arbitrary expressions, SQLite SQL, catalog
  mutations, or SQLite fork patches.
