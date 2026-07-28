# Embedding, Packaging, and ABI

MyLite builds a static core library by default. A versioned shared library is
available for hosts that need a process-wide runtime. Both forms expose the
same public C API from `mylite/mylite.h`.

## Production builds

The production presets enable function/data sections and linker dead-code
elimination. Link-time optimization remains an explicit option because the
controlled GCC artifact comparison made the PHP modules larger.

```sh
cmake --preset production
cmake --build --preset production
cmake --install build/production --prefix /path/to/prefix --strip
```

Use `php-production` to include the PHP modules and `shared-abi` to build the
versioned shared library. `MYLITE_ENABLE_LTO=ON` is available for toolchain- and
host-specific measurement; it is not part of the default production profile.

The `mylite_size_report` target writes a reproducible section/object and sorted
symbol report beside the library. `mylite_size_reports` enforces reviewed byte
budgets for the static core, shared core, and each PHP module. A budget increase
is a release decision and must be justified by measured functionality rather
than adjusted automatically. Production packaging should strip installed
artifacts, not the build-tree artifacts that tests and ABI checks inspect.

The formal tag/manual release workflow calls every reusable quality tier,
rebuilds static, shared, and PHP packages twice in separate build trees, and
requires byte-identical compressed archives. Release evidence includes SHA-256
checksums, size reports, a source/build manifest, an SPDX 2.3 SBOM, local
SLSA-style provenance, and GitHub-signed provenance/SBOM attestations when the
repository supports them. Component license fields remain `NOASSERTION`; the
SBOM records inventory without selecting a project license.

The binary release workflow currently qualifies and publishes Linux x86-64
artifacts only. Linux, macOS, and Windows source builds remain CI-qualified, but
they are not represented as downloadable binary release packages. The release
job runs inside a digest-pinned PHP image and installs build dependencies from
a dated Debian snapshot so rebuilding the same commit does not silently select
new compiler, PHP, or system-package inputs.

## CMake consumers

The installed CMake package exports `MyLite::mylite` and `MyLite::headers`.
Static packages also export the private bundled SQLite target needed to carry
the complete link contract.

```cmake
find_package(MyLite 0.1 CONFIG REQUIRED)
target_link_libraries(application PRIVATE MyLite::mylite)
```

The installed `mylite.pc` provides the equivalent pkg-config contract. Static
consumers must request private dependencies:

```sh
cc application.c $(pkg-config --cflags --libs --static mylite)
```

The integration suite installs the current build into an empty prefix, builds
and runs a clean CMake consumer, and builds and runs a pkg-config consumer. It
does not use checkout-relative include or library paths.

## Ownership and threading

Database, statement, result, and returned-value ownership is defined beside the
public declarations in `mylite/mylite.h`. A database handle and statements
created from it are single-threaded and non-reentrant. Independent handles may
be used concurrently. Applications must finalize statements and free results
according to the documented lifetime boundaries.

Length-bearing hosts must use `mylite_open_with_size()` or
`mylite_open_with_size_and_diagnostic()`. The path span is borrowed for the
call, excludes any optional C terminator, and is copied only after validation.
Empty spans and spans containing NUL are rejected before SQLite, the offset
VFS, or the platform filesystem is reached. `mylite_open()` and
`mylite_open_with_diagnostic()` remain NUL-terminated convenience wrappers that
measure the input with `strlen()`; native memory databases use the separate
`mylite_open_memory*()` family.

## Shared-library ABI

Shared builds use the project major version as `SOVERSION`; the current ABI is
`libmylite.so.0`. Only declarations marked `MYLITE_API` are public. Windows
consumers outside the exported CMake target define
`MYLITE_USING_SHARED_LIBRARY` so the header imports those declarations; the
exported target propagates it automatically.

Linux shared builds expose `mylite_abi_check`. It compares dynamic exports with
`packages/libmylite/abi/libmylite-0.symbols` and compares the complete public
header with the reviewed ABI-0 header snapshot. This catches source-level
changes to declarations, types, constants, and macros even when exported symbol
names are unchanged. Adding, removing, or changing a public declaration
requires an intentional ABI review. An incompatible change requires a new
major/SOVERSION and new snapshots rather than editing the old contract silently.

## PHP modules

PHP builds install `mylite`, the mysqli replacement, and `pdo_mylite` under
`${CMAKE_INSTALL_LIBDIR}/mylite/php` by default. The directory is configurable
with `MYLITE_PHP_INSTALL_DIR`. Load the core `mylite` module before either
adapter, and build all modules from the same MyLite and PHP configuration.

The adapter targets consume the core public-header target; their build no
longer depends on source-tree or generated-header paths from the checkout.
