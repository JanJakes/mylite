# SQLite upstream snapshot

MyLite pins SQLite source explicitly instead of using the system SQLite library.
SQLite is part of the embedded runtime architecture and may need targeted local
patches, including support for the `.mylite` file header and shifted SQLite page
offset model.

## Pinned release

| Field | Value |
| --- | --- |
| Version | 3.53.0 |
| Version number | 3053000 |
| Download archive number | 3530000 |
| Source archive | `https://www.sqlite.org/2026/sqlite-src-3530000.zip` |
| SHA3-256 | `4ffbd00ba8db1e1172dbc69a5203a2c185556a32543e58585ba3713abf676fe5` |
| Amalgamation archive | `https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip` |
| Amalgamation SHA3-256 | `c2325c53b3b41761469f91cfb078e96882ac5d85bac10c11b0bd8f253b031e5b` |

The official SQLite download page identifies `sqlite-src-3530000.zip` as the
canonical source tree for SQLite 3.53.0.

## Vendored files

The repository vendors Lemon from the pinned SQLite source tree and the SQLite
engine amalgamation from the matching release:

| Source path | Repository path |
| --- | --- |
| `tool/lemon.c` | `third_party/sqlite/lemon/lemon.c` |
| `tool/lempar.c` | `third_party/sqlite/lemon/lempar.c` |
| `LICENSE.md` | `third_party/sqlite/upstream/LICENSE.md` |
| `sqlite3.c` | `third_party/sqlite/upstream/sqlite3.c` |
| `sqlite3.h` | `third_party/sqlite/upstream/sqlite3.h` |
| `sqlite3ext.h` | `third_party/sqlite/upstream/sqlite3ext.h` |

## Refresh procedure

```sh
curl -fsSL -o /tmp/sqlite-src-3530000.zip https://www.sqlite.org/2026/sqlite-src-3530000.zip
openssl dgst -sha3-256 /tmp/sqlite-src-3530000.zip
unzip -p /tmp/sqlite-src-3530000.zip sqlite-src-3530000/tool/lemon.c > third_party/sqlite/lemon/lemon.c
unzip -p /tmp/sqlite-src-3530000.zip sqlite-src-3530000/tool/lempar.c > third_party/sqlite/lemon/lempar.c
unzip -p /tmp/sqlite-src-3530000.zip sqlite-src-3530000/LICENSE.md > third_party/sqlite/upstream/LICENSE.md

curl -fsSL -o /tmp/sqlite-amalgamation-3530000.zip https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip
openssl dgst -sha3-256 /tmp/sqlite-amalgamation-3530000.zip
unzip -p /tmp/sqlite-amalgamation-3530000.zip sqlite-amalgamation-3530000/sqlite3.c > third_party/sqlite/upstream/sqlite3.c
unzip -p /tmp/sqlite-amalgamation-3530000.zip sqlite-amalgamation-3530000/sqlite3.h > third_party/sqlite/upstream/sqlite3.h
unzip -p /tmp/sqlite-amalgamation-3530000.zip sqlite-amalgamation-3530000/sqlite3ext.h > third_party/sqlite/upstream/sqlite3ext.h
```
