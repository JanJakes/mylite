# Baseline Replication Control Placeholders Tasks

- [x] Record official MySQL 8.4 references and MySQL 8.4.9 runtime probes for
      source, replica, and Group Replication control statements.
- [x] Specify MyLite's embedded no-op behavior and intentionally unsupported
      replication side effects.
- [x] Extend the placeholder classifier for `CHANGE REPLICATION FILTER`,
      `START REPLICA`, `STOP REPLICA`, `START GROUP_REPLICATION`, and
      `STOP GROUP_REPLICATION`.
- [x] Add parser and runtime coverage for representative accepted forms and
      removed-alias rejection.
- [x] Update baseline and detailed compatibility documentation.
- [x] Run focused parser/runtime verification, diff checks, and the release
      gate before committing.
