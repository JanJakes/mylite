# Baseline INFORMATION_SCHEMA Role Session Tables Tasks

- [x] Verify MySQL 8.4.9 documentation, baseline rows, status, diagnostics,
  and system metadata for role-session views.
- [x] Add MySQL expectation script for
  `ADMINISTRABLE_ROLE_AUTHORIZATIONS`, `APPLICABLE_ROLES`, and
  `ENABLED_ROLES`.
- [x] Add the three role-session views to the synthetic information-schema
  table definitions.
- [x] Keep the current slice rowless until role graph and active-role
  descriptors exist.
- [x] Add focused C runtime tests for queries, predicates, metadata, selected
  `information_schema` resolution, and diagnostics.
- [x] Update compatibility docs.
- [x] Run focused tests, MySQL expectation script, `git diff --check`, and the
  full `cmake --workflow --preset check`.
- [x] Review, fix findings, commit, and push.
