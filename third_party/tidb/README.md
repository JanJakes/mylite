# TiDB Parser Grammar

`parser.y` is a vendored copy of TiDB's MySQL-compatible parser grammar. MyLite
uses it as the source grammar for the Lemon parser prototype through
`scripts/port_tidb_lemon.py`.

TiDB is licensed under the Apache License 2.0. The source file also retains its
upstream copyright and license header.

This vendored file should stay close to upstream. MyLite-specific syntax changes
belong in the generator overlay, not as direct edits to `parser.y`, unless a
future import process explicitly records a patch stack.
