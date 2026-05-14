# Baseline JSON Type Tasks

- [x] Review existing descriptor-owned string, binary, enum/set, DML, metadata,
      and result-column patterns.
- [x] Verify MySQL 8.4.9 behavior for JSON DDL, DML, metadata, invalid values,
      defaults, indexes, and canonical readback.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [ ] Add JSON parser/AST column-type support while keeping `JSON` usable as a
      nonreserved identifier.
- [ ] Add MyLite-owned JSON validation and canonicalization for the admitted
      subset.
- [ ] Map JSON to descriptor-owned logical `JSON` and physical SQLite `TEXT`.
- [ ] Integrate JSON conversion into `INSERT`, `REPLACE`, `UPDATE`, DML
      defaults, `INSERT IGNORE`, compatible `INSERT ... SELECT`,
      `REPLACE ... SELECT`, and scalar-subquery `UPDATE`.
- [ ] Add descriptor-backed JSON readback, `IS NULL` predicates, and metadata
      rendering.
- [ ] Reject direct JSON indexes/defaults and unsupported JSON expressions with
      deterministic diagnostics.
- [ ] Add focused C parser/runtime tests and CMake registration.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run the feature MySQL expectation script.
- [ ] Run targeted CTest entries covering parser, JSON, DML, metadata,
      introspection, and file-format safety.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Run a subagent release-gate review, amend any findings, commit
      atomically, and push `main`.
