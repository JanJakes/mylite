# MyLite SQLite Patch Stack

This directory holds ordered patch files for local SQLite fork changes.

The stack is intentionally empty until MyLite needs a SQLite extension point
that cannot be implemented through SQLite's public extension APIs or a MyLite
wrapper. Keep patches narrow, documented, and focused on hooks for MyLite-owned
code.

Patch filenames should use a stable order:

```text
0001-add-mylite-io-offset-hook.patch
0002-add-mylite-result-metadata-hook.patch
```

Before adding or updating a patch, verify that it applies cleanly to the
currently pinned `third_party/sqlite/upstream` tree.
