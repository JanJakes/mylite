# SQLite Lemon

This directory contains vendored SQLite Lemon sources used to generate the
prototype MyLite parser:

- `lemon.c`: the Lemon parser generator.
- `lempar.c`: the Lemon parser template.

SQLite's Lemon source is public domain. `lempar.c` has a small local warning
cleanup so the generated MyLite parser builds cleanly with `-Wall -Wextra`.
