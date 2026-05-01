# SQLite upstream snapshot

MyLite pins SQLite source explicitly instead of using the system SQLite library.
SQLite is part of the embedded runtime architecture and may need targeted local
patches, including support for the `.mylite` file header and shifted SQLite page
offset model.

## Pinned release

| Field | Value |
| --- | --- |
| Version | 3.53.0 |
| Version number | 3530000 |
| Source archive | `https://www.sqlite.org/2026/sqlite-src-3530000.zip` |
| SHA3-256 | `4ffbd00ba8db1e1172dbc69a5203a2c185556a32543e58585ba3713abf676fe5` |

The official SQLite download page identifies `sqlite-src-3530000.zip` as the
canonical source tree for SQLite 3.53.0.

## Vendored files

The repository currently vendors only Lemon from the pinned SQLite source tree:

| Source path | Repository path |
| --- | --- |
| `tool/lemon.c` | `third_party/sqlite/lemon/lemon.c` |
| `tool/lempar.c` | `third_party/sqlite/lemon/lempar.c` |
| `LICENSE.md` | `third_party/sqlite/upstream/LICENSE.md` |

The full SQLite engine source will be added under this directory when the
runtime integration starts. Until then, this pin records the source snapshot
that parser-generation tooling comes from.

## Refresh procedure

```sh
curl -fsSL -o /tmp/sqlite-src-3530000.zip https://www.sqlite.org/2026/sqlite-src-3530000.zip
openssl dgst -sha3-256 /tmp/sqlite-src-3530000.zip
unzip -p /tmp/sqlite-src-3530000.zip sqlite-src-3530000/tool/lemon.c > third_party/sqlite/lemon/lemon.c
unzip -p /tmp/sqlite-src-3530000.zip sqlite-src-3530000/tool/lempar.c > third_party/sqlite/lemon/lempar.c
unzip -p /tmp/sqlite-src-3530000.zip sqlite-src-3530000/LICENSE.md > third_party/sqlite/upstream/LICENSE.md
```
