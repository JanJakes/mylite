# Compatibility Claim Evidence

Supported rows in `COMPATIBILITY.md` are release claims, not informal progress
notes. Their durable identifiers and evidence mappings live in `claims.tsv`.
The identifier remains attached to the behavior when wording or table order
changes; it is not regenerated from the current line number.

MySQL feature claims map to at least one independently authored specification,
one MySQL 8.4.9 expectation script, one registered native test, the native pull
request tier, and the complete nightly MySQL tier. Application claims instead
map to their harness specification, executable runner or test, and exact CI
job. One evidence artifact may support multiple narrow claims when it tests the
shared behavior explicitly.

Run the repository gate with:

```sh
tools/validate-compatibility-claims
```

The gate also verifies that the complete MySQL manifest is sorted, unique, and
contains every current expectation script. New green rows or expectation
scripts therefore require evidence updates in the same change.
