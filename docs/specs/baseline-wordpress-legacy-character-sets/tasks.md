# Baseline WordPress Legacy Character Sets Tasks

- [x] Verify the focused MySQL 8.4.9 conversion and metadata behavior.
- [x] Write the independently authored feature specification.
- [x] Admit WordPress legacy charset and default collation catalog rows for
      descriptor, scalar, and session readback paths.
- [x] Canonicalize `utf8` charset and collation aliases to `utf8mb3`.
- [x] Generalize `SET NAMES` / `SET CHARACTER SET` validation for the admitted
      charset and collation names.
- [x] Preserve invalid single-byte legacy text through scalar and row-scalar
      `CONVERT(... USING charset)` paths.
- [x] Add focused Big5 character and byte truncation behavior for WordPress
      validation queries.
- [x] Add fast C regressions for metadata, session readback, nested conversion,
      and Big5 truncation behavior.
- [x] Update compatibility documentation for the exact limited surface.
- [x] Run focused C tests and the WordPress `Tests_DB_Charset` class.
- [ ] Run broader WordPress PHPUnit verification after committing this slice.
