# Baseline Server Character Set System Variables Tasks

Add the server character-set system-variable readback and assignment baseline:
`@@character_set_server` and `@@collation_server`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, `SHOW VARIABLES` rows,
     assignment forms, warnings, diagnostics, quoted-name behavior, and
     statement-diagnostics interactions.
   - Specify fixed MyLite values: `utf8mb4` and `utf8mb4_0900_ai_ci`.
   - Specify handle-local session/local assignment state and fixed-global
     exact/default no-op policy.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `character_set_server` and `collation_server` to the existing
     system-variable resolver.
   - Return fixed server defaults for global reads and current handle-local
     values for no-scope, `session`, and `local` forms.
   - Add assignment handling for `DEFAULT`, charset/collation names, aliases,
     integer collation IDs, and integer user variables.
   - Keep value-changing global assignments rejected while accepting
     exact/default no-op global forms.
   - Preserve existing unknown-variable and quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, catalog, storage, VFS, or
     SQLite integration.

3. Runtime tests
   - Extend the focused character-set system variable runtime test or add a
     similarly focused test binary.
   - Cover values, labels, scopes, quoted final names, `SHOW VARIABLES`,
     assignment forms, coupled readback, warnings, diagnostics, global no-op
     forms, `FROM DUAL`, mixed diagnostics variables, warning/error clearing,
     unknown names, quoted-scope rejection, persistence, preamble
     preservation, unchanged generations, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     assignment warnings, assignment diagnostics, and wider forms relevant to
     this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update character-set and collation detail docs.
   - Do not claim mutable server-global defaults, persisted state, startup
     options, conversion, string semantics, or collation comparisons.

6. Verification
   - Run focused CTest entries for parser and runtime system variables.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement value-changing shared `GLOBAL` assignments, startup options,
  persisted variables, `character_set_database`, `collation_database`,
  `character_set_system`, client charset negotiation, wire-protocol metadata,
  string types, character conversion, collation coercibility, table-backed
  evaluation, clauses, arbitrary expressions, SQLite SQL, catalog mutations, or
  SQLite fork patches.
