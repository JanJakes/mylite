# Catalog Integrity

MyLite validates the complete catalog and its correspondence with SQLite's
physical schema whenever it opens an existing database file.

## Detection Contract

Open-time validation covers:

- the exact shape, constraints, and required indexes of MyLite catalog tables;
- the required catalog-seal invalidation triggers;
- catalog row ownership, ordering, generation, and cross-reference invariants;
- catalog table, column, index, generated-column, check-constraint, view, and
  foreign-key correspondence with the physical SQLite schema; and
- corruption that SQLite itself reports while reading or checking the
  structures touched by validation.

This contract does not promise a checksum of every user-data page, detection of
bit rot in an otherwise unread user value, or resistance to an attacker who can
rewrite both the database and the validation implementation. Broader payload
scrubbing remains the responsibility of SQLite integrity checking and the
embedding application's storage protections.

## Open Protocol

The catalog integrity seal records the catalog generation and SQLite schema
cookie that were current when MyLite last completed structural validation.
Catalog triggers invalidate the seal when catalog structure or durable
descriptors change.

The seal is a publication marker, not sufficient evidence for opening a file.
An existing current-format file always enters one writer transaction and runs
full structural validation before the catalog becomes visible to the new
handle. A matching seal avoids only redundant seal publication. This rule
prevents a structured catalog-row mutation from bypassing validation by
preserving or restoring the catalog generation and SQLite schema cookie.

If validation fails, opening returns `MYLITE_ERROR`, leaves the output handle
null, and reports error 1105 with SQLSTATE `HY000` and the deterministic message
`MyLite catalog is incompatible or corrupt`. Validation never reseals a failed
state.

## Mutation Protocol

Catalog and physical-schema mutations occur in one writer transaction. Before
commit, MyLite validates any previously unsealed state, applies the complete
logical and physical change, advances the catalog generation, validates
structural mutations, and publishes the new seal. A failure rolls back the
physical change, catalog rows, generation, and seal together.

The validation cost is linear in catalog metadata and physical object count.
MyLite accepts that existing-file open cost in exchange for deterministic
corruption detection and no additional hashing dependency.
