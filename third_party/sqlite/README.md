# SQLite upstream snapshot

MyLite pins SQLite source explicitly instead of using the system SQLite
library. SQLite is part of the embedded runtime architecture. MyLite should use
public SQLite extension APIs where they fit and carry targeted local SQLite
fork patches only where MySQL compatibility or performance needs extension
points SQLite does not expose cleanly.

## Pinned Release

| Field | Value |
| --- | --- |
| SQLite version | 3.53.0 |
| `SQLITE_VERSION_NUMBER` | 3053000 |
| Download version number | 3530000 |
| Source tree archive | `https://www.sqlite.org/2026/sqlite-src-3530000.zip` |
| Source tree SHA3-256 | `4ffbd00ba8db1e1172dbc69a5203a2c185556a32543e58585ba3713abf676fe5` |
| Amalgamation archive | `https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip` |
| Amalgamation SHA3-256 | `c2325c53b3b41761469f91cfb078e96882ac5d85bac10c11b0bd8f253b031e5b` |

The source tree is the authoritative fork surface. The amalgamation is checked
in as the buildable SQLite artifact for MyLite's default CMake build, avoiding
a Tcl dependency and SQLite's generated-source build pipeline in normal MyLite
builds.

## Layout

| Path | Purpose |
| --- | --- |
| `upstream/` | Pristine SQLite source tree unpacked from `sqlite-src-3530000.zip`. |
| `amalgamation/` | Official SQLite amalgamation unpacked from `sqlite-amalgamation-3530000.zip`. |
| `patches/` | Ordered MyLite patch stack for local SQLite fork changes. Empty until the first hook is needed. |
| `CMakeLists.txt` | Vendored SQLite target. It intentionally does not use MyLite's first-party warning or tidy policy. |
| `source.json` | Machine-readable pin for source, checksums, and patch-stack location. |

SQLite's source archive uses the release download number `3530000`, while
SQLite's runtime/header version macro is `3053000`.

## Build Policy

The CMake target is `mylite_sqlite`, with alias `MyLite::sqlite`.

This target is vendored code:

- It does not inherit first-party `-Wall`, `-Wextra`, `-Wconversion`, or
  clang-tidy policy.
- It is compiled with hidden visibility so SQLite symbols do not become part of
  MyLite's eventual public ABI by accident.
- Loadable extension support is omitted by default with
  `SQLITE_OMIT_LOAD_EXTENSION`. MyLite can still use SQLite's embedded
  extension APIs for functions, collations, virtual tables, and other internal
  registrations.
- SQLite is linked through MyLite's targets; applications should include
  MyLite headers, not SQLite headers.

## Fork Policy

Local SQLite changes must stay small, documented, and focused on exposing
extension points for MyLite. Substantial MySQL compatibility behavior belongs in
MyLite code, not in SQLite.

Use this decision order before adding a patch:

1. Use SQLite's public extension APIs.
2. Wrap SQLite behavior in MyLite code.
3. Add a narrow SQLite hook only when the public API and wrapper strategy are
   insufficient for correctness or avoidable overhead.

Each patch should be independently reviewable, have a focused commit message,
and explain why the hook belongs in SQLite instead of MyLite.

## Refresh Procedure

Download and verify the pinned source tree:

```sh
curl -fsSL -o /tmp/sqlite-src-3530000.zip https://www.sqlite.org/2026/sqlite-src-3530000.zip
openssl dgst -sha3-256 /tmp/sqlite-src-3530000.zip
rm -rf third_party/sqlite/upstream
mkdir -p third_party/sqlite/upstream
unzip -q /tmp/sqlite-src-3530000.zip -d /tmp/sqlite-src
cp -R /tmp/sqlite-src/sqlite-src-3530000/. third_party/sqlite/upstream/
```

Download and verify the matching amalgamation:

```sh
curl -fsSL -o /tmp/sqlite-amalgamation-3530000.zip https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip
openssl dgst -sha3-256 /tmp/sqlite-amalgamation-3530000.zip
rm -rf third_party/sqlite/amalgamation
mkdir -p third_party/sqlite/amalgamation
unzip -q /tmp/sqlite-amalgamation-3530000.zip -d /tmp/sqlite-amalgamation
cp /tmp/sqlite-amalgamation/sqlite-amalgamation-3530000/sqlite3.c third_party/sqlite/amalgamation/sqlite3.c
cp /tmp/sqlite-amalgamation/sqlite-amalgamation-3530000/sqlite3.h third_party/sqlite/amalgamation/sqlite3.h
cp /tmp/sqlite-amalgamation/sqlite-amalgamation-3530000/sqlite3ext.h third_party/sqlite/amalgamation/sqlite3ext.h
cp /tmp/sqlite-amalgamation/sqlite-amalgamation-3530000/shell.c third_party/sqlite/amalgamation/shell.c
```

When MyLite carries SQLite fork patches, refresh into a temporary clean source
tree first, apply `patches/*.patch` in order, and then regenerate or replace the
amalgamation so the build target contains the same SQLite changes.

Useful review commands:

```sh
git diff -- third_party/sqlite/upstream third_party/sqlite/amalgamation third_party/sqlite/patches
git apply --check third_party/sqlite/patches/*.patch
```
