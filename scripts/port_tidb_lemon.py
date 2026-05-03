#!/usr/bin/env python3
"""Generate a Lemon grammar from TiDB's yacc parser source.

The first MyLite parser milestone is syntax acceptance. This generator keeps
TiDB's productions and precedence declarations, strips Go semantic actions, and
renames symbols to match Lemon's terminal/nonterminal conventions.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


CHAR_TOKEN_NAMES = {
    "(": "LPAREN",
    ")": "RPAREN",
    ",": "COMMA",
    ".": "DOT",
    ";": "SEMICOLON",
    "+": "PLUS",
    "-": "MINUS",
    "*": "STAR",
    "/": "SLASH",
    "%": "PERCENT",
    "<": "LT",
    ">": "GT",
    "=": "EQCHAR",
    "&": "AMP",
    "|": "PIPECHAR",
    "^": "CARET",
    "~": "TILDE",
    ":": "COLON",
    "!": "BANG",
    "[": "LBRACKET",
    "]": "RBRACKET",
    "{": "LBRACE",
    "}": "RBRACE",
}

MYSQL_OVERLAY_KEYWORDS = {
    "ACTIVE": "ACTIVE",
    "AT": "AT",
    "BEFORE": "BEFORE",
    "CHANGED": "CHANGED",
    "CHANNEL": "CHANNEL",
    "CODE": "CODE",
    "COMPLETION": "COMPLETION",
    "COMPONENT": "COMPONENT",
    "CONDITION": "CONDITION",
    "CONTAINS": "CONTAINS",
    "DATAFILE": "DATAFILE",
    "DEC": "DECIMAL_TYPE",
    "DETERMINISTIC": "DETERMINISTIC",
    "DUMPFILE": "DUMPFILE",
    "EACH": "EACH",
    "EMPTY": "EMPTY_KWD",
    "ENDS": "ENDS",
    "EVERY": "EVERY",
    "EXPORT": "EXPORT",
    "FAST": "FAST",
    "FOLLOWS": "FOLLOWS",
    "GEOMETRY": "GEOMETRY",
    "GEOMCOLLECTION": "GEOMETRYCOLLECTION",
    "GEOMETRYCOLLECTION": "GEOMETRYCOLLECTION",
    "DIAGNOSTICS": "DIAGNOSTICS",
    "GET": "GET",
    "GTIDS": "GTIDS",
    "INSTALL": "INSTALL",
    "INACTIVE": "INACTIVE",
    "JSON_TABLE": "JSON_TABLE",
    "JSON_VALUE": "JSON_VALUE",
    "LEAVES": "LEAVES",
    "LINESTRING": "LINESTRING",
    "LOOP": "LOOP",
    "MULTILINESTRING": "MULTILINESTRING",
    "MULTIPOINT": "MULTIPOINT",
    "MULTIPOLYGON": "MULTIPOLYGON",
    "MIGRATE": "MIGRATE",
    "MUTEX": "MUTEX",
    "NUMBER": "NUMBER",
    "OLD": "OLD",
    "ONE": "ONE",
    "ORDINALITY": "ORDINALITY",
    "OPTIMIZER_COSTS": "OPTIMIZER_COSTS",
    "OPTIONS": "OPTIONS",
    "PERSIST": "PERSIST",
    "PATH": "PATH",
    "PHASE": "PHASE",
    "POLYGON": "POLYGON",
    "PERSIST_ONLY": "PERSIST_ONLY",
    "PLUGIN": "PLUGIN",
    "PRECEDES": "PRECEDES",
    "PREV": "PREV",
    "RANDOM": "RANDOM",
    "SCHEMA": "SCHEMA",
    "SCHEMAS": "SCHEMAS",
    "SERVER": "SERVER",
    "MODIFIES": "MODIFIES",
    "READS": "READS",
    "RELAY": "RELAY",
    "RETURN": "RETURN",
    "RETURNING": "RETURNING",
    "RETURNS": "RETURNS",
    "RESIGNAL": "RESIGNAL",
    "RETAIN": "RETAIN",
    "SIGNAL": "SIGNAL",
    "SONAME": "SONAME",
    "SOUNDS": "SOUNDS",
    "SRID": "SRID",
    "STACKED": "STACKED",
    "STARTS": "STARTS",
    "SUSPEND": "SUSPEND",
    "TREE": "TREE",
    "UNINSTALL": "UNINSTALL",
    "UNDO": "UNDO",
    "UPGRADE": "UPGRADE",
    "USE_FRM": "USE_FRM",
    "USER_RESOURCES": "USER_RESOURCES",
    "WORK": "WORK",
    "WRAPPER": "WRAPPER",
    "XML": "XML",
    "XA": "XA",
    "XID": "XID",
    "ZONE": "ZONE",
}

SYMBOL_RE = re.compile(
    r"%prec|[A-Za-z_][A-Za-z0-9_]*|'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\""
)


@dataclass(frozen=True)
class PrecedenceLine:
    kind: str
    symbols: list[str]


@dataclass(frozen=True)
class Rule:
    lhs: str
    rhs: list[str]
    precedence: str | None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--grammar-output", required=True, type=Path)
    parser.add_argument("--tokens-output", required=True, type=Path)
    args = parser.parse_args()

    source = args.source.read_text(encoding="utf-8")
    declarations, grammar, *_ = source.split("%%")

    token_symbols, display_to_symbol, precedence = parse_declarations(declarations)
    raw_rules = parse_raw_rules(strip_comments(strip_actions(grammar)))
    nonterminals = {lhs for lhs, _ in raw_rules}

    used_terminals: set[str] = set()
    rules = [
        parse_rule(lhs, rhs_text, token_symbols, nonterminals, used_terminals)
        for lhs, rhs_text in raw_rules
    ]
    rules = [
        rule
        for rule in rules
        if not (rule.lhs == "Expression" and rule.rhs[:1] == ['"MATCH"'])
    ]

    grammar_text = render_lemon_grammar(
        precedence=precedence,
        rules=rules,
        token_symbols=token_symbols,
        display_to_symbol=display_to_symbol,
        nonterminals=nonterminals,
        used_terminals=used_terminals,
    )
    token_text = render_token_map(display_to_symbol, used_terminals)

    args.grammar_output.parent.mkdir(parents=True, exist_ok=True)
    args.tokens_output.parent.mkdir(parents=True, exist_ok=True)
    args.grammar_output.write_text(grammar_text, encoding="utf-8")
    args.tokens_output.write_text(token_text, encoding="utf-8")


def parse_declarations(text: str) -> tuple[set[str], dict[str, str], list[PrecedenceLine]]:
    token_symbols: set[str] = set()
    display_to_symbol: dict[str, str] = {}
    precedence: list[PrecedenceLine] = []
    current = None

    for line in text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue

        if stripped.startswith("%token"):
            current = "token"
            rest = re.sub(r"^%token(?:\s+<[^>]+>)?", "", stripped).strip()
            pieces = [rest] if rest else []
        elif stripped.startswith("%"):
            current = None
            if stripped.startswith(("%left", "%right", "%nonassoc", "%precedence")):
                parts = re.findall(r"'[^']*'|\b[A-Za-z_][A-Za-z0-9_]*\b", stripped)
                kind = stripped.split()[0]
                symbols = parts[1:]
                token_symbols.update(symbols)
                precedence.append(PrecedenceLine(kind, symbols))
            continue
        elif current == "token":
            pieces = [stripped]
        else:
            continue

        for piece in pieces:
            if not piece or piece.startswith(("/*", "*")):
                continue
            match = re.match(r'([A-Za-z_][A-Za-z0-9_]*)(?:\s+"((?:[^"\\]|\\.)*)")?', piece)
            if match is None:
                continue
            symbol = match.group(1)
            token_symbols.add(symbol)
            if match.group(2) is not None:
                display = bytes(match.group(2), "utf-8").decode("unicode_escape")
                display_to_symbol.setdefault(display, symbol)

    return token_symbols, display_to_symbol, precedence


def strip_actions(text: str) -> str:
    output: list[str] = []
    i = 0
    state = "normal"

    while i < len(text):
        char = text[i]
        if state == "normal":
            if char == "'":
                output.append(char)
                i += 1
                state = "single_quote"
                continue
            if char == '"':
                output.append(char)
                i += 1
                state = "double_quote"
                continue
            if char == "/" and i + 1 < len(text) and text[i + 1] == "*":
                output.extend("/*")
                i += 2
                state = "block_comment"
                continue
            if char == "/" and i + 1 < len(text) and text[i + 1] == "/":
                output.extend("//")
                i += 2
                state = "line_comment"
                continue
            if char == "{":
                i = skip_action_block(text, i + 1)
                output.append(" ")
                continue
            output.append(char)
            i += 1
        elif state == "single_quote":
            output.append(char)
            i += 1
            if char == "\\" and i < len(text):
                output.append(text[i])
                i += 1
            elif char == "'":
                state = "normal"
        elif state == "double_quote":
            output.append(char)
            i += 1
            if char == "\\" and i < len(text):
                output.append(text[i])
                i += 1
            elif char == '"':
                state = "normal"
        elif state == "block_comment":
            output.append(char)
            i += 1
            if char == "*" and i < len(text) and text[i] == "/":
                output.append("/")
                i += 1
                state = "normal"
        elif state == "line_comment":
            output.append(char)
            i += 1
            if char == "\n":
                state = "normal"

    return "".join(output)


def skip_action_block(text: str, i: int) -> int:
    depth = 1
    state = "normal"
    while i < len(text) and depth > 0:
        char = text[i]
        if state == "normal":
            if char == "'":
                state = "single_quote"
            elif char == '"':
                state = "double_quote"
            elif char == "`":
                state = "backtick"
            elif char == "/" and i + 1 < len(text) and text[i + 1] == "*":
                state = "block_comment"
                i += 1
            elif char == "/" and i + 1 < len(text) and text[i + 1] == "/":
                state = "line_comment"
                i += 1
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
        elif state == "single_quote":
            if char == "\\":
                i += 1
            elif char == "'":
                state = "normal"
        elif state == "double_quote":
            if char == "\\":
                i += 1
            elif char == '"':
                state = "normal"
        elif state == "backtick":
            if char == "`":
                state = "normal"
        elif state == "block_comment":
            if char == "*" and i + 1 < len(text) and text[i + 1] == "/":
                state = "normal"
                i += 1
        elif state == "line_comment":
            if char == "\n":
                state = "normal"
        i += 1
    return i


def strip_comments(text: str) -> str:
    output: list[str] = []
    i = 0
    state = "normal"

    while i < len(text):
        char = text[i]
        if state == "normal":
            if char == "'":
                output.append(char)
                i += 1
                state = "single_quote"
                continue
            if char == '"':
                output.append(char)
                i += 1
                state = "double_quote"
                continue
            if char == "/" and i + 1 < len(text) and text[i + 1] == "*":
                output.append(" ")
                i += 2
                state = "block_comment"
                continue
            if char == "/" and i + 1 < len(text) and text[i + 1] == "/":
                output.append(" ")
                i += 2
                state = "line_comment"
                continue
            output.append(char)
            i += 1
        elif state == "single_quote":
            output.append(char)
            i += 1
            if char == "\\" and i < len(text):
                output.append(text[i])
                i += 1
            elif char == "'":
                state = "normal"
        elif state == "double_quote":
            output.append(char)
            i += 1
            if char == "\\" and i < len(text):
                output.append(text[i])
                i += 1
            elif char == '"':
                state = "normal"
        elif state == "block_comment":
            if char == "*" and i + 1 < len(text) and text[i + 1] == "/":
                i += 2
                state = "normal"
            else:
                i += 1
        elif state == "line_comment":
            if char == "\n":
                output.append("\n")
                i += 1
                state = "normal"
            else:
                i += 1

    return "".join(output)


def parse_raw_rules(grammar: str) -> list[tuple[str, str]]:
    raw_rules: list[tuple[str, str]] = []
    lhs = None
    current = None
    lhs_re = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$")

    for line in grammar.splitlines():
        if not line.strip():
            continue
        match = lhs_re.match(line)
        if match is not None:
            if lhs is not None and current is not None:
                raw_rules.append((lhs, current.strip()))
            lhs = match.group(1)
            current = match.group(2).strip()
            continue

        stripped = line.strip()
        if stripped.startswith("|"):
            if lhs is not None and current is not None:
                raw_rules.append((lhs, current.strip()))
            current = stripped[1:].strip()
        elif lhs is not None and current is not None and line.startswith((" ", "\t")):
            current += " " + stripped

    if lhs is not None and current is not None:
        raw_rules.append((lhs, current.strip()))
    return raw_rules


def parse_rule(
    lhs: str,
    rhs_text: str,
    token_symbols: set[str],
    nonterminals: set[str],
    used_terminals: set[str],
) -> Rule:
    symbols = SYMBOL_RE.findall(rhs_text)
    rhs: list[str] = []
    precedence = None
    i = 0
    while i < len(symbols):
        if symbols[i] == "%prec":
            precedence = symbols[i + 1] if i + 1 < len(symbols) else None
            if precedence is not None:
                used_terminals.add(precedence)
            i += 2
        else:
            rhs.append(symbols[i])
            if symbols[i] in token_symbols:
                used_terminals.add(symbols[i])
            i += 1

    for symbol in rhs:
        if symbol.startswith(("'", '"')):
            continue
        if symbol not in token_symbols and symbol not in nonterminals:
            raise ValueError(f"unknown grammar symbol {symbol!r} in {lhs}")

    if lhs == "PredicateExpr" and rhs == ["BitExpr"] and precedence is None:
        precedence = "lowerThanEq"
        used_terminals.add(precedence)
    if lhs == "HavingClause" and rhs == [] and precedence is None:
        precedence = "lowerThanSelectOpt"
        used_terminals.add(precedence)
    if lhs == "OptBinary" and rhs == [] and precedence is None:
        precedence = "lowerThanNot"
        used_terminals.add(precedence)
    if (
        lhs == "OptBinary"
        and rhs in (['"BINARY"', "OptCharset"], ["CharsetKw", "CharsetName", "OptBinMod"])
        and precedence is None
    ):
        precedence = "lowerThanNot"
        used_terminals.add(precedence)
    if lhs == "CurdateSym" and rhs == ["builtinCurDate"] and precedence is None:
        precedence = "lowerThanParenthese"
        used_terminals.add(precedence)

    return Rule(lhs, rhs, precedence)


def render_lemon_grammar(
    *,
    precedence: list[PrecedenceLine],
    rules: list[Rule],
    token_symbols: set[str],
    display_to_symbol: dict[str, str],
    nonterminals: set[str],
    used_terminals: set[str],
) -> str:
    lines = [
        "%name MyliteTidbParse",
        "%token_prefix MYLITE_TOK_",
        "%token_type {MyliteAstNode *}",
        "%default_type {MyliteAstNode *}",
        "%extra_argument {MyliteParserState *state}",
        "%include {",
        "#include <stdlib.h>",
        "#include <string.h>",
        '#include "mylite/parser_internal.h"',
        "}",
        "%syntax_error { mylite_parser_state_syntax_error(state, yymajor); }",
        "%parse_accept { mylite_parser_state_accept(state); }",
        "%parse_failure { mylite_parser_state_failure(state); }",
        "%stack_overflow { mylite_parser_state_stack_overflow(state); }",
        "%stack_size 4096",
        "",
    ]

    for line in precedence:
        kind = "%nonassoc" if line.kind == "%precedence" else line.kind
        mapped = [
            map_terminal_symbol(symbol, display_to_symbol, used_terminals)
            for symbol in line.symbols
        ]
        if {"JOIN", "STRAIGHT_JOIN"}.issubset(mapped):
            kind = "%right"
        lines.append(f"{kind} {' '.join(mapped)}.")
    lines.append("%nonassoc FOR_KWD.")
    lines.append("%nonassoc INTO.")

    lines.append("")
    rule_id = 1
    lines.append(render_lemon_rule("input", [map_nonterminal("Start")], None, rule_id, True))
    rule_id += 1

    for rule in rules:
        mapped_rhs = [
            map_symbol(symbol, token_symbols, display_to_symbol, nonterminals, used_terminals)
            for symbol in rule.rhs
        ]
        precedence = None
        if rule.precedence is not None:
            precedence = map_terminal_symbol(rule.precedence, display_to_symbol, used_terminals)
        lines.append(render_lemon_rule(map_nonterminal(rule.lhs), mapped_rhs, precedence, rule_id))
        rule_id += 1

    for line in render_mysql_overlay_rules(used_terminals):
        if "::=" in line:
            lines.append(render_overlay_lemon_rule(line, rule_id))
            rule_id += 1
        else:
            lines.append(line)
    return "\n".join(lines) + "\n"


def render_lemon_rule(
    lhs: str, rhs: list[str], precedence: str | None, rule_id: int, root: bool = False
) -> str:
    rhs_text = " ".join(f"{symbol}(C{i})" for i, symbol in enumerate(rhs))
    rendered = f"{lhs}(A) ::= {rhs_text}."
    if precedence is not None:
        rendered += f" [{precedence}]"
    rendered += " " + render_reduce_action(lhs, len(rhs), rule_id, root)
    return rendered


def render_overlay_lemon_rule(line: str, rule_id: int) -> str:
    match = re.match(r"^(\w+)\s+::=\s*(.*?)\.\s*(?:\[(\w+)\])?$", line.strip())
    if match is None:
        raise ValueError(f"cannot render overlay rule {line!r}")
    lhs = match.group(1)
    rhs = match.group(2).split() if match.group(2) else []
    return render_lemon_rule(lhs, rhs, match.group(3), rule_id)


def render_reduce_action(lhs: str, child_count: int, rule_id: int, root: bool) -> str:
    if child_count == 0:
        action = (
            "{ A = mylite_parser_state_reduce(state, "
            f"{rule_id}u, \"{lhs}\", 0, NULL);"
        )
    else:
        children = ", ".join(f"C{i}" for i in range(child_count))
        action = (
            "{ MyliteAstNode *children[] = {"
            f"{children}"
            "}; A = mylite_parser_state_reduce(state, "
            f"{rule_id}u, \"{lhs}\", {child_count}, children);"
        )
    if root:
        action += " mylite_parser_state_root(state, A);"
    action += " }"
    return action


def render_token_map(display_to_symbol: dict[str, str], used_terminals: set[str]) -> str:
    entries: list[tuple[str, str]] = []
    builtin_entries: list[tuple[str, str]] = []
    compound_entries: list[tuple[str, str, str, str]] = []

    for display, symbol in sorted(display_to_symbol.items()):
        if symbol not in used_terminals:
            continue
        upper_display = display.upper()
        if re.fullmatch(r"[A-Z][A-Z0-9_]*", upper_display):
            entries.append((upper_display, terminal_name(symbol)))
        elif upper_display in {
            "AS OF",
            "TO TIMESTAMP",
            "TO TSO",
            "MEMBER OF",
            "OPTIONALLY ENCLOSED BY",
        }:
            words = upper_display.split()
            while len(words) < 3:
                words.append("")
            compound_entries.append((words[0], words[1], words[2], terminal_name(symbol)))

    for symbol in sorted(used_terminals):
        keyword = builtin_keyword(symbol)
        if keyword is not None:
            builtin_entries.append((keyword, terminal_name(symbol)))
    for keyword, token in MYSQL_OVERLAY_KEYWORDS.items():
        if token in used_terminals:
            entries.append((keyword, token))

    entries = sorted(set(entries))
    builtin_entries = sorted(set(builtin_entries))
    lines = [
        "typedef struct MyliteTidbKeyword {",
        "  const char *keyword;",
        "  int token;",
        "} MyliteTidbKeyword;",
        "",
        "typedef struct MyliteTidbCompoundKeyword {",
        "  const char *first;",
        "  const char *second;",
        "  const char *third;",
        "  int token;",
        "} MyliteTidbCompoundKeyword;",
        "",
        "static const MyliteTidbKeyword mylite_tidb_keywords[] = {",
    ]
    for keyword, token in entries:
        lines.append(f'  {{"{keyword}", MYLITE_TOK_{token}}},')
    lines.extend(
        [
            "};",
            "",
            "static const size_t mylite_tidb_keyword_count =",
            "    sizeof(mylite_tidb_keywords) / sizeof(mylite_tidb_keywords[0]);",
            "",
            "static const MyliteTidbKeyword mylite_tidb_builtin_keywords[] = {",
        ]
    )
    for keyword, token in builtin_entries:
        lines.append(f'  {{"{keyword}", MYLITE_TOK_{token}}},')
    lines.extend(
        [
            "};",
            "",
            "static const size_t mylite_tidb_builtin_keyword_count =",
            "    sizeof(mylite_tidb_builtin_keywords) /",
            "    sizeof(mylite_tidb_builtin_keywords[0]);",
            "",
            "static const MyliteTidbCompoundKeyword mylite_tidb_compound_keywords[] = {",
        ]
    )
    for first, second, third, token in compound_entries:
        lines.append(f'  {{"{first}", "{second}", "{third}", MYLITE_TOK_{token}}},')
    lines.extend(
        [
            "};",
            "",
            "static const size_t mylite_tidb_compound_keyword_count =",
            "    sizeof(mylite_tidb_compound_keywords) /",
            "    sizeof(mylite_tidb_compound_keywords[0]);",
            "",
        ]
    )
    return "\n".join(lines)


def render_mysql_overlay_rules(used_terminals: set[str]) -> list[str]:
    used_terminals.update({"DECIMAL_TYPE", "PERSIST", "PERSIST_ONLY"})
    used_terminals.update(
        {
            "CHANGED",
            "CHANNEL",
            "COMPLETION",
            "AT",
            "ACTIVE",
            "BEFORE",
            "CODE",
            "CONTAINS",
            "DATAFILE",
            "COMPONENT",
            "CONDITION",
            "DETERMINISTIC",
            "DUMPFILE",
            "EACH",
            "EMPTY_KWD",
            "ENDS",
            "EVENT_BODY",
            "EVERY",
            "EXPORT",
            "FAST",
            "FOLLOWS",
            "GEOMETRY",
            "GEOMETRYCOLLECTION",
            "DIAGNOSTICS",
            "GET",
            "GTIDS",
            "INSTALL",
            "INACTIVE",
            "JSON_TABLE",
            "JSON_VALUE",
            "LEAVES",
            "LINESTRING",
            "LOOP",
            "MIGRATE",
            "MULTILINESTRING",
            "MULTIPOINT",
            "MULTIPOLYGON",
            "MUTEX",
            "NUMBER",
            "OLD",
            "ONE",
            "OPTIMIZER_COSTS",
            "OPTIONS",
            "ORDINALITY",
            "PATH",
            "PHASE",
            "POLYGON",
            "PLUGIN",
            "PRECEDES",
            "PREV",
            "RANDOM",
            "MODIFIES",
            "READS",
            "RELAY",
            "RETURN",
            "RETURNING",
            "RETURNS",
            "RESIGNAL",
            "RETAIN",
            "SCHEMA",
            "SCHEMAS",
            "SERVER",
            "SIGNAL",
            "SONAME",
            "SOUNDS",
            "SRID",
            "STACKED",
            "STARTS",
            "SUSPEND",
            "TREE",
            "UNINSTALL",
            "UNDO",
            "UPGRADE",
            "USE_FRM",
            "USER_RESOURCES",
            "WORK",
            "WRAPPER",
            "XML",
            "XA",
            "XID",
            "ZONE",
        }
    )
    return [
        "",
        "/* MyLite MySQL overlay rules absent from TiDB's server grammar. */",
        "nt_un_reserved_keyword ::= ACTIVE.",
        "nt_un_reserved_keyword ::= AT.",
        "nt_un_reserved_keyword ::= BEFORE.",
        "nt_un_reserved_keyword ::= CHANGED.",
        "nt_un_reserved_keyword ::= CHANNEL.",
        "nt_un_reserved_keyword ::= CODE.",
        "nt_un_reserved_keyword ::= COMPLETION.",
        "nt_un_reserved_keyword ::= COMPONENT.",
        "nt_un_reserved_keyword ::= CONDITION.",
        "nt_un_reserved_keyword ::= CONTAINS.",
        "nt_un_reserved_keyword ::= DATAFILE.",
        "nt_un_reserved_keyword ::= DETERMINISTIC.",
        "nt_un_reserved_keyword ::= DIAGNOSTICS.",
        "nt_un_reserved_keyword ::= DUMPFILE.",
        "nt_un_reserved_keyword ::= EACH.",
        "nt_un_reserved_keyword ::= EMPTY_KWD.",
        "nt_un_reserved_keyword ::= ENDS.",
        "nt_un_reserved_keyword ::= EVERY.",
        "nt_un_reserved_keyword ::= EXPORT.",
        "nt_un_reserved_keyword ::= FAST.",
        "nt_un_reserved_keyword ::= FOLLOWS.",
        "nt_un_reserved_keyword ::= GEOMETRY.",
        "nt_un_reserved_keyword ::= GEOMETRYCOLLECTION.",
        "nt_un_reserved_keyword ::= GET.",
        "nt_un_reserved_keyword ::= GTIDS.",
        "nt_un_reserved_keyword ::= INSTALL.",
        "nt_un_reserved_keyword ::= INACTIVE.",
        "nt_un_reserved_keyword ::= JSON_TABLE.",
        "nt_un_reserved_keyword ::= JSON_VALUE.",
        "nt_un_reserved_keyword ::= LEAVES.",
        "nt_un_reserved_keyword ::= LINESTRING.",
        "nt_un_reserved_keyword ::= MIGRATE.",
        "nt_un_reserved_keyword ::= MULTILINESTRING.",
        "nt_un_reserved_keyword ::= MULTIPOINT.",
        "nt_un_reserved_keyword ::= MULTIPOLYGON.",
        "nt_un_reserved_keyword ::= MUTEX.",
        "nt_un_reserved_keyword ::= NUMBER.",
        "nt_un_reserved_keyword ::= OLD.",
        "nt_un_reserved_keyword ::= ONE.",
        "nt_un_reserved_keyword ::= OPTIMIZER_COSTS.",
        "nt_un_reserved_keyword ::= OPTIONS.",
        "nt_un_reserved_keyword ::= ORDINALITY.",
        "nt_un_reserved_keyword ::= PATH.",
        "nt_un_reserved_keyword ::= PERSIST.",
        "nt_un_reserved_keyword ::= PERSIST_ONLY.",
        "nt_un_reserved_keyword ::= PHASE.",
        "nt_un_reserved_keyword ::= PLUGIN.",
        "nt_un_reserved_keyword ::= POLYGON.",
        "nt_un_reserved_keyword ::= PRECEDES.",
        "nt_un_reserved_keyword ::= PREV.",
        "nt_un_reserved_keyword ::= RANDOM.",
        "nt_un_reserved_keyword ::= MODIFIES.",
        "nt_un_reserved_keyword ::= READS.",
        "nt_un_reserved_keyword ::= RELAY.",
        "nt_un_reserved_keyword ::= RETURN.",
        "nt_un_reserved_keyword ::= RETURNING.",
        "nt_un_reserved_keyword ::= RETURNS.",
        "nt_un_reserved_keyword ::= RESIGNAL.",
        "nt_un_reserved_keyword ::= RETAIN.",
        "nt_un_reserved_keyword ::= SCHEMA.",
        "nt_un_reserved_keyword ::= SCHEMAS.",
        "nt_un_reserved_keyword ::= SERVER.",
        "nt_un_reserved_keyword ::= SIGNAL.",
        "nt_un_reserved_keyword ::= SONAME.",
        "nt_un_reserved_keyword ::= SRID.",
        "nt_un_reserved_keyword ::= STACKED.",
        "nt_un_reserved_keyword ::= STARTS.",
        "nt_un_reserved_keyword ::= SUSPEND.",
        "nt_un_reserved_keyword ::= TREE.",
        "nt_un_reserved_keyword ::= UNINSTALL.",
        "nt_un_reserved_keyword ::= UNDO.",
        "nt_un_reserved_keyword ::= UPGRADE.",
        "nt_un_reserved_keyword ::= USE_FRM.",
        "nt_un_reserved_keyword ::= USER_RESOURCES.",
        "nt_un_reserved_keyword ::= WORK.",
        "nt_un_reserved_keyword ::= WRAPPER.",
        "nt_un_reserved_keyword ::= XML.",
        "nt_un_reserved_keyword ::= XA.",
        "nt_un_reserved_keyword ::= XID.",
        "nt_un_reserved_keyword ::= ZONE.",
        "nt_statement ::= nt_mysql_check_table_stmt.",
        "nt_statement ::= nt_mysql_create_trigger_stmt.",
        "nt_statement ::= nt_mysql_drop_trigger_stmt.",
        "nt_statement ::= nt_mysql_create_event_stmt.",
        "nt_statement ::= nt_mysql_alter_event_stmt.",
        "nt_statement ::= nt_mysql_drop_event_stmt.",
        "nt_statement ::= nt_mysql_handler_stmt.",
        "nt_statement ::= nt_mysql_cache_index_stmt.",
        "nt_statement ::= nt_mysql_load_index_stmt.",
        "nt_statement ::= nt_mysql_import_table_stmt.",
        "nt_statement ::= nt_mysql_load_xml_stmt.",
        "nt_statement ::= nt_mysql_plugin_stmt.",
        "nt_statement ::= nt_mysql_repair_table_stmt.",
        "nt_statement ::= nt_mysql_checksum_table_stmt.",
        "nt_statement ::= nt_mysql_change_replication_stmt.",
        "nt_statement ::= nt_mysql_replication_control_stmt.",
        "nt_statement ::= nt_mysql_create_function_stmt.",
        "nt_statement ::= nt_mysql_drop_function_stmt.",
        "nt_statement ::= nt_mysql_alter_routine_stmt.",
        "nt_statement ::= nt_mysql_alter_view_stmt.",
        "nt_statement ::= nt_mysql_get_diagnostics_stmt.",
        "nt_statement ::= nt_mysql_purge_logs_stmt.",
        "nt_statement ::= nt_mysql_reset_stmt.",
        "nt_statement ::= nt_mysql_tablespace_stmt.",
        "nt_statement ::= nt_mysql_server_stmt.",
        "nt_statement ::= nt_mysql_revoke_proxy_stmt.",
        "nt_statement ::= nt_mysql_lock_instance_stmt.",
        "nt_statement ::= nt_mysql_xa_stmt.",
        "nt_procedure_statement_stmt ::= nt_alter_database_stmt.",
        "nt_procedure_statement_stmt ::= nt_alter_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_begin_transaction_stmt.",
        "nt_procedure_statement_stmt ::= nt_create_database_stmt.",
        "nt_procedure_statement_stmt ::= nt_create_index_stmt.",
        "nt_procedure_statement_stmt ::= nt_create_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_create_trigger_stmt.",
        "nt_procedure_statement_stmt ::= nt_create_user_stmt.",
        "nt_procedure_statement_stmt ::= nt_create_view_stmt.",
        "nt_procedure_statement_stmt ::= nt_do_stmt.",
        "nt_procedure_statement_stmt ::= nt_drop_database_stmt.",
        "nt_procedure_statement_stmt ::= nt_drop_index_stmt.",
        "nt_procedure_statement_stmt ::= nt_drop_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_drop_trigger_stmt.",
        "nt_procedure_statement_stmt ::= nt_drop_user_stmt.",
        "nt_procedure_statement_stmt ::= nt_drop_view_stmt.",
        "nt_procedure_statement_stmt ::= nt_alter_user_stmt.",
        "nt_procedure_statement_stmt ::= nt_flush_stmt.",
        "nt_procedure_statement_stmt ::= nt_grant_stmt.",
        "nt_procedure_statement_stmt ::= nt_grant_proxy_stmt.",
        "nt_procedure_statement_stmt ::= nt_grant_role_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_cache_index_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_load_index_stmt.",
        "nt_procedure_statement_stmt ::= nt_load_data_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_load_xml_stmt.",
        "nt_procedure_statement_stmt ::= nt_lock_tables_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_lock_instance_stmt.",
        "nt_procedure_statement_stmt ::= nt_optimize_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_prepared_stmt.",
        "nt_procedure_statement_stmt ::= nt_execute_stmt.",
        "nt_procedure_statement_stmt ::= nt_deallocate_stmt.",
        "nt_procedure_statement_stmt ::= nt_rename_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_savepoint_stmt.",
        "nt_procedure_statement_stmt ::= nt_release_savepoint_stmt.",
        "nt_procedure_statement_stmt ::= nt_revoke_stmt.",
        "nt_procedure_statement_stmt ::= nt_revoke_role_stmt.",
        "nt_procedure_statement_stmt ::= nt_set_default_role_stmt.",
        "nt_procedure_statement_stmt ::= nt_set_role_stmt.",
        "nt_procedure_statement_stmt ::= nt_show_stmt.",
        "nt_procedure_statement_stmt ::= nt_unlock_tables_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_check_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_repair_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_checksum_table_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_create_event_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_alter_event_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_drop_event_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_plugin_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_purge_logs_stmt.",
        "nt_procedure_statement_stmt ::= nt_call_stmt.",
        "nt_procedure_statement_stmt ::= nt_kill_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_reset_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_replication_control_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_get_diagnostics_stmt.",
        "nt_procedure_statement_stmt ::= nt_mysql_tablespace_stmt.",
        "nt_procedure_proc_stmt ::= RETURN nt_expression.",
        "nt_procedure_proc_stmt ::= SIGNAL nt_mysql_signal_condition nt_mysql_signal_set_opt.",
        "nt_procedure_proc_stmt ::= RESIGNAL nt_mysql_signal_condition_opt nt_mysql_signal_set_opt.",
        "nt_procedure_unlabel_loop_stmt ::= LOOP nt_procedure_proc_stmts END LOOP.",
        "nt_mysql_signal_condition_opt ::= .",
        "nt_mysql_signal_condition_opt ::= nt_mysql_signal_condition.",
        "nt_mysql_signal_condition ::= SQLSTATE nt_mysql_sqlstate_value_opt STRING_LIT.",
        "nt_mysql_signal_condition ::= nt_identifier.",
        "nt_mysql_sqlstate_value_opt ::= .",
        "nt_mysql_sqlstate_value_opt ::= VALUE.",
        "nt_mysql_signal_set_opt ::= .",
        "nt_mysql_signal_set_opt ::= SET nt_mysql_signal_item_list.",
        "nt_mysql_signal_item_list ::= nt_mysql_signal_item.",
        "nt_mysql_signal_item_list ::= nt_mysql_signal_item_list COMMA nt_mysql_signal_item.",
        "nt_mysql_signal_item ::= nt_identifier EQ nt_expr_or_default.",
        "nt_procedure_decl ::= DECLARE nt_identifier CONDITION FOR_KWD nt_mysql_condition_value.",
        "nt_procedure_hcond ::= nt_identifier.",
        "nt_mysql_condition_value ::= nt_num.",
        "nt_mysql_condition_value ::= SQLSTATE nt_opt_value STRING_LIT.",
        "nt_mysql_get_diagnostics_stmt ::= GET nt_mysql_current_opt DIAGNOSTICS nt_mysql_diagnostics_area.",
        "nt_mysql_current_opt ::= .",
        "nt_mysql_current_opt ::= CURRENT.",
        "nt_mysql_current_opt ::= STACKED.",
        "nt_mysql_diagnostics_area ::= nt_mysql_diagnostics_assignment_list.",
        "nt_mysql_diagnostics_area ::= CONDITION nt_expr_or_default nt_mysql_diagnostics_assignment_list.",
        "nt_mysql_diagnostics_assignment_list ::= nt_mysql_diagnostics_assignment.",
        "nt_mysql_diagnostics_assignment_list ::= nt_mysql_diagnostics_assignment_list COMMA nt_mysql_diagnostics_assignment.",
        "nt_mysql_diagnostics_assignment ::= nt_mysql_diagnostics_target EQ nt_mysql_diagnostics_item.",
        "nt_mysql_diagnostics_target ::= nt_user_variable.",
        "nt_mysql_diagnostics_target ::= nt_identifier.",
        "nt_mysql_diagnostics_item ::= nt_identifier.",
        "nt_username ::= BUILTIN_USER nt_optional_braces.",
        "nt_role_name_string ::= LOCKED.",
        "nt_role_name_string ::= NOWAIT.",
        "nt_role_name_string ::= ROLE.",
        "nt_role_name_string ::= SKIP.",
        "nt_mysql_create_function_stmt ::= CREATE nt_or_replace nt_view_algorithm nt_view_definer FUNCTION nt_if_not_exists nt_table_name LPAREN nt_opt_sp_pdparams RPAREN RETURNS nt_type nt_mysql_routine_characteristic_list_opt nt_mysql_function_body.",
        "nt_show_stmt ::= SHOW CREATE FUNCTION nt_table_name.",
        "nt_show_stmt ::= SHOW CREATE SCHEMA nt_if_not_exists nt_db_name.",
        "nt_show_stmt ::= SHOW FUNCTION CODE nt_table_name.",
        "nt_show_stmt ::= SHOW nt_replica HOSTS.",
        "nt_show_stmt ::= SHOW BINLOG EVENTS.",
        "nt_show_stmt ::= SHOW MASTER LOGS.",
        "nt_show_stmt ::= SHOW BINARY_TYPE LOGS.",
        "nt_show_stmt ::= SHOW WARNINGS nt_select_stmt_limit.",
        "nt_show_stmt ::= SHOW IDENT_SQL_ERRORS nt_select_stmt_limit.",
        "nt_show_stmt ::= SHOW ENGINE nt_identifier LOGS.",
        "nt_show_stmt ::= SHOW ENGINE nt_identifier MUTEX.",
        "nt_show_stmt ::= SHOW ENGINE nt_identifier STATUS.",
        "nt_show_stmt ::= SHOW PROCEDURE CODE nt_table_name.",
        "nt_mysql_function_body ::= RETURN nt_expression.",
        "nt_mysql_function_body ::= nt_procedure_unlabeled_block.",
        "nt_mysql_drop_function_stmt ::= DROP FUNCTION nt_if_exists nt_table_name.",
        "nt_mysql_alter_routine_stmt ::= ALTER FUNCTION nt_table_name.",
        "nt_mysql_alter_routine_stmt ::= ALTER FUNCTION nt_table_name nt_mysql_routine_characteristic_list.",
        "nt_mysql_alter_routine_stmt ::= ALTER PROCEDURE nt_table_name.",
        "nt_mysql_alter_routine_stmt ::= ALTER PROCEDURE nt_table_name nt_mysql_routine_characteristic_list.",
        "nt_create_procedure_stmt ::= CREATE PROCEDURE nt_if_not_exists nt_table_name LPAREN nt_opt_sp_pdparams RPAREN SQL SECURITY DEFINER nt_procedure_proc_stmt.",
        "nt_create_procedure_stmt ::= CREATE PROCEDURE nt_if_not_exists nt_table_name LPAREN nt_opt_sp_pdparams RPAREN SQL SECURITY INVOKER nt_procedure_proc_stmt.",
        "nt_mysql_routine_characteristic_list_opt ::= .",
        "nt_mysql_routine_characteristic_list_opt ::= nt_mysql_routine_characteristic_list.",
        "nt_mysql_routine_characteristic_list ::= nt_mysql_routine_characteristic.",
        "nt_mysql_routine_characteristic_list ::= nt_mysql_routine_characteristic_list nt_mysql_routine_characteristic.",
        "nt_mysql_routine_characteristic ::= DETERMINISTIC.",
        "nt_mysql_routine_characteristic ::= NOT DETERMINISTIC.",
        "nt_mysql_routine_characteristic ::= LANGUAGE SQL.",
        "nt_mysql_routine_characteristic ::= SQL SECURITY DEFINER.",
        "nt_mysql_routine_characteristic ::= SQL SECURITY INVOKER.",
        "nt_mysql_routine_characteristic ::= COMMENT STRING_LIT.",
        "nt_mysql_routine_characteristic ::= CONTAINS SQL.",
        "nt_mysql_routine_characteristic ::= NO SQL.",
        "nt_mysql_routine_characteristic ::= READS SQL DATA.",
        "nt_mysql_routine_characteristic ::= MODIFIES SQL DATA.",
        "nt_create_procedure_stmt ::= CREATE PROCEDURE nt_if_not_exists nt_table_name LPAREN nt_opt_sp_pdparams RPAREN LANGUAGE SQL nt_procedure_proc_stmt.",
        "nt_mysql_alter_view_stmt ::= ALTER nt_view_algorithm nt_view_definer nt_view_sql_security VIEW nt_view_name nt_view_field_list AS nt_create_view_select_opt nt_view_check_option.",
        "nt_set_stmt ::= SET PERSIST nt_variable_assignment_list.",
        "nt_set_stmt ::= SET PERSIST_ONLY nt_variable_assignment_list.",
        "nt_set_stmt ::= SET LOCAL TRANSACTION nt_transaction_chars.",
        "nt_set_stmt ::= SET PASSWORD nt_eq_or_assignment_eq nt_password_opt REPLACE nt_password_opt.",
        "nt_set_stmt ::= SET PASSWORD FOR_KWD nt_username nt_eq_or_assignment_eq nt_password_opt REPLACE nt_password_opt.",
        "nt_set_stmt ::= SET PASSWORD TO RANDOM.",
        "nt_set_stmt ::= SET PASSWORD FOR_KWD nt_username TO RANDOM.",
        "nt_auth_option ::= IDENTIFIED BY RANDOM PASSWORD.",
        "nt_auth_option ::= IDENTIFIED BY nt_auth_string REPLACE nt_auth_string.",
        "nt_password_or_lock_option ::= DISCARD OLD PASSWORD.",
        "nt_password_or_lock_option ::= PASSWORD REQUIRE CURRENT.",
        "nt_password_or_lock_option ::= PASSWORD REQUIRE CURRENT OPTIONAL.",
        "nt_password_or_lock_option ::= RETAIN CURRENT PASSWORD.",
        "nt_set_expr ::= ALL.",
        "nt_set_expr ::= ROW.",
        "nt_grant_role_stmt ::= GRANT nt_role_or_priv_elem_list TO nt_username_list WITH ADMIN OPTION.",
        "nt_revoke_stmt ::= REVOKE IF_KWD EXISTS nt_role_or_priv_elem_list ON nt_object_type nt_priv_level FROM nt_user_spec_list.",
        "nt_revoke_stmt ::= REVOKE nt_role_or_priv_elem_list ON nt_object_type nt_priv_level FROM nt_user_spec_list IGNORE UNKNOWN USER.",
        "nt_revoke_stmt ::= REVOKE IF_KWD EXISTS nt_role_or_priv_elem_list ON nt_object_type nt_priv_level FROM nt_user_spec_list IGNORE UNKNOWN USER.",
        "nt_revoke_role_stmt ::= REVOKE IF_KWD EXISTS nt_role_or_priv_elem_list FROM nt_username_list.",
        "nt_revoke_role_stmt ::= REVOKE nt_role_or_priv_elem_list FROM nt_username_list IGNORE UNKNOWN USER.",
        "nt_revoke_role_stmt ::= REVOKE IF_KWD EXISTS nt_role_or_priv_elem_list FROM nt_username_list IGNORE UNKNOWN USER.",
        "nt_drop_resource_group_stmt ::= DROP RESOURCE GROUP nt_if_exists nt_resource_group_name FORCE.",
        "nt_like_or_ilike_escape_opt ::= ESCAPE UNDERSCORE_CS STRING_LIT.",
        "nt_like_or_ilike_escape_opt ::= ESCAPE UNDERSCORE_CS HEX_LIT.",
        "nt_like_or_ilike_escape_opt ::= ESCAPE UNDERSCORE_CS BIT_LIT.",
        "nt_like_or_ilike_escape_opt ::= ESCAPE nt_function_call_generic.",
        "nt_like_or_ilike_escape_opt ::= ESCAPE nt_sub_select.",
        "nt_like_or_ilike_escape_opt ::= ESCAPE INT_LIT.",
        "nt_like_or_ilike_escape_opt ::= ESCAPE STRING_LIT PIPES STRING_LIT.",
        "nt_show_target_filterable ::= SCHEMAS.",
        "nt_show_target_filterable ::= STORAGE ENGINES.",
        "nt_show_target_filterable ::= EXTENDED nt_show_index_kwd nt_from_or_in nt_table_name.",
        "nt_show_target_filterable ::= EXTENDED nt_show_index_kwd nt_from_or_in nt_identifier nt_from_or_in nt_identifier.",
        "nt_show_target_filterable ::= LOCAL VARIABLES.",
        "nt_show_target_filterable ::= LOCAL STATUS.",
        "nt_predicate_expr ::= nt_bit_expr SOUNDS LIKE nt_simple_expr.",
        "nt_simple_expr ::= MATCH LPAREN nt_column_name_list RPAREN AGAINST LPAREN nt_bit_expr nt_fulltext_search_modifier_opt RPAREN.",
        "nt_simple_expr ::= MATCH nt_column_name_list AGAINST LPAREN nt_bit_expr nt_fulltext_search_modifier_opt RPAREN.",
        "nt_function_call_non_keyword ::= JSON_VALUE LPAREN nt_expression COMMA STRING_LIT nt_mysql_json_value_clause_list_opt RPAREN.",
        "nt_mysql_json_value_clause_list_opt ::= .",
        "nt_mysql_json_value_clause_list_opt ::= nt_mysql_json_value_clause_list.",
        "nt_mysql_json_value_clause_list ::= nt_mysql_json_value_clause.",
        "nt_mysql_json_value_clause_list ::= nt_mysql_json_value_clause_list nt_mysql_json_value_clause.",
        "nt_mysql_json_value_clause ::= RETURNING nt_cast_type.",
        "nt_mysql_json_value_clause ::= NULL ON EMPTY_KWD.",
        "nt_mysql_json_value_clause ::= ERROR_KWD ON EMPTY_KWD.",
        "nt_mysql_json_value_clause ::= DEFAULT_KWD nt_expression ON EMPTY_KWD.",
        "nt_mysql_json_value_clause ::= NULL ON ERROR_KWD.",
        "nt_mysql_json_value_clause ::= ERROR_KWD ON ERROR_KWD.",
        "nt_mysql_json_value_clause ::= DEFAULT_KWD nt_expression ON ERROR_KWD.",
        "nt_function_call_generic ::= IDENTIFIER LPAREN nt_mysql_function_attr_arg_list RPAREN.",
        "nt_function_call_generic ::= nt_identifier DOT nt_identifier LPAREN nt_mysql_function_attr_arg_list RPAREN.",
        "nt_mysql_function_attr_arg_list ::= nt_mysql_function_attr_arg.",
        "nt_mysql_function_attr_arg_list ::= nt_mysql_function_attr_arg_list COMMA nt_mysql_function_attr_arg.",
        "nt_mysql_function_attr_arg ::= nt_expression AS nt_identifier.",
        "nt_mysql_function_attr_arg ::= nt_expression nt_identifier.",
        "nt_function_call_generic ::= SEQUENCE LPAREN nt_expression_list_opt RPAREN.",
        "nt_function_call_non_keyword ::= WEIGHT_STRING LPAREN nt_expression COMMA nt_expression_list RPAREN.",
        "nt_sum_expr ::= STD LPAREN nt_buggy_default_false_distinct_opt nt_expression RPAREN nt_opt_windowing_clause.",
        "nt_sum_expr ::= STDDEV LPAREN nt_buggy_default_false_distinct_opt nt_expression RPAREN nt_opt_windowing_clause.",
        "nt_sum_expr ::= VARIANCE LPAREN nt_buggy_default_false_distinct_opt nt_expression RPAREN nt_opt_windowing_clause.",
        "nt_opt_lead_lag_info ::= COMMA nt_identifier nt_opt_ll_default.",
        "nt_opt_lead_lag_info ::= COMMA nt_user_variable nt_opt_ll_default.",
        "nt_log_type_opt ::= RELAY.",
        "nt_flush_option ::= OPTIMIZER_COSTS.",
        "nt_flush_option ::= USER_RESOURCES.",
        "nt_flush_option ::= nt_table_or_tables nt_table_name_list FOR_KWD EXPORT.",
        "nt_select_stmt_from_table ::= SELECT_KWD nt_select_stmt_opts nt_select_stmt_field_list INTO nt_mysql_select_into_target_list FROM nt_table_refs_clause nt_where_clause_optional nt_select_stmt_group nt_having_clause nt_window_clause_optional.",
        "nt_select_stmt_from_table ::= SELECT_KWD nt_select_stmt_opts nt_select_stmt_field_list INTO nt_mysql_select_file_target FROM nt_table_refs_clause nt_where_clause_optional nt_select_stmt_group nt_having_clause nt_window_clause_optional.",
        "nt_select_stmt_from_dual_table ::= SELECT_KWD nt_select_stmt_opts nt_select_stmt_field_list INTO nt_mysql_select_into_target_list nt_from_dual nt_where_clause_optional.",
        "nt_select_stmt_from_dual_table ::= SELECT_KWD nt_select_stmt_opts nt_select_stmt_field_list INTO nt_mysql_select_file_target nt_from_dual nt_where_clause_optional.",
        "nt_select_stmt ::= SELECT_KWD nt_select_stmt_opts nt_select_stmt_field_list INTO nt_mysql_select_into_target_list.",
        "nt_select_stmt ::= SELECT_KWD nt_select_stmt_opts nt_select_stmt_field_list INTO nt_mysql_select_file_target.",
        "nt_select_stmt_into_option ::= INTO nt_mysql_select_into_target_list.",
        "nt_select_stmt_into_option ::= INTO nt_mysql_select_into_target_list nt_mysql_select_lock_clause_list.",
        "nt_select_stmt_into_option ::= INTO nt_mysql_select_file_target.",
        "nt_select_stmt_into_option ::= INTO DUMPFILE STRING_LIT.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select INTO nt_mysql_select_into_target_list.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select nt_select_stmt_limit INTO nt_mysql_select_into_target_list.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select nt_order_by INTO nt_mysql_select_into_target_list.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select nt_order_by nt_select_stmt_limit INTO nt_mysql_select_into_target_list.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select INTO nt_mysql_select_file_target.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select nt_select_stmt_limit INTO nt_mysql_select_file_target.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select nt_order_by INTO nt_mysql_select_file_target.",
        "nt_set_opr_stmt_with_limit_order_by ::= nt_sub_select nt_order_by nt_select_stmt_limit INTO nt_mysql_select_file_target.",
        "nt_select_lock_opt ::= nt_mysql_select_lock_clause nt_mysql_select_lock_clause_list.",
        "nt_mysql_select_lock_clause_list ::= nt_mysql_select_lock_clause.",
        "nt_mysql_select_lock_clause_list ::= nt_mysql_select_lock_clause_list nt_mysql_select_lock_clause.",
        "nt_mysql_select_lock_clause ::= FOR_KWD UPDATE nt_of_tables_opt.",
        "nt_mysql_select_lock_clause ::= FOR_KWD SHARE nt_of_tables_opt.",
        "nt_mysql_select_lock_clause ::= FOR_KWD UPDATE nt_of_tables_opt NOWAIT.",
        "nt_mysql_select_lock_clause ::= FOR_KWD UPDATE nt_of_tables_opt WAIT nt_num.",
        "nt_mysql_select_lock_clause ::= FOR_KWD SHARE nt_of_tables_opt NOWAIT.",
        "nt_mysql_select_lock_clause ::= FOR_KWD UPDATE nt_of_tables_opt SKIP LOCKED.",
        "nt_mysql_select_lock_clause ::= FOR_KWD SHARE nt_of_tables_opt SKIP LOCKED.",
        "nt_mysql_select_lock_clause ::= LOCK IN SHARE MODE.",
        "nt_limit_option ::= nt_identifier.",
        "nt_mysql_select_file_target ::= OUTFILE STRING_LIT nt_fields nt_lines.",
        "nt_mysql_select_file_target ::= OUTFILE STRING_LIT CHARACTER SET nt_charset_name nt_fields nt_lines.",
        "nt_mysql_select_file_target ::= DUMPFILE STRING_LIT.",
        "nt_mysql_select_into_target_list ::= nt_mysql_select_into_target.",
        "nt_mysql_select_into_target_list ::= nt_mysql_select_into_target_list COMMA nt_mysql_select_into_target.",
        "nt_mysql_select_into_target ::= nt_user_variable.",
        "nt_mysql_select_into_target ::= nt_identifier.",
        "nt_analyze_table_stmt ::= ANALYZE nt_no_write_to_bin_log_alias_opt TABLES nt_table_name_list nt_all_columns_or_predicate_columns_opt nt_analyze_option_list_opt.",
        "nt_analyze_table_stmt ::= ANALYZE nt_no_write_to_bin_log_alias_opt TABLE_KWD nt_table_name UPDATE HISTOGRAM ON nt_ident_list USING DATA STRING_LIT.",
        "nt_mysql_cache_index_stmt ::= CACHE INDEX nt_mysql_load_index_table_list IN nt_mysql_key_cache_name.",
        "nt_mysql_key_cache_name ::= nt_identifier.",
        "nt_mysql_key_cache_name ::= DEFAULT_KWD.",
        "nt_mysql_load_index_stmt ::= LOAD INDEX INTO CACHE nt_mysql_load_index_table_list nt_mysql_ignore_leaves_opt.",
        "nt_mysql_load_index_table_list ::= nt_mysql_load_index_table.",
        "nt_mysql_load_index_table_list ::= nt_mysql_load_index_table_list COMMA nt_mysql_load_index_table.",
        "nt_mysql_load_index_table ::= nt_table_name nt_mysql_load_index_partition_opt nt_mysql_load_index_key_opt.",
        "nt_mysql_load_index_partition_opt ::= .",
        "nt_mysql_load_index_partition_opt ::= PARTITION LPAREN ALL RPAREN.",
        "nt_mysql_load_index_partition_opt ::= PARTITION LPAREN nt_partition_name_list RPAREN.",
        "nt_mysql_load_index_key_opt ::= .",
        "nt_mysql_load_index_key_opt ::= KEY LPAREN nt_index_name_list RPAREN.",
        "nt_mysql_ignore_leaves_opt ::= .",
        "nt_mysql_ignore_leaves_opt ::= IGNORE LEAVES.",
        "nt_load_data_stmt ::= LOAD DATA nt_low_priority_opt nt_local_opt INFILE STRING_LIT nt_format_opt nt_duplicate_opt INTO TABLE_KWD nt_table_name PARTITION LPAREN nt_partition_name_list RPAREN nt_charset_opt nt_fields nt_lines nt_ignore_lines nt_column_name_or_user_var_list_opt_with_brackets nt_load_data_set_spec_opt nt_load_data_option_list_opt.",
        "nt_load_data_set_item ::= nt_simple_ident ASSIGNMENT_EQ nt_expr_or_default.",
        "nt_mysql_load_xml_stmt ::= LOAD XML nt_low_priority_opt nt_local_opt INFILE STRING_LIT nt_duplicate_opt INTO TABLE_KWD nt_table_name nt_charset_opt nt_mysql_rows_identified_opt nt_mysql_load_xml_ignore_rows_opt nt_column_name_or_user_var_list_opt_with_brackets nt_load_data_set_spec_opt.",
        "nt_mysql_rows_identified_opt ::= .",
        "nt_mysql_rows_identified_opt ::= ROWS IDENTIFIED BY STRING_LIT.",
        "nt_mysql_load_xml_ignore_rows_opt ::= .",
        "nt_mysql_load_xml_ignore_rows_opt ::= IGNORE nt_num LINES.",
        "nt_mysql_load_xml_ignore_rows_opt ::= IGNORE nt_num ROWS.",
        "nt_mysql_import_table_stmt ::= IMPORT_KWD TABLE_KWD FROM nt_string_list.",
        "nt_mysql_server_stmt ::= CREATE SERVER nt_mysql_server_name FOREIGN DATA WRAPPER nt_mysql_server_name OPTIONS LPAREN nt_mysql_server_option_list RPAREN.",
        "nt_mysql_server_stmt ::= ALTER SERVER nt_mysql_server_name OPTIONS LPAREN nt_mysql_server_option_list RPAREN.",
        "nt_mysql_server_stmt ::= DROP SERVER nt_mysql_server_name.",
        "nt_mysql_server_name ::= nt_string_name.",
        "nt_mysql_server_option_list ::= nt_mysql_server_option.",
        "nt_mysql_server_option_list ::= nt_mysql_server_option_list COMMA nt_mysql_server_option.",
        "nt_mysql_server_option ::= nt_mysql_server_option_key nt_mysql_server_option_value.",
        "nt_mysql_server_option_key ::= nt_identifier.",
        "nt_mysql_server_option_key ::= DATABASE.",
        "nt_mysql_server_option_value ::= nt_string_name.",
        "nt_mysql_server_option_value ::= nt_num.",
        "nt_mysql_revoke_proxy_stmt ::= REVOKE PROXY ON nt_username FROM nt_username_list nt_mysql_ignore_unknown_user_opt.",
        "nt_mysql_revoke_proxy_stmt ::= REVOKE IF_KWD EXISTS PROXY ON nt_username FROM nt_username_list nt_mysql_ignore_unknown_user_opt.",
        "nt_mysql_ignore_unknown_user_opt ::= .",
        "nt_mysql_ignore_unknown_user_opt ::= IGNORE UNKNOWN USER.",
        "nt_help_stmt ::= HELP nt_identifier.",
        "nt_do_stmt ::= DO nt_mysql_do_expression_alias_list.",
        "nt_mysql_do_expression_alias_list ::= nt_expression nt_field_as_name.",
        "nt_mysql_do_expression_alias_list ::= nt_mysql_do_expression_alias_list COMMA nt_expression nt_field_as_name.",
        "nt_rename_table_stmt ::= RENAME TABLES nt_table_to_table_list.",
        "nt_begin_transaction_stmt ::= START TRANSACTION READ ONLY COMMA WITH CONSISTENT SNAPSHOT.",
        "nt_begin_transaction_stmt ::= START TRANSACTION WITH CONSISTENT SNAPSHOT COMMA READ ONLY.",
        "nt_begin_transaction_stmt ::= START TRANSACTION READ WRITE COMMA WITH CONSISTENT SNAPSHOT.",
        "nt_begin_transaction_stmt ::= START TRANSACTION WITH CONSISTENT SNAPSHOT COMMA READ WRITE.",
        "nt_begin_transaction_stmt ::= BEGIN WORK.",
        "nt_commit_stmt ::= COMMIT WORK.",
        "nt_commit_stmt ::= COMMIT WORK nt_completion_type_within_transaction.",
        "nt_rollback_stmt ::= ROLLBACK WORK.",
        "nt_rollback_stmt ::= ROLLBACK WORK nt_completion_type_within_transaction.",
        "nt_rollback_stmt ::= ROLLBACK WORK TO nt_identifier.",
        "nt_rollback_stmt ::= ROLLBACK WORK TO SAVEPOINT nt_identifier.",
        "nt_create_table_stmt ::= CREATE nt_opt_temporary TABLE_KWD nt_if_not_exists nt_table_name nt_table_element_list_opt nt_create_table_option_list_opt START TRANSACTION.",
        "nt_alter_table_spec ::= ALTER nt_column_keyword_opt nt_column_name SET VISIBLE.",
        "nt_alter_table_spec ::= ALTER nt_column_keyword_opt nt_column_name SET INVISIBLE.",
        "nt_database_option ::= READ ONLY nt_eq_opt DEFAULT_KWD.",
        "nt_database_sym ::= SCHEMA.",
        "nt_view_check_option ::= WITH CHECK OPTION.",
        "nt_explain_format_type ::= TREE.",
        "nt_cast_type ::= nt_n_char nt_opt_field_len.",
        "nt_cast_type ::= nt_n_varchar nt_opt_field_len.",
        "nt_cast_type ::= nt_char nt_opt_field_len nt_mysql_charset_shorthand.",
        "nt_cast_type ::= nt_n_char nt_opt_field_len nt_mysql_charset_shorthand.",
        "nt_cast_type ::= nt_varchar nt_field_len nt_mysql_charset_shorthand.",
        "nt_cast_type ::= nt_n_varchar nt_field_len nt_mysql_charset_shorthand.",
        "nt_cast_type ::= DOUBLE_TYPE PRECISION_TYPE.",
        "nt_opt_binary ::= nt_charset_kw nt_charset_name nt_opt_bin_mod COLLATE nt_collation_name.",
        "nt_opt_binary ::= BINARY_TYPE nt_opt_charset COLLATE nt_collation_name.",
        "nt_opt_binary ::= COLLATE nt_collation_name.",
        "nt_string_type ::= nt_char nt_field_len nt_mysql_charset_shorthand.",
        "nt_string_type ::= nt_char nt_mysql_charset_shorthand.",
        "nt_string_type ::= nt_varchar nt_field_len nt_mysql_charset_shorthand.",
        "nt_string_type ::= nt_n_char nt_field_len nt_mysql_charset_shorthand.",
        "nt_string_type ::= nt_n_char nt_mysql_charset_shorthand.",
        "nt_string_type ::= nt_n_varchar nt_field_len nt_mysql_charset_shorthand.",
        "nt_mysql_charset_shorthand ::= ASCII.",
        "nt_mysql_charset_shorthand ::= UNICODE_SYM.",
        "nt_mysql_charset_shorthand ::= BYTE_TYPE.",
        "nt_mysql_charset_shorthand ::= ASCII BINARY_TYPE.",
        "nt_mysql_charset_shorthand ::= BINARY_TYPE ASCII.",
        "nt_mysql_charset_shorthand ::= UNICODE_SYM BINARY_TYPE.",
        "nt_mysql_charset_shorthand ::= BINARY_TYPE UNICODE_SYM.",
        "nt_mysql_charset_shorthand ::= BYTE_TYPE BINARY_TYPE.",
        "nt_mysql_charset_shorthand ::= BINARY_TYPE BYTE_TYPE.",
        "nt_float_opt ::= LPAREN DEC_LIT RPAREN.",
        "nt_table_factor ::= nt_sub_select nt_table_as_name nt_mysql_ident_list_with_paren.",
        "nt_mysql_ident_list_with_paren ::= LPAREN nt_ident_list RPAREN.",
        "nt_table_factor ::= JSON_TABLE LPAREN nt_expression COMMA STRING_LIT COLUMNS LPAREN nt_mysql_json_table_column_list RPAREN RPAREN nt_table_as_name_opt.",
        "nt_mysql_json_table_column_list ::= nt_mysql_json_table_column.",
        "nt_mysql_json_table_column_list ::= nt_mysql_json_table_column_list COMMA nt_mysql_json_table_column.",
        "nt_mysql_json_table_column ::= nt_identifier FOR_KWD ORDINALITY.",
        "nt_mysql_json_table_column ::= nt_identifier nt_type PATH STRING_LIT nt_mysql_json_table_response_list_opt.",
        "nt_mysql_json_table_column ::= nt_identifier nt_type EXISTS PATH STRING_LIT nt_mysql_json_table_response_list_opt.",
        "nt_mysql_json_table_response_list_opt ::= .",
        "nt_mysql_json_table_response_list_opt ::= nt_mysql_json_table_response_list.",
        "nt_mysql_json_table_response_list ::= nt_mysql_json_table_response.",
        "nt_mysql_json_table_response_list ::= nt_mysql_json_table_response_list nt_mysql_json_table_response.",
        "nt_mysql_json_table_response ::= DEFAULT_KWD nt_expression ON EMPTY_KWD.",
        "nt_mysql_json_table_response ::= DEFAULT_KWD nt_expression ON ERROR_KWD.",
        "nt_mysql_json_table_response ::= NULL ON EMPTY_KWD.",
        "nt_mysql_json_table_response ::= NULL ON ERROR_KWD.",
        "nt_mysql_json_table_response ::= ERROR_KWD ON EMPTY_KWD.",
        "nt_mysql_json_table_response ::= ERROR_KWD ON ERROR_KWD.",
        "nt_type ::= nt_mysql_spatial_type.",
        "nt_mysql_spatial_type ::= GEOMETRY.",
        "nt_mysql_spatial_type ::= POINT.",
        "nt_mysql_spatial_type ::= LINESTRING.",
        "nt_mysql_spatial_type ::= POLYGON.",
        "nt_mysql_spatial_type ::= MULTIPOINT.",
        "nt_mysql_spatial_type ::= MULTILINESTRING.",
        "nt_mysql_spatial_type ::= MULTIPOLYGON.",
        "nt_mysql_spatial_type ::= GEOMETRYCOLLECTION.",
        "nt_function_name_conflict ::= GEOMETRY.",
        "nt_function_name_conflict ::= GEOMETRYCOLLECTION.",
        "nt_function_name_conflict ::= LINESTRING.",
        "nt_function_name_conflict ::= MULTIPOINT.",
        "nt_function_name_conflict ::= MULTILINESTRING.",
        "nt_function_name_conflict ::= MULTIPOLYGON.",
        "nt_function_name_conflict ::= POLYGON.",
        "nt_function_name_conflict ::= SCHEMA.",
        "nt_column_option ::= SRID nt_length_num.",
        "nt_column_option ::= VISIBLE.",
        "nt_column_option ::= INVISIBLE.",
        "nt_column_option ::= SECONDARY.",
        "nt_column_option ::= NOT SECONDARY.",
        "nt_default_value_expr ::= LPAREN BUILTIN_CUR_DATE LPAREN RPAREN RPAREN.",
        "nt_default_value_expr ::= LPAREN nt_mysql_default_binary_expr RPAREN.",
        "nt_default_value_expr ::= LPAREN nt_mysql_default_unary_expr RPAREN.",
        "nt_default_value_expr ::= LPAREN UNDERSCORE_CS STRING_LIT RPAREN.",
        "nt_mysql_default_binary_expr ::= nt_mysql_default_operand nt_mysql_default_operator nt_mysql_default_operand.",
        "nt_mysql_default_unary_expr ::= PLUS nt_identifier.",
        "nt_mysql_default_unary_expr ::= MINUS nt_identifier.",
        "nt_mysql_default_operand ::= nt_identifier.",
        "nt_mysql_default_operand ::= nt_mysql_default_unary_expr.",
        "nt_mysql_default_operand ::= nt_signed_literal.",
        "nt_mysql_default_operand ::= nt_builtin_function.",
        "nt_builtin_function ::= nt_function_name_date_arith_multi_forms LPAREN nt_expression COMMA nt_expression RPAREN.",
        "nt_builtin_function ::= nt_function_name_date_arith_multi_forms LPAREN nt_expression COMMA INTERVAL nt_expression nt_time_unit RPAREN.",
        "nt_builtin_function ::= nt_function_name_date_arith LPAREN nt_expression COMMA INTERVAL nt_expression nt_time_unit RPAREN.",
        "nt_builtin_function ::= CURRENT_TIME nt_func_datetime_prec.",
        "nt_builtin_function ::= CURRENT_USER nt_optional_braces.",
        "nt_builtin_function ::= DATABASE LPAREN RPAREN.",
        "nt_builtin_function ::= USER LPAREN RPAREN.",
        "nt_builtin_function ::= BUILTIN_USER LPAREN nt_expression_list_opt RPAREN.",
        "nt_builtin_function ::= UTC_DATE nt_optional_braces.",
        "nt_builtin_function ::= UTC_TIME nt_func_datetime_prec.",
        "nt_builtin_function ::= UTC_TIMESTAMP nt_func_datetime_prec.",
        "nt_builtin_function ::= BUILTIN_CUR_TIME LPAREN nt_func_datetime_prec_list_opt RPAREN.",
        "nt_builtin_function ::= BUILTIN_SYS_DATE LPAREN nt_func_datetime_prec_list_opt RPAREN.",
        "nt_builtin_function ::= REPEAT LPAREN nt_expression_list RPAREN.",
        "nt_builtin_function ::= TIMESTAMP_ADD LPAREN nt_timestamp_unit COMMA nt_expression COMMA nt_expression RPAREN.",
        "nt_mysql_default_operator ::= STAR.",
        "nt_mysql_default_operator ::= SLASH.",
        "nt_mysql_default_operator ::= DIV.",
        "nt_mysql_default_operator ::= PLUS.",
        "nt_mysql_default_operator ::= MINUS.",
        "nt_constraint_elem ::= SPATIAL nt_key_or_index_opt nt_index_name LPAREN nt_index_part_specification_list RPAREN nt_index_option_list.",
        "nt_alter_table_stmt ::= ALTER nt_ignore_optional TABLE_KWD nt_table_name nt_alter_table_spec_list COMMA nt_alter_table_spec_single_opt.",
        "nt_show_target_filterable ::= EXTENDED nt_opt_full TABLES nt_show_database_name_opt.",
        "nt_mysql_handler_stmt ::= HANDLER nt_table_name OPEN.",
        "nt_mysql_handler_stmt ::= HANDLER nt_table_name OPEN AS nt_identifier.",
        "nt_mysql_handler_stmt ::= HANDLER nt_table_name OPEN nt_identifier.",
        "nt_mysql_handler_stmt ::= HANDLER nt_table_name CLOSE.",
        "nt_mysql_handler_stmt ::= HANDLER nt_table_name READ nt_mysql_handler_read_target nt_where_clause_optional nt_select_stmt_limit_opt.",
        "nt_mysql_handler_read_target ::= nt_mysql_handler_read_direction.",
        "nt_mysql_handler_read_target ::= nt_identifier nt_mysql_handler_read_direction.",
        "nt_mysql_handler_read_target ::= nt_identifier nt_mysql_handler_key_op LPAREN nt_expression_list RPAREN.",
        "nt_mysql_handler_read_direction ::= FIRST.",
        "nt_mysql_handler_read_direction ::= NEXT.",
        "nt_mysql_handler_read_direction ::= PREV.",
        "nt_mysql_handler_read_direction ::= LAST.",
        "nt_mysql_handler_key_op ::= EQ.",
        "nt_mysql_handler_key_op ::= LE.",
        "nt_mysql_handler_key_op ::= GE.",
        "nt_mysql_handler_key_op ::= LT.",
        "nt_mysql_handler_key_op ::= GT.",
        "nt_mysql_xa_stmt ::= XA START nt_mysql_xa_xid nt_mysql_xa_start_option_opt.",
        "nt_mysql_xa_stmt ::= XA END nt_mysql_xa_xid nt_mysql_xa_end_option_opt.",
        "nt_mysql_xa_stmt ::= XA PREPARE nt_mysql_xa_xid.",
        "nt_mysql_xa_stmt ::= XA COMMIT nt_mysql_xa_xid nt_mysql_xa_commit_option_opt.",
        "nt_mysql_xa_stmt ::= XA ROLLBACK nt_mysql_xa_xid.",
        "nt_mysql_xa_stmt ::= XA RECOVER nt_mysql_xa_recover_option_opt.",
        "nt_mysql_xa_xid ::= nt_expression.",
        "nt_mysql_xa_xid ::= nt_expression COMMA nt_expression.",
        "nt_mysql_xa_xid ::= nt_expression COMMA nt_expression COMMA nt_expression.",
        "nt_mysql_xa_start_option_opt ::= .",
        "nt_mysql_xa_start_option_opt ::= JOIN.",
        "nt_mysql_xa_start_option_opt ::= RESUME.",
        "nt_mysql_xa_end_option_opt ::= .",
        "nt_mysql_xa_end_option_opt ::= SUSPEND.",
        "nt_mysql_xa_end_option_opt ::= SUSPEND FOR_KWD MIGRATE.",
        "nt_mysql_xa_commit_option_opt ::= .",
        "nt_mysql_xa_commit_option_opt ::= ONE PHASE.",
        "nt_mysql_xa_recover_option_opt ::= .",
        "nt_mysql_xa_recover_option_opt ::= CONVERT XID.",
        "nt_mysql_plugin_stmt ::= INSTALL PLUGIN nt_identifier SONAME STRING_LIT.",
        "nt_mysql_plugin_stmt ::= UNINSTALL PLUGIN nt_identifier.",
        "nt_mysql_plugin_stmt ::= INSTALL COMPONENT nt_string_list. [LOWER_THAN_SET_KEYWORD]",
        "nt_mysql_plugin_stmt ::= INSTALL COMPONENT nt_string_list SET nt_mysql_component_set_list.",
        "nt_mysql_plugin_stmt ::= UNINSTALL COMPONENT nt_string_list. [LOWER_THAN_SET_KEYWORD]",
        "nt_mysql_component_set_list ::= nt_mysql_component_set_item.",
        "nt_mysql_component_set_list ::= nt_mysql_component_set_list COMMA nt_mysql_component_set_item.",
        "nt_mysql_component_set_item ::= nt_variable_assignment.",
        "nt_mysql_component_set_item ::= PERSIST nt_variable_assignment.",
        "nt_mysql_component_set_item ::= PERSIST_ONLY nt_variable_assignment.",
        "nt_mysql_change_replication_stmt ::= CHANGE REPLICATION SOURCE TO nt_mysql_replication_option_list. [LOWER_THAN_SET_KEYWORD]",
        "nt_mysql_change_replication_stmt ::= CHANGE REPLICATION SOURCE TO nt_mysql_replication_option_list FOR_KWD CHANNEL STRING_LIT.",
        "nt_mysql_change_replication_stmt ::= CHANGE MASTER TO nt_mysql_replication_option_list. [LOWER_THAN_SET_KEYWORD]",
        "nt_mysql_change_replication_stmt ::= CHANGE MASTER TO nt_mysql_replication_option_list FOR_KWD CHANNEL STRING_LIT.",
        "nt_mysql_purge_logs_stmt ::= PURGE nt_mysql_binary_or_master LOGS TO STRING_LIT.",
        "nt_mysql_purge_logs_stmt ::= PURGE nt_mysql_binary_or_master LOGS BEFORE nt_expression.",
        "nt_mysql_binary_or_master ::= BINARY_TYPE.",
        "nt_mysql_binary_or_master ::= MASTER.",
        "nt_mysql_replication_control_stmt ::= START SLAVE.",
        "nt_mysql_replication_control_stmt ::= START REPLICA.",
        "nt_mysql_replication_control_stmt ::= STOP SLAVE.",
        "nt_mysql_replication_control_stmt ::= STOP REPLICA.",
        "nt_mysql_replication_option_list ::= nt_mysql_replication_option.",
        "nt_mysql_replication_option_list ::= nt_mysql_replication_option_list COMMA nt_mysql_replication_option.",
        "nt_mysql_replication_option ::= nt_identifier nt_eq_or_assignment_eq nt_mysql_replication_option_value.",
        "nt_mysql_replication_option_value ::= nt_mysql_replication_scalar_value.",
        "nt_mysql_replication_option_value ::= LPAREN nt_mysql_int_lit_list RPAREN.",
        "nt_mysql_replication_scalar_value ::= STRING_LIT.",
        "nt_mysql_replication_scalar_value ::= INT_LIT.",
        "nt_mysql_replication_scalar_value ::= FLOAT_LIT.",
        "nt_mysql_replication_scalar_value ::= DEC_LIT.",
        "nt_mysql_replication_scalar_value ::= HEX_LIT.",
        "nt_mysql_replication_scalar_value ::= BIT_LIT.",
        "nt_mysql_replication_scalar_value ::= NULL.",
        "nt_mysql_replication_scalar_value ::= TRUE_KWD.",
        "nt_mysql_replication_scalar_value ::= FALSE_KWD.",
        "nt_mysql_replication_scalar_value ::= nt_identifier.",
        "nt_mysql_int_lit_list ::= INT_LIT.",
        "nt_mysql_int_lit_list ::= nt_mysql_int_lit_list COMMA INT_LIT.",
        "nt_mysql_create_trigger_stmt ::= CREATE TRIGGER nt_if_not_exists nt_table_name nt_mysql_trigger_time nt_mysql_trigger_event ON nt_table_name FOR_KWD EACH ROW nt_mysql_trigger_body.",
        "nt_mysql_create_trigger_stmt ::= CREATE TRIGGER nt_if_not_exists nt_table_name nt_mysql_trigger_time nt_mysql_trigger_event ON nt_table_name FOR_KWD EACH ROW nt_mysql_trigger_order nt_mysql_trigger_body.",
        "nt_mysql_trigger_time ::= BEFORE.",
        "nt_mysql_trigger_time ::= AFTER.",
        "nt_mysql_trigger_event ::= INSERT.",
        "nt_mysql_trigger_event ::= UPDATE.",
        "nt_mysql_trigger_event ::= DELETE_KWD.",
        "nt_mysql_trigger_order ::= FOLLOWS nt_table_name.",
        "nt_mysql_trigger_order ::= PRECEDES nt_table_name.",
        "nt_mysql_trigger_body ::= nt_procedure_proc_stmt.",
        "nt_mysql_drop_trigger_stmt ::= DROP TRIGGER nt_if_exists nt_table_name.",
        "nt_show_stmt ::= SHOW CREATE TRIGGER nt_table_name.",
        "nt_mysql_create_event_stmt ::= CREATE nt_or_replace nt_view_algorithm nt_view_definer EVENT nt_if_not_exists nt_table_name ON SCHEDULE nt_mysql_event_schedule nt_mysql_event_create_options DO nt_mysql_event_body.",
        "nt_mysql_alter_event_stmt ::= ALTER nt_view_algorithm nt_view_definer EVENT nt_table_name nt_mysql_event_alter_options.",
        "nt_mysql_drop_event_stmt ::= DROP EVENT nt_if_exists nt_table_name.",
        "nt_show_stmt ::= SHOW CREATE EVENT nt_table_name.",
        "nt_mysql_event_schedule ::= AT nt_expression.",
        "nt_mysql_event_schedule ::= EVERY nt_expression nt_time_unit nt_mysql_event_schedule_bounds_opt.",
        "nt_mysql_event_schedule_bounds_opt ::= .",
        "nt_mysql_event_schedule_bounds_opt ::= STARTS nt_expression.",
        "nt_mysql_event_schedule_bounds_opt ::= ENDS nt_expression.",
        "nt_mysql_event_schedule_bounds_opt ::= STARTS nt_expression ENDS nt_expression.",
        "nt_mysql_event_create_options ::= nt_mysql_event_completion_opt nt_mysql_event_status_opt nt_mysql_event_comment_opt.",
        "nt_mysql_event_alter_options ::= ON SCHEDULE nt_mysql_event_schedule nt_mysql_event_completion_opt nt_mysql_event_status_opt nt_mysql_event_rename_opt nt_mysql_event_comment_opt nt_mysql_event_do_opt.",
        "nt_mysql_event_alter_options ::= ON COMPLETION nt_mysql_event_completion nt_mysql_event_status_opt nt_mysql_event_rename_opt nt_mysql_event_comment_opt nt_mysql_event_do_opt.",
        "nt_mysql_event_alter_options ::= nt_mysql_event_status nt_mysql_event_rename_opt nt_mysql_event_comment_opt nt_mysql_event_do_opt.",
        "nt_mysql_event_alter_options ::= nt_mysql_event_rename nt_mysql_event_comment_opt nt_mysql_event_do_opt.",
        "nt_mysql_event_alter_options ::= nt_mysql_event_rename nt_mysql_event_status nt_mysql_event_comment_opt nt_mysql_event_do_opt.",
        "nt_mysql_event_alter_options ::= nt_mysql_event_comment nt_mysql_event_do_opt.",
        "nt_mysql_event_alter_options ::= nt_mysql_event_do.",
        "nt_mysql_event_completion_opt ::= . [LOWER_THAN_ON]",
        "nt_mysql_event_completion_opt ::= ON COMPLETION nt_mysql_event_completion.",
        "nt_mysql_event_completion ::= PRESERVE.",
        "nt_mysql_event_completion ::= NOT PRESERVE.",
        "nt_mysql_event_status_opt ::= .",
        "nt_mysql_event_status_opt ::= nt_mysql_event_status.",
        "nt_mysql_event_status ::= ENABLE.",
        "nt_mysql_event_status ::= ENABLED.",
        "nt_mysql_event_status ::= DISABLE.",
        "nt_mysql_event_status ::= DISABLED.",
        "nt_mysql_event_status ::= DISABLE ON SLAVE.",
        "nt_mysql_event_status ::= DISABLED ON SLAVE.",
        "nt_mysql_event_rename_opt ::= .",
        "nt_mysql_event_rename_opt ::= nt_mysql_event_rename.",
        "nt_mysql_event_rename ::= RENAME TO nt_table_name.",
        "nt_mysql_event_comment_opt ::= .",
        "nt_mysql_event_comment_opt ::= nt_mysql_event_comment.",
        "nt_mysql_event_comment ::= COMMENT STRING_LIT.",
        "nt_mysql_event_do_opt ::= .",
        "nt_mysql_event_do_opt ::= nt_mysql_event_do.",
        "nt_mysql_event_do ::= DO nt_mysql_event_body.",
        "nt_mysql_event_body ::= EVENT_BODY.",
        "nt_mysql_repair_table_stmt ::= REPAIR nt_table_or_tables nt_table_name_list nt_mysql_repair_table_option_list_opt.",
        "nt_mysql_repair_table_option_list_opt ::= .",
        "nt_mysql_repair_table_option_list_opt ::= nt_mysql_repair_table_option_list.",
        "nt_mysql_repair_table_option_list ::= nt_mysql_repair_table_option.",
        "nt_mysql_repair_table_option_list ::= nt_mysql_repair_table_option_list nt_mysql_repair_table_option.",
        "nt_mysql_repair_table_option ::= QUICK.",
        "nt_mysql_repair_table_option ::= EXTENDED.",
        "nt_mysql_repair_table_option ::= USE_FRM.",
        "nt_mysql_checksum_table_stmt ::= CHECKSUM nt_table_or_tables nt_table_name_list nt_mysql_checksum_table_option_opt.",
        "nt_mysql_checksum_table_option_opt ::= .",
        "nt_mysql_checksum_table_option_opt ::= QUICK.",
        "nt_mysql_checksum_table_option_opt ::= EXTENDED.",
        "nt_mysql_reset_stmt ::= RESET MASTER.",
        "nt_mysql_reset_stmt ::= RESET BINARY_TYPE LOGS AND GTIDS.",
        "nt_mysql_reset_stmt ::= RESET REPLICA nt_mysql_reset_all_opt.",
        "nt_mysql_reset_stmt ::= RESET SLAVE nt_mysql_reset_all_opt.",
        "nt_mysql_reset_stmt ::= RESET SOURCE nt_mysql_reset_all_opt.",
        "nt_mysql_reset_stmt ::= RESET PERSIST.",
        "nt_mysql_reset_stmt ::= RESET PERSIST nt_variable_name.",
        "nt_mysql_reset_stmt ::= RESET PERSIST IF_KWD EXISTS nt_variable_name.",
        "nt_mysql_reset_all_opt ::= .",
        "nt_mysql_reset_all_opt ::= ALL.",
        "nt_mysql_tablespace_stmt ::= CREATE TABLESPACE nt_identifier nt_mysql_tablespace_option_list_opt.",
        "nt_mysql_tablespace_stmt ::= CREATE UNDO TABLESPACE nt_identifier nt_mysql_tablespace_option_list_opt.",
        "nt_mysql_tablespace_stmt ::= ALTER TABLESPACE nt_identifier nt_mysql_tablespace_option_list_opt.",
        "nt_mysql_tablespace_stmt ::= ALTER UNDO TABLESPACE nt_identifier SET nt_mysql_undo_tablespace_state nt_mysql_tablespace_option_list_opt.",
        "nt_mysql_tablespace_stmt ::= ALTER TABLESPACE nt_identifier RENAME TO nt_identifier.",
        "nt_mysql_tablespace_stmt ::= DROP TABLESPACE nt_identifier nt_mysql_tablespace_option_list_opt.",
        "nt_mysql_tablespace_stmt ::= DROP UNDO TABLESPACE nt_identifier nt_mysql_tablespace_option_list_opt.",
        "nt_mysql_undo_tablespace_state ::= ACTIVE.",
        "nt_mysql_undo_tablespace_state ::= INACTIVE.",
        "nt_mysql_tablespace_option_list_opt ::= .",
        "nt_mysql_tablespace_option_list_opt ::= nt_mysql_tablespace_option_list.",
        "nt_mysql_tablespace_option_list ::= nt_mysql_tablespace_option.",
        "nt_mysql_tablespace_option_list ::= nt_mysql_tablespace_option_list nt_mysql_tablespace_option.",
        "nt_mysql_tablespace_option ::= ADD DATAFILE STRING_LIT.",
        "nt_mysql_tablespace_option ::= ENGINE nt_eq_opt nt_identifier.",
        "nt_mysql_tablespace_option ::= ENGINE_ATTRIBUTE nt_eq_opt STRING_LIT.",
        "nt_mysql_tablespace_option ::= ENCRYPTION nt_eq_opt nt_mysql_replication_scalar_value.",
        "nt_mysql_tablespace_option ::= SECONDARY_ENGINE_ATTRIBUTE nt_eq_opt STRING_LIT.",
        "nt_table_lock ::= nt_table_name AS nt_identifier nt_lock_type.",
        "nt_table_lock ::= nt_table_name nt_identifier nt_lock_type.",
        "nt_lock_type ::= LOW_PRIORITY WRITE.",
        "nt_join_type ::= INNER.",
        "nt_mysql_lock_instance_stmt ::= LOCK INSTANCE FOR_KWD BACKUP.",
        "nt_mysql_lock_instance_stmt ::= UNLOCK INSTANCE.",
        "nt_kill_stmt ::= nt_kill_or_kill_ti_db IDENTIFIER.",
        "nt_kill_stmt ::= nt_kill_or_kill_ti_db CONNECTION IDENTIFIER.",
        "nt_kill_stmt ::= nt_kill_or_kill_ti_db QUERY IDENTIFIER.",
        "nt_kill_stmt ::= nt_kill_or_kill_ti_db nt_user_variable.",
        "nt_kill_stmt ::= nt_kill_or_kill_ti_db CONNECTION nt_user_variable.",
        "nt_kill_stmt ::= nt_kill_or_kill_ti_db QUERY nt_user_variable.",
        "nt_mysql_check_table_stmt ::= CHECK nt_table_or_tables nt_table_name_list nt_mysql_check_table_option_list_opt.",
        "nt_mysql_check_table_option_list_opt ::= .",
        "nt_mysql_check_table_option_list_opt ::= nt_mysql_check_table_option_list.",
        "nt_mysql_check_table_option_list ::= nt_mysql_check_table_option.",
        "nt_mysql_check_table_option_list ::= nt_mysql_check_table_option_list nt_mysql_check_table_option.",
        "nt_mysql_check_table_option ::= QUICK.",
        "nt_mysql_check_table_option ::= FAST.",
        "nt_mysql_check_table_option ::= MEDIUM.",
        "nt_mysql_check_table_option ::= EXTENDED.",
        "nt_mysql_check_table_option ::= CHANGED.",
        "nt_mysql_check_table_option ::= FOR_KWD UPGRADE.",
    ]


def builtin_keyword(symbol: str) -> str | None:
    if not symbol.startswith("builtin"):
        return None

    exceptions = {
        "builtinApproxCountDistinct": "APPROX_COUNT_DISTINCT",
        "builtinApproxPercentile": "APPROX_PERCENTILE",
        "builtinBitAnd": "BIT_AND",
        "builtinBitOr": "BIT_OR",
        "builtinBitXor": "BIT_XOR",
        "builtinCurDate": "CURDATE",
        "builtinCurTime": "CURTIME",
        "builtinDateAdd": "DATE_ADD",
        "builtinDateSub": "DATE_SUB",
        "builtinGroupConcat": "GROUP_CONCAT",
        "builtinStddevPop": "STDDEV_POP",
        "builtinStddevSamp": "STDDEV_SAMP",
        "builtinSysDate": "SYSDATE",
        "builtinVarPop": "VAR_POP",
        "builtinVarSamp": "VAR_SAMP",
    }
    if symbol in exceptions:
        return exceptions[symbol]
    return terminal_name(symbol.removeprefix("builtin"))


def map_symbol(
    symbol: str,
    token_symbols: set[str],
    display_to_symbol: dict[str, str],
    nonterminals: set[str],
    used_terminals: set[str],
) -> str:
    if symbol.startswith(("'", '"')):
        return map_quoted_terminal(symbol, display_to_symbol, used_terminals)
    if symbol in token_symbols:
        return map_terminal_symbol(symbol, display_to_symbol, used_terminals)
    if symbol in nonterminals:
        return map_nonterminal(symbol)
    raise ValueError(f"unknown grammar symbol {symbol!r}")


def map_quoted_terminal(
    symbol: str, display_to_symbol: dict[str, str], used_terminals: set[str]
) -> str:
    value = unquote(symbol)
    if value in display_to_symbol:
        token_symbol = display_to_symbol[value]
        used_terminals.add(token_symbol)
        return terminal_name(token_symbol)
    synthetic = char_token_name(value)
    used_terminals.add(synthetic)
    return synthetic


def map_terminal_symbol(
    symbol: str, display_to_symbol: dict[str, str], used_terminals: set[str]
) -> str:
    if symbol.startswith(("'", '"')):
        return map_quoted_terminal(symbol, display_to_symbol, used_terminals)
    used_terminals.add(symbol)
    return terminal_name(symbol)


def map_nonterminal(symbol: str) -> str:
    return "nt_" + terminal_name(symbol).lower()


def terminal_name(symbol: str) -> str:
    if symbol.startswith("'") and symbol.endswith("'"):
        return char_token_name(unquote(symbol))

    output: list[str] = []
    previous = ""
    for i, char in enumerate(symbol):
        if (
            char.isupper()
            and i > 0
            and (previous.islower() or previous.isdigit() or (i + 1 < len(symbol) and symbol[i + 1].islower()))
        ):
            output.append("_")
        output.append(char.upper() if char.isalnum() else "_")
        previous = char
    return re.sub(r"_+", "_", "".join(output)).strip("_") or "EMPTY"


def char_token_name(value: str) -> str:
    if value in CHAR_TOKEN_NAMES:
        return CHAR_TOKEN_NAMES[value]
    return "CHAR_" + "_".join(f"{ord(char):02X}" for char in value)


def unquote(value: str) -> str:
    return bytes(value[1:-1], "utf-8").decode("unicode_escape")


if __name__ == "__main__":
    main()
