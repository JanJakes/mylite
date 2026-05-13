# Baseline Composite String Primary Key Lifecycle Tasks

- [x] Read project guidance, existing primary-key/index specs, compatibility
      docs, and relevant runtime/catalog code.
- [x] Verify MySQL 8.4.9 behavior for composite string and mixed primary keys,
      metadata, key-length limits, duplicate diagnostics, and `ALTER TABLE ...
      ADD PRIMARY KEY`.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [x] Implement create-time composite `CHAR` / `VARCHAR` and mixed primary-key
      validation.
- [x] Implement `ALTER TABLE ... ADD PRIMARY KEY` composite string/mixed
      validation.
- [x] Preserve descriptor-owned metadata and generated SQLite unique-index
      behavior.
- [x] Add focused C runtime tests for create, alter-add, DML enforcement,
      metadata, persistence, and unsupported forms.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run the feature MySQL expectation script.
- [x] Run targeted CTest entries covering primary keys, char/varchar keys,
      indexes, DML, and metadata.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, push `main`, and run a subagent
      release-gate review.
