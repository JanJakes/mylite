# MyLite

**MySQL in a single file.**

> [!NOTE]
> **Status:** Early development. See: [COMPATIBILITY.md](COMPATIBILITY.md)

## Overview

MyLite is an embedded MySQL drop-in built on a bundled SQLite foundation.

At a glance:

| 💡 | ℹ️ |
| --- | --- |
| **Compatibility** | MySQL 8 LTS API (currently MySQL 8.4.9) |
| **Storage** | Single `.mylite` file |
| **Engine** | Bundled SQLite with MyLite compatibility layers and targeted extension points |
| **Validation** | Compatibility dashboard & extensive test suite |
| **Repository** | Monorepo for `libmylite`, tooling, protocol support, extensions, and integration wiring |

## Goals

MyLite should power MySQL-oriented applications without modifications. Here's a list of the main goals:

- **MySQL drop-in:** Work as an effortless drop-in replacement for MySQL.
- **Single file:** Keep the database portable as a single `.mylite` file.
- **Uncompromising compatibility:** Implement the MySQL API surface that real applications depend on.
- **Correctness:** Mirror MySQL semantics for expressions, statements, operations, types, values, errors, etc.
- **Extensive test suite:** Create and maintain a large test suite.
- **Coverage matrix:** Track MySQL functionality coverage in a detailed document.
- **Freedom:** Keep the implementation independently authored and free of
  restrictions that would limit our license choices.

## Compatibility

MyLite makes compatibility with MySQL a fundamental principle of the project.
MySQL compatibility is carefully evaluated, tracked, and covered with tests.

See [COMPATIBILITY.md](COMPATIBILITY.md) for the current compatibility status.

## Architecture

MyLite is a layered MySQL compatibility system built on a bundled SQLite
engine. MyLite uses SQLite extension APIs wherever they provide the right
surface. When MySQL compatibility needs behavior SQLite cannot expose cleanly,
MyLite carries small targeted SQLite fork patches that expose extension points;
MyLite-specific behavior should stay in MyLite code rather than being embedded
inside the SQLite fork.

### Core library

- `libmylite` is the embedded runtime.
- SQLite is bundled into the library and used as the storage and execution
  foundation.
- MyLite adds the MySQL-facing parser, metadata model, built-in functions,
  compatibility rules, and `.mylite` file handling.
- SQLite integration prefers public extension APIs first, then targeted fork
  extension points when compatibility or performance requires them.

### SQL pipeline

1. **Parse:** MySQL-compatible syntax for implemented compatibility surfaces is
   represented as an AST.
2. **Analyze:** The AST is resolved against MySQL-compatible metadata, type
   rules, function definitions, warnings, errors, and statement semantics.
3. **Plan and translate:** Supported statements are lowered to SQLite execution
   primitives plus MyLite runtime hooks.
4. **Execute:** SQLite executes underlying operations, calling MyLite functions,
   wrappers, or fork-exposed extension points where SQLite behavior differs
   from MySQL.

MySQL-specific constructs without an embedded equivalent can still be accepted
and handled with explicit warnings or placeholder behavior.

### Metadata layer

- MySQL information schema tables are maintained internally.
- Metadata informs DDL, introspection, type handling, casting, and other
  MySQL-sensitive behavior.
- Operations such as `ALTER TABLE` depend on this metadata, but their execution
  still belongs to the broader SQL pipeline.

### File format

- User data lives in a single `.mylite` file.
- Each file begins with a MyLite header region that holds format metadata.
- SQLite reads its pages at an offset past this header, so the underlying
  database remains a normal SQLite database shifted forward in the file.
- This shifted-offset model may require a targeted SQLite extension point or
  fork patch if it cannot be handled cleanly through the public extension API.

### Integration packages

The repository is intended to hold `libmylite` and surrounding integration
packages:

- tooling
- MySQL wire protocol support
- PHP extension
- other application and runtime wiring

## SQLite extensions

Existing SQLite extensions may provide useful building blocks, especially math,
JSON, full-text search, regexp, and related functionality. Each extension still
needs explicit evaluation before becoming part of the default build:

- Does it match MySQL semantics closely enough?
- How much code size does it add?
- Does it simplify core implementation without compromising behavior?
- Is it better used as-is, wrapped, or reimplemented?
- Is its license compatible with this project?

## Development

TBD.

## References

MyLite is possible because of [SQLite](https://www.sqlite.org/), an exceptional
embedded database engine and one of the most carefully engineered software
projects in the world.

## License

TBD.

Until a project license is chosen, all MyLite work must be derived from official
MySQL documentation, observed MySQL runtime behavior, and other sources with
permissive licenses that keep MIT, BSD, and similar licensing options available.
Do not copy or adapt MySQL, MariaDB, Percona, GPL, or other restrictively
licensed projects.
