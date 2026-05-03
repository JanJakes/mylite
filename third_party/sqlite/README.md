# SQLite upstream snapshot

MyLite pins SQLite source explicitly instead of using the system SQLite library.
SQLite is part of the embedded runtime architecture and may need targeted local
patches, including support for the `.mylite` file header and shifted SQLite page
offset model.

## Pinned experimental branch

| Field | Value |
| --- | --- |
| Branch | `begin-concurrent` |
| Version | 3.54.0 |
| Manifest UUID | `7f954a9e2fa4203b55825dfd70a46ffde7c985a4c8b940208d74d97441f3fd04` |
| Source snapshot | `https://sqlite.org/src/tarball/sqlite-begin-concurrent.tar.gz?r=begin-concurrent` |
| Source snapshot SHA3-256 | `64921d9d0a85cfd520a600171b1a943f19b0892226b20bd7d853026bdaf07279` |

This branch pin is an experimental MyLite prototype for evaluating SQLite's
`BEGIN CONCURRENT` support. It is not a canonical SQLite release.

## Vendored files

The repository vendors Lemon from the pinned SQLite source tree and a generated
SQLite engine amalgamation from the same source snapshot:

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
curl -fsSL -o /tmp/sqlite-begin-concurrent.tar.gz \
  'https://sqlite.org/src/tarball/sqlite-begin-concurrent.tar.gz?r=begin-concurrent'
openssl dgst -sha3-256 /tmp/sqlite-begin-concurrent.tar.gz
tar -xzf /tmp/sqlite-begin-concurrent.tar.gz -C /tmp

cd /tmp/sqlite-begin-concurrent
./configure --disable-shared --disable-load-extension
make sqlite3.c

cd /path/to/MyLite
cp /tmp/sqlite-begin-concurrent/tool/lemon.c third_party/sqlite/lemon/lemon.c
cp /tmp/sqlite-begin-concurrent/tool/lempar.c third_party/sqlite/lemon/lempar.c
cp /tmp/sqlite-begin-concurrent/LICENSE.md third_party/sqlite/upstream/LICENSE.md
cp /tmp/sqlite-begin-concurrent/sqlite3.c third_party/sqlite/upstream/sqlite3.c
cp /tmp/sqlite-begin-concurrent/sqlite3.h third_party/sqlite/upstream/sqlite3.h
cp /tmp/sqlite-begin-concurrent/sqlite3ext.h third_party/sqlite/upstream/sqlite3ext.h
```
