# php-ext-mylite

`php-ext-mylite` builds a PHP module named `mysqli` that routes standard PHP
mysqli APIs to embedded MyLite.

Build it with the monorepo CMake workflow:

```sh
cmake --preset dev
cmake --build --preset dev --target mylite_php_mysqli mylite_php_binary
```

Run it without PHP's stock mysqli module:

```sh
php -n -d extension=build/dev/packages/php-ext-mylite/mysqli.so script.php
```

The package also builds a `mylite` executable linked to the same bundled MyLite
runtime for smoke testing and packaging.
