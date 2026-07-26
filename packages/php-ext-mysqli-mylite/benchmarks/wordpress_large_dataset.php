<?php

declare(strict_types=1);

const DEFAULT_POSTS = 100000;
const DEFAULT_META_PER_POST = 5;
const DEFAULT_SAMPLES = 3;
const DEFAULT_ITERATIONS = 10;
const INSERT_BATCH_SIZE = 100;
const TAXONOMY_COUNT = 1000;
const RESULT_LIMIT = 10000;

/**
 * @return array<string, int|string|bool>
 */
function parse_options(array $arguments): array
{
    $options = [
        'database' => '',
        'wordpress_dir' => '',
        'output' => '',
        'posts' => DEFAULT_POSTS,
        'meta_per_post' => DEFAULT_META_PER_POST,
        'samples' => DEFAULT_SAMPLES,
        'iterations' => DEFAULT_ITERATIONS,
        'reuse' => false,
        'seed_only' => false,
    ];

    for ($index = 1; $index < count($arguments); ++$index) {
        $argument = $arguments[$index];
        if ($argument === '--reuse') {
            $options['reuse'] = true;
            continue;
        }
        if ($argument === '--seed-only') {
            $options['seed_only'] = true;
            continue;
        }
        if (!str_starts_with($argument, '--') || $index + 1 >= count($arguments)) {
            throw new InvalidArgumentException("invalid option: {$argument}");
        }

        $name = str_replace('-', '_', substr($argument, 2));
        $value = $arguments[++$index];
        if (!array_key_exists($name, $options)) {
            throw new InvalidArgumentException("unknown option: {$argument}");
        }
        if (in_array($name, ['posts', 'meta_per_post', 'samples', 'iterations'], true)) {
            if (!ctype_digit($value) || (int) $value < 1) {
                throw new InvalidArgumentException("invalid value for {$argument}: {$value}");
            }
            $options[$name] = (int) $value;
        } else {
            $options[$name] = $value;
        }
    }

    if ($options['database'] === '' || $options['wordpress_dir'] === '') {
        throw new InvalidArgumentException('--database and --wordpress-dir are required');
    }

    return $options;
}

function open_wordpress_database(string $path, string $wordpressDir, bool $reuse): wpdb
{
    if (!$reuse) {
        foreach (['', '-journal', '-wal', '-shm'] as $suffix) {
            @unlink($path . $suffix);
        }
    }

    $server = new mysqli($path, '', '', '', 0, '');
    if ($server->connect_errno !== 0) {
        throw new RuntimeException('server connection failed: ' . $server->connect_error);
    }
    if (!$reuse && !$server->query('CREATE DATABASE wpbench')) {
        throw new RuntimeException('create database failed: ' . $server->error);
    }
    $server->close();

    if (!defined('WP_DEBUG')) {
        define('WP_DEBUG', false);
    }
    if (!defined('WP_DEBUG_DISPLAY')) {
        define('WP_DEBUG_DISPLAY', false);
    }
    if (!defined('WP_CONTENT_DIR')) {
        define('WP_CONTENT_DIR', sys_get_temp_dir());
    }
    if (!function_exists('apply_filters')) {
        function apply_filters(string $hookName, mixed $value, mixed ...$arguments): mixed
        {
            return $value;
        }
    }
    if (!function_exists('has_filter')) {
        function has_filter(string $hookName, callable|false $callback = false): bool
        {
            return false;
        }
    }
    if (!function_exists('add_filter')) {
        function add_filter(
            string $hookName,
            callable $callback,
            int $priority = 10,
            int $acceptedArguments = 1
        ): bool {
            return true;
        }
    }
    require_once rtrim($wordpressDir, '/') . '/src/wp-includes/class-wpdb.php';

    $database = new wpdb('', '', 'wpbench', 'localhost:' . $path);
    if (!$database->dbh instanceof mysqli) {
        throw new RuntimeException('wpdb connection failed');
    }
    $database->prefix = 'wp_';
    $database->base_prefix = 'wp_';
    $database->suppress_errors(true);

    return $database;
}

function execute_or_fail(wpdb $database, string $sql, string $context): void
{
    if ($database->query($sql) === false) {
        throw new RuntimeException($context . ': ' . $database->last_error);
    }
}

function create_schema(wpdb $database): void
{
    $queries = [
        'CREATE TABLE wp_posts (
            ID BIGINT NOT NULL,
            post_author BIGINT NOT NULL,
            post_date BIGINT NOT NULL,
            post_status VARCHAR(20) NOT NULL,
            post_type VARCHAR(20) NOT NULL,
            post_title VARCHAR(160) NOT NULL,
            post_name VARCHAR(160) NOT NULL,
            post_parent BIGINT NOT NULL,
            menu_order INT NOT NULL,
            PRIMARY KEY (ID),
            KEY type_status_date (post_type, post_status, post_date, ID),
            KEY post_author (post_author, ID),
            KEY post_parent (post_parent, ID),
            KEY post_name (post_name, ID)
        )',
        'CREATE TABLE wp_postmeta (
            meta_id BIGINT NOT NULL,
            post_id BIGINT NOT NULL,
            meta_key VARCHAR(64) NOT NULL,
            meta_value VARCHAR(128) NOT NULL,
            PRIMARY KEY (meta_id),
            KEY post_id (post_id, meta_id),
            KEY meta_key_value (meta_key, meta_value, post_id)
        )',
        'CREATE TABLE wp_term_relationships (
            object_id BIGINT NOT NULL,
            term_taxonomy_id BIGINT NOT NULL,
            term_order INT NOT NULL,
            PRIMARY KEY (object_id, term_taxonomy_id),
            KEY term_taxonomy_id (term_taxonomy_id, object_id)
        )',
    ];

    foreach ($queries as $index => $query) {
        execute_or_fail($database, $query, 'create schema statement ' . ($index + 1));
    }
}

function seed_database(wpdb $database, int $postCount, int $metaPerPost): void
{
    execute_or_fail($database, 'START TRANSACTION', 'begin seed');
    try {
        seed_posts($database, $postCount);
        seed_postmeta($database, $postCount, $metaPerPost);
        seed_term_relationships($database, $postCount);
        execute_or_fail($database, 'COMMIT', 'commit seed');
    } catch (Throwable $error) {
        $database->query('ROLLBACK');
        throw $error;
    }
}

function seed_posts(wpdb $database, int $postCount): void
{
    for ($first = 1; $first <= $postCount; $first += INSERT_BATCH_SIZE) {
        $values = [];
        $last = min($postCount, $first + INSERT_BATCH_SIZE - 1);
        for ($id = $first; $id <= $last; ++$id) {
            $status = $id % 10 < 8 ? 'publish' : 'draft';
            $type = $id % 20 === 0 ? 'page' : 'post';
            $values[] = sprintf(
                "(%d,%d,%d,'%s','%s','Post %010d','post-%010d',%d,%d)",
                $id,
                ($id % 1000) + 1,
                1700000000 + $id,
                $status,
                $type,
                $id,
                $id,
                $id > 100 ? $id - 100 : 0,
                $id % 20
            );
        }
        execute_or_fail(
            $database,
            'INSERT INTO wp_posts VALUES ' . implode(',', $values),
            "seed posts {$first}-{$last}"
        );
    }
}

function seed_postmeta(wpdb $database, int $postCount, int $metaPerPost): void
{
    $values = [];
    $metaId = 1;
    for ($postId = 1; $postId <= $postCount; ++$postId) {
        for ($slot = 0; $slot < $metaPerPost; ++$slot) {
            $values[] = sprintf(
                "(%d,%d,'_bench_key_%d','value-%010d-%d')",
                $metaId++,
                $postId,
                $slot,
                $postId,
                $slot
            );
            if (count($values) === INSERT_BATCH_SIZE) {
                execute_or_fail(
                    $database,
                    'INSERT INTO wp_postmeta VALUES ' . implode(',', $values),
                    'seed postmeta through ' . ($metaId - 1)
                );
                $values = [];
            }
        }
    }
    if ($values !== []) {
        execute_or_fail(
            $database,
            'INSERT INTO wp_postmeta VALUES ' . implode(',', $values),
            'seed final postmeta batch'
        );
    }
}

function seed_term_relationships(wpdb $database, int $postCount): void
{
    for ($first = 1; $first <= $postCount; $first += INSERT_BATCH_SIZE) {
        $values = [];
        $last = min($postCount, $first + INSERT_BATCH_SIZE - 1);
        for ($id = $first; $id <= $last; ++$id) {
            $values[] = sprintf('(%d,%d,%d)', $id, $id % TAXONOMY_COUNT, $id % 10);
        }
        execute_or_fail(
            $database,
            'INSERT INTO wp_term_relationships VALUES ' . implode(',', $values),
            "seed terms {$first}-{$last}"
        );
    }
}

function verify_database(wpdb $database, int $postCount, int $metaPerPost): void
{
    $expected = [
        'wp_posts' => $postCount,
        'wp_postmeta' => $postCount * $metaPerPost,
        'wp_term_relationships' => $postCount,
    ];
    foreach ($expected as $table => $count) {
        $actual = (int) $database->get_var("SELECT COUNT(*) FROM {$table}");
        if ($actual !== $count) {
            throw new RuntimeException("{$table}: expected {$count} rows, got {$actual}");
        }
    }
}

/**
 * @param list<array<int, mixed>> $rows
 * @return array{int, int, string}
 */
function consume_rows(array $rows): array
{
    $hash = hash_init('sha256');
    $bytes = 0;
    foreach ($rows as $row) {
        foreach ($row as $value) {
            $text = $value === null ? '<NULL>' : (string) $value;
            $bytes += strlen($text);
            hash_update($hash, pack('J', strlen($text)) . $text);
        }
    }

    return [count($rows), $bytes, hash_final($hash)];
}

/**
 * @param callable(int): array{int, int, string} $operation
 */
function run_scenario(
    $output,
    string $name,
    int $postCount,
    int $samples,
    int $iterations,
    callable $operation
): void {
    for ($sample = 1; $sample <= $samples; ++$sample) {
        $aggregateHash = hash_init('sha256');
        $resultRows = 0;
        $valueBytes = 0;
        $operation($sample);

        $started = hrtime(true);
        for ($iteration = 0; $iteration < $iterations; ++$iteration) {
            [$rows, $bytes, $checksum] = $operation($iteration);
            $resultRows += $rows;
            $valueBytes += $bytes;
            hash_update($aggregateHash, $checksum);
        }
        $elapsedNs = hrtime(true) - $started;
        $elapsedMs = $elapsedNs / 1_000_000;
        $averageUs = $elapsedNs / 1_000 / $iterations;
        $operationsPerSecond = $iterations * 1_000_000_000 / $elapsedNs;

        fputcsv(
            $output,
            [
                $name,
                $postCount,
                $sample,
                $iterations,
                $resultRows,
                $valueBytes,
                hash_final($aggregateHash),
                number_format($elapsedMs, 3, '.', ''),
                number_format($averageUs, 3, '.', ''),
                number_format($operationsPerSecond, 3, '.', ''),
            ],
            ',',
            '"',
            ''
        );
    }
}

/**
 * @return list<array<int, mixed>>
 */
function fetch_mysqli_rows(mysqli $connection, string $sql, int $mode): array
{
    $result = $connection->query($sql, $mode);
    if (!$result instanceof mysqli_result) {
        throw new RuntimeException('mysqli query failed: ' . $connection->error);
    }
    $rows = [];
    while (($row = $result->fetch_row()) !== null) {
        $rows[] = $row;
    }
    $result->free();

    return $rows;
}

function run_scenarios(wpdb $database, array $options, $output): void
{
    $postCount = (int) $options['posts'];
    $samples = (int) $options['samples'];
    $iterations = (int) $options['iterations'];
    $connection = $database->dbh;

    fputcsv(
        $output,
        [
            'scenario',
            'posts',
            'sample',
            'iterations',
            'result_rows',
            'value_bytes',
            'checksum',
            'total_ms',
            'avg_us',
            'ops_per_sec',
        ],
        ',',
        '"',
        ''
    );

    run_scenario(
        $output,
        'wpdb_prepare_point',
        $postCount,
        $samples,
        $iterations,
        static function (int $iteration) use ($database, $postCount): array {
            $id = (($iteration * 48271) % $postCount) + 1;
            $sql = $database->prepare(
                'SELECT ID, post_title, post_status FROM wp_posts WHERE ID = %d',
                $id
            );

            return consume_rows($database->get_results($sql, ARRAY_N));
        }
    );
    run_scenario(
        $output,
        'wpdb_pagination_found_rows',
        $postCount,
        $samples,
        $iterations,
        static function (int $iteration) use ($database): array {
            $offset = ($iteration % 10) * 20;
            $rows = $database->get_results(
                "SELECT SQL_CALC_FOUND_ROWS ID, post_title FROM wp_posts
                 WHERE post_type = 'post' AND post_status = 'publish'
                 ORDER BY post_date DESC LIMIT {$offset}, 20",
                ARRAY_N
            );
            $rows[] = [(string) $database->get_var('SELECT FOUND_ROWS()')];

            return consume_rows($rows);
        }
    );
    run_scenario(
        $output,
        'wpdb_postmeta_filter_join',
        $postCount,
        $samples,
        $iterations,
        static function (int $iteration) use ($database, $postCount): array {
            $postId = (($iteration * 48271) % $postCount) + 1;
            $value = sprintf('value-%010d-0', $postId);
            $sql = $database->prepare(
                "SELECT p.ID, p.post_title FROM wp_posts AS p
                 INNER JOIN wp_postmeta AS pm ON pm.post_id = p.ID
                 WHERE pm.meta_key = %s AND pm.meta_value = %s
                 ORDER BY p.ID LIMIT 20",
                '_bench_key_0',
                $value
            );

            return consume_rows($database->get_results($sql, ARRAY_N));
        }
    );
    run_scenario(
        $output,
        'wpdb_taxonomy_join',
        $postCount,
        $samples,
        $iterations,
        static function (int $iteration) use ($database): array {
            $taxonomyId = $iteration % TAXONOMY_COUNT;
            $sql = $database->prepare(
                "SELECT p.ID, p.post_title FROM wp_posts AS p
                 INNER JOIN wp_term_relationships AS tr ON tr.object_id = p.ID
                 WHERE tr.term_taxonomy_id = %d ORDER BY p.ID LIMIT 100",
                $taxonomyId
            );

            return consume_rows($database->get_results($sql, ARRAY_N));
        }
    );
    run_scenario(
        $output,
        'wpdb_result_10000',
        $postCount,
        $samples,
        max(1, intdiv($iterations, 5)),
        static fn(int $iteration): array => consume_rows(
            $database->get_results(
                'SELECT ID, post_title, post_status FROM wp_posts ORDER BY ID LIMIT ' . RESULT_LIMIT,
                ARRAY_N
            )
        )
    );
    run_scenario(
        $output,
        'mysqli_buffered_10000',
        $postCount,
        $samples,
        max(1, intdiv($iterations, 5)),
        static fn(int $iteration): array => consume_rows(
            fetch_mysqli_rows(
                $connection,
                'SELECT ID, post_title, post_status FROM wp_posts ORDER BY ID LIMIT ' . RESULT_LIMIT,
                MYSQLI_STORE_RESULT
            )
        )
    );
    run_scenario(
        $output,
        'mysqli_streaming_10000',
        $postCount,
        $samples,
        max(1, intdiv($iterations, 5)),
        static fn(int $iteration): array => consume_rows(
            fetch_mysqli_rows(
                $connection,
                'SELECT ID, post_title, post_status FROM wp_posts ORDER BY ID LIMIT ' . RESULT_LIMIT,
                MYSQLI_USE_RESULT
            )
        )
    );
}

try {
    $options = parse_options($argv);
    $database = open_wordpress_database(
        (string) $options['database'],
        (string) $options['wordpress_dir'],
        (bool) $options['reuse']
    );
    if (!(bool) $options['reuse']) {
        create_schema($database);
        seed_database($database, (int) $options['posts'], (int) $options['meta_per_post']);
    }
    verify_database($database, (int) $options['posts'], (int) $options['meta_per_post']);

    $output = $options['output'] === ''
        ? STDOUT
        : fopen((string) $options['output'], 'wb');
    if (!is_resource($output)) {
        throw new RuntimeException('failed to open benchmark output');
    }
    if (!(bool) $options['seed_only']) {
        run_scenarios($database, $options, $output);
    }
    if ($output !== STDOUT) {
        fclose($output);
    }
    $database->close();
} catch (Throwable $error) {
    fwrite(STDERR, 'wordpress-large-dataset: ' . $error->getMessage() . PHP_EOL);
    exit(1);
}
