# Baseline Named Window Definitions Tasks

- [x] Research MySQL 8.4 named-window syntax and runtime diagnostics.
- [x] Capture MySQL 8.4.9 runtime expectations for direct references,
  inheritance, duplicate names, missing names, cycles, and duplicate inherited
  properties.
- [x] Reuse existing MyLite parser AST nodes for named window definitions and
  references.
- [x] Add row-scalar select planning support for statement-local named window
  resolution.
- [x] Lower resolved named windows through the existing inline window-function
  planning and SQLite rendering path.
- [x] Add focused C runtime coverage.
- [x] Update compatibility documentation.
