# php-ext-mysqli-mylite

`php-ext-mysqli-mylite` builds a PHP module named `mysqli` that routes standard PHP
mysqli APIs to embedded MyLite.

Build it with the monorepo CMake workflow:

```sh
cmake --preset php-dev
cmake --build --preset php-dev --target mylite_php_mysqli_extension
```

Run it without PHP's stock mysqli module:

```sh
php -n \
  -d extension=build/php-dev/packages/php-ext-mylite/mylite.so \
  -d extension=build/php-dev/packages/php-ext-mysqli-mylite/mysqli.so \
  script.php
```
