# MySQL Parser

Build the prototype parser:

```sh
make
```

Parse one statement:

```sh
bin/mylite-parse "SELECT 1"
```

Run smoke tests:

```sh
make smoke
```

Run the WordPress MySQL query corpus:

```sh
make corpus
```

The corpus target downloads the CSV fixture into `build/corpus/`, which is
ignored by Git. The current corpus reader handles the fixture's MySQL-style
backslash-escaped double quotes and verifies 69,577 records.
