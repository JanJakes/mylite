#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/docker-php84-latest"
upstream_dir="$repo_root/build/wp-sqlite-integration-src"
results_path="$repo_root/build/wp-sqlite-mylite-results.json"

if [[ ! -d "$upstream_dir/.git" ]]; then
	rm -rf "$upstream_dir"
	git clone --depth=1 --branch=trunk https://github.com/WordPress/sqlite-database-integration.git "$upstream_dir"
else
	git -C "$upstream_dir" fetch --depth=1 origin trunk
	git -C "$upstream_dir" checkout --detach FETCH_HEAD >/dev/null
fi

docker run --rm -v "$repo_root":/work -w /work php:8.4-cli-bookworm sh -lc '
	set -eu
	apt-get update >/dev/null
	apt-get install -y --no-install-recommends cmake ninja-build build-essential pkg-config >/dev/null
	cmake -S . -B build/docker-php84-latest -G Ninja -DCMAKE_BUILD_TYPE=Release -DMYLITE_WARNINGS_AS_ERRORS=ON >/dev/null
	cmake --build build/docker-php84-latest >/dev/null
'

docker run --rm -v "$repo_root":/work -w /work php:8.4-cli-bookworm sh -lc '
	set -eu
	php \
		-d extension=/work/build/docker-php84-latest/packages/php-ext-mylite/mysqli.so \
		experiments/wp-sqlite-tests/runner.php \
		--tests-dir=/work/build/wp-sqlite-integration-src/packages/mysql-on-sqlite/tests \
		--results=/work/build/wp-sqlite-mylite-results.json
'

echo "Results written to $results_path"
