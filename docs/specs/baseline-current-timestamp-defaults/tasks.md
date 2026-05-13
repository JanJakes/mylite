# Baseline Current Timestamp Defaults Tasks

- [x] Audit existing timestamp/datetime, default, update, SQL-mode, and
      statement-time support.
- [x] Verify MySQL 8.4.9 behavior for current timestamp functions, `SET
      timestamp`, defaults, auto-update columns, metadata, and backfill.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the supported subset.
- [ ] Implement parser and AST support for current timestamp value forms and
      `ON UPDATE CURRENT_TIMESTAMP` column attributes.
- [ ] Bump the catalog schema and persist current-timestamp default and
      auto-update metadata.
- [ ] Implement statement-time materialization, limited `SET timestamp`, scalar
      projection, DML defaults, explicit temporal assignments, and automatic
      update behavior.
- [ ] Update descriptor introspection, `SHOW`, and compatibility docs.
- [ ] Add focused fast C runtime tests and register any new test binary.
- [ ] Run focused MySQL expectations, build, focused CTest entries, and full
      `cmake --workflow --preset check`.
- [ ] Review the final diff with a subagent, fix findings, commit, push, and
      continue.
