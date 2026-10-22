#!/usr/bin/env python3
"""Создать простые .txt-версии документации для релизного архива.

Markdown остаётся основным форматом в репозитории. Этот скрипт нужен только
при упаковке: человеку, который распаковал игру, проще открыть README.txt или
docs/gameplay.txt в блокноте, чем искать просмотрщик Markdown.

Использование:
    python tools/md_to_txt.py [--source .] [--output build/txt-docs]
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

DOCUMENTS = [
    ("README.md", "README.txt"),
    ("docs/gameplay.md", "docs/gameplay.txt"),
    ("docs/level-authoring.md", "docs/level-authoring.txt"),
    ("docs/level-design.md", "docs/level-design.txt"),
    ("docs/game.md", "docs/game.txt"),
    ("docs/architecture.md", "docs/architecture.txt"),
    ("docs/level-audit.md", "docs/level-audit.txt"),
    ("docs/level-renumbering.md", "docs/level-renumbering.txt"),
]

LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
INLINE_CODE_RE = re.compile(r"`([^`]+)`")
IMAGE_RE = re.compile(r"!\[([^\]]*)\]\([^)]+\)")
HEADING_RE = re.compile(r"^\s{0,3}#{1,6}\s*")
BLOCKQUOTE_RE = re.compile(r"^\s*>\s?")
TABLE_SEPARATOR_RE = re.compile(r"^\s*\|?[\s:|-]+\|[\s:|-]*$")
HTML_TAG_RE = re.compile(r"<[^>]+>")


def strip_inline_markdown(text: str) -> str:
    text = IMAGE_RE.sub(r"\1", text)
    text = LINK_RE.sub(lambda m: m.group(1) if m.group(2).startswith(("http://", "https://")) else m.group(1), text)

    # Содержимое `...` не должно участвовать в разметке: иначе `**` в
    # примерах превращается в пустое место. Прячем его до обработки.
    code_spans: list[str] = []

    def protect_code(match: re.Match[str]) -> str:
        code_spans.append(match.group(1))
        return f"\x00code{len(code_spans) - 1}\x00"

    text = INLINE_CODE_RE.sub(protect_code, text)
    text = re.sub(r"\*\*([^*\n]+)\*\*", r"\1", text)
    text = re.sub(r"__([^_\n]+)__", r"\1", text)
    # Одиночные * и _ часто являются акцентами, но могут быть частью текста.
    text = re.sub(r"(?<!\w)[*_]([^*_\n]+)[*_](?!\w)", r"\1", text)
    text = HTML_TAG_RE.sub("", text)

    for index, code in enumerate(code_spans):
        text = text.replace(f"\x00code{index}\x00", code)
    return text


def markdown_to_text(markdown: str) -> str:
    lines = markdown.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    result: list[str] = []
    in_code = False
    code_language = ""

    for raw in lines:
        line = raw.rstrip()

        if line.strip().startswith("```"):
            if not in_code:
                in_code = True
                code_language = line.strip()[3:].strip()
            else:
                in_code = False
                code_language = ""
            result.append("")
            continue

        if in_code:
            result.append("    " + line if line else "")
            continue

        if TABLE_SEPARATOR_RE.match(line):
            continue

        line = HEADING_RE.sub("", line)
        line = BLOCKQUOTE_RE.sub("", line)
        line = strip_inline_markdown(line)

        # Таблицу оставляем читаемой: убираем только внешние палки.
        if line.lstrip().startswith("|") and line.rstrip().endswith("|"):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            line = " | ".join(cells)

        if line.strip() == "---":
            result.append("")
            continue

        result.append(line)

    # Больше двух пустых строк подряд не нужно.
    compact: list[str] = []
    empty = 0
    for line in result:
        if line.strip():
            empty = 0
            compact.append(line)
        else:
            empty += 1
            if empty <= 2:
                compact.append("")
    return "\n".join(compact).strip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", default=".", help="корень репозитория")
    parser.add_argument("--output", default="build/txt-docs", help="каталог для .txt")
    args = parser.parse_args()

    source = Path(args.source).resolve()
    output = Path(args.output).resolve()

    for source_name, output_name in DOCUMENTS:
        src = source / source_name
        if not src.is_file():
            print(f"пропуск: {source_name} не найден")
            continue
        text = markdown_to_text(src.read_text(encoding="utf-8"))
        dst = output / output_name
        dst.parent.mkdir(parents=True, exist_ok=True)
        # BOM помогает старым версиям Блокнота на Windows правильно определить UTF-8.
        dst.write_text(text, encoding="utf-8-sig")
        print(f"{source_name} -> {dst}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
