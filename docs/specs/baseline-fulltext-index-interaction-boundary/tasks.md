# Baseline FULLTEXT Index Interaction Boundary Tasks

- [x] Research MySQL 8.4 fulltext index/search interaction from official docs.
- [x] Confirm existing MyLite fulltext metadata specs and MySQL expectation
  scripts cover descriptor behavior.
- [x] Document the MyLite boundary between metadata-only descriptors and
  unsupported executable full-text search.
- [x] Add a runtime assertion that `MATCH ... AGAINST` remains a deterministic
  unsupported diagnostic even when a matching `FULLTEXT` descriptor exists.
- [x] Update detailed and baseline compatibility links/status.
- [x] Run focused tests and full release checks.
