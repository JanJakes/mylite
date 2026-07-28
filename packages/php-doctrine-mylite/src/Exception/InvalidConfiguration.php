<?php

declare(strict_types=1);

namespace MyLite\Doctrine\DBAL\Exception;

use Doctrine\DBAL\Driver\AbstractException;

final class InvalidConfiguration extends AbstractException
{
    public static function value(string $message): self
    {
        return new self($message);
    }
}
