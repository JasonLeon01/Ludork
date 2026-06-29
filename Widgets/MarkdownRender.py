from __future__ import annotations

import builtins
import html
import io
import keyword
import os
import re
import sys
import token
import tokenize

_STYLE_CACHE: dict[str, str] = {}

_INLINE_CODE_STYLE = (
    "font-family:Consolas,monospace;"
    "background-color:#2d2d2d;"
    "color:#ce9178;"
    "padding:1px 4px;"
)

_DOCUMENT_BODY_STYLE = (
    "body{font-family:Segoe UI,Arial,sans-serif;font-size:14px;color:#dcdcdc;}"
    "h1,h2,h3,h4,h5,h6{color:#f0f0f0;}"
    "h1{font-size:24px;margin:10px 0;}h2{font-size:20px;margin:10px 0;}h3{font-size:18px;}"
    "pre{background:#1e1e1e;color:#dcdcdc;border:1px solid #444;padding:10px;margin:10px 0;}"
    "pre code{background:transparent;color:#dcdcdc;padding:0;}"
    "ul{padding-left:20px;}"
    "hr{border:0;border-top:1px solid #555;margin:16px 0;}"
    "p{line-height:1.6;}"
    "table.md-table{border-collapse:collapse;margin:10px 0;}"
    "table.md-table th,table.md-table td{border:1px solid #555;padding:6px 8px;}"
    "table.md-table th{background:#2d2d2d;color:#f0f0f0;font-weight:600;}"
    "table.md-table td{background:#1f1f1f;color:#dcdcdc;}"
    "a{color:#4aa3ff;text-decoration:none;}"
)

_COMPACT_BODY_STYLE = (
    "body{font-family:Segoe UI,Arial,sans-serif;font-size:13px;color:#e0e0e0;margin:0;padding:0;}"
    "h1,h2,h3,h4,h5,h6{color:#f0f0f0;margin:6px 0;}"
    "h1{font-size:18px;}h2{font-size:16px;}h3{font-size:15px;}"
    "pre{background:#252525;color:#dcdcdc;border:1px solid #444;padding:8px;margin:6px 0;border-radius:4px;}"
    "pre code{background:transparent;color:#dcdcdc;padding:0;}"
    "ul{margin:4px 0;padding-left:18px;}"
    "hr{border:0;border-top:1px solid #555;margin:10px 0;}"
    "p{margin:4px 0;line-height:1.5;}"
    "table.md-table{border-collapse:collapse;margin:6px 0;}"
    "table.md-table th,table.md-table td{border:1px solid #4a4a4a;padding:4px 6px;}"
    "table.md-table th{background:#333;color:#f0f0f0;font-weight:600;}"
    "table.md-table td{background:#252525;color:#e0e0e0;}"
    "a{color:#7ec8ff;text-decoration:none;}"
)


def _isPacked() -> bool:
    return hasattr(builtins, "__compiled__") or hasattr(builtins, "__nuitka_binary_dir")


def _loadStyleSheet(fileName: str, fallback: str) -> str:
    cached = _STYLE_CACHE.get(fileName)
    if cached is not None:
        return cached
    baseDir = os.path.dirname(sys.executable) if _isPacked() else os.getcwd()
    paths = [
        os.path.join(baseDir, "Styles", fileName),
        os.path.join("Styles", fileName),
        os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Styles", fileName),
    ]
    for path in paths:
        if os.path.isfile(path):
            try:
                with open(path, "r", encoding="utf-8") as f:
                    _STYLE_CACHE[fileName] = f.read()
                    return _STYLE_CACHE[fileName]
            except OSError:
                continue
    _STYLE_CACHE[fileName] = fallback
    return fallback


def MarkdownToHtml(
    text: str,
    *,
    collapsedHeadings: set[str] | None = None,
    collapsibleHeadings: bool = False,
    compact: bool = False,
) -> str:
    r"""\brief Convert markdown text to a complete HTML document.

    - \param text - Markdown source text.
    - \param collapsedHeadings - Heading ids currently collapsed when collapsibleHeadings is enabled.
    - \param collapsibleHeadings - Whether headings can be toggled via ludork-collapse links.
    - \param compact - Use tighter spacing suitable for inline chat bubbles.
    - \return HTML document string.
    """
    if compact:
        bodyStyle = _loadStyleSheet("markdownCompact.qss", _COMPACT_BODY_STYLE)
    else:
        bodyStyle = _loadStyleSheet("markdownDocument.qss", _DOCUMENT_BODY_STYLE)
    body = _renderMarkdownBody(
        text,
        collapsedHeadings=collapsedHeadings or set(),
        collapsibleHeadings=collapsibleHeadings,
    )
    return f"<html><head><style>{bodyStyle}</style></head><body>{body}</body></html>"


def _renderMarkdownBody(
    text: str,
    *,
    collapsedHeadings: set[str],
    collapsibleHeadings: bool,
) -> str:
    lines = text.splitlines()
    out: list[str] = []
    inCode = False
    codeLang = ""
    codeLines: list[str] = []
    inList = False
    headingIndex = 0
    hiddenLevels: list[int] = []

    def flushList() -> None:
        nonlocal inList
        if inList:
            out.append("</ul>")
            inList = False

    lineIndex = 0
    while lineIndex < len(lines):
        ln = lines[lineIndex]
        if ln.strip().startswith("```"):
            if hiddenLevels:
                inCode = not inCode
                lineIndex += 1
                continue
            if not inCode:
                flushList()
                codeLang = ln.strip()[3:].strip()
                codeLines = []
                inCode = True
            else:
                out.append(_codeBlockHtml(codeLang, "\n".join(codeLines)))
                codeLang = ""
                codeLines = []
                inCode = False
            lineIndex += 1
            continue
        if inCode:
            if hiddenLevels:
                lineIndex += 1
                continue
            codeLines.append(ln)
            lineIndex += 1
            continue
        headingMatch = re.match(r"^ {0,3}(#{1,6})\s+(.*)$", ln)
        if headingMatch:
            level = len(headingMatch.group(1))
            hiddenLevels = [hiddenLevel for hiddenLevel in hiddenLevels if hiddenLevel < level]
            headingId = f"h{headingIndex}"
            headingIndex += 1
            if hiddenLevels:
                lineIndex += 1
                continue
            flushList()
            content = _inlineMarkup(headingMatch.group(2))
            if collapsibleHeadings:
                collapsed = headingId in collapsedHeadings
                marker = "&#9654;" if collapsed else "&#9660;"
                if collapsed:
                    hiddenLevels.append(level)
                out.append(
                    f'<h{level} style="color:#ffffff;"><a style="color:#ffffff;text-decoration:none;" '
                    f'href="ludork-collapse:{headingId}">'
                    f'<span style="color:#ffffff;font-family:Consolas,monospace;">{marker}</span> '
                    f'<span style="color:#ffffff;">{content}</span>'
                    f"</a></h{level}>"
                )
            else:
                out.append(f"<h{level}>{content}</h{level}>")
            lineIndex += 1
            continue
        if hiddenLevels:
            lineIndex += 1
            continue
        if lineIndex + 1 < len(lines) and _isTableStart(ln, lines[lineIndex + 1]):
            flushList()
            headerLine = ln
            separatorLine = lines[lineIndex + 1]
            rowLines: list[str] = []
            lineIndex += 2
            while lineIndex < len(lines) and _isTableDataLine(lines[lineIndex]):
                rowLines.append(lines[lineIndex])
                lineIndex += 1
            out.append(_tableHtml(headerLine, separatorLine, rowLines))
            continue
        if _isThematicBreak(ln):
            flushList()
            out.append("<hr>")
            lineIndex += 1
            continue
        listMatch = re.match(r"^\s*[-*]\s+(.*)$", ln)
        if listMatch:
            if not inList:
                out.append("<ul>")
                inList = True
            content = _inlineMarkup(listMatch.group(1))
            out.append(f"<li>{content}</li>")
            lineIndex += 1
            continue
        if not ln.strip():
            flushList()
            out.append("")
            lineIndex += 1
            continue
        flushList()
        out.append(f"<p>{_inlineMarkup(ln)}</p>")
        lineIndex += 1
    if inCode:
        out.append(_codeBlockHtml(codeLang, "\n".join(codeLines)))
    flushList()
    return "\n".join(out)


def _isThematicBreak(line: str) -> bool:
    compact = re.sub(r"[ \t]", "", line.strip())
    return len(compact) >= 3 and compact[0] in "-_*" and set(compact) == {compact[0]}


def _isTableStart(headerLine: str, separatorLine: str) -> bool:
    headerCells = _splitTableRow(headerLine)
    separatorCells = _splitTableRow(separatorLine)
    return (
        len(headerCells) >= 2
        and len(headerCells) == len(separatorCells)
        and all(_isTableSeparatorCell(cell) for cell in separatorCells)
    )


def _isTableDataLine(line: str) -> bool:
    return "|" in line and bool(line.strip())


def _splitTableRow(line: str) -> list[str]:
    if "|" not in line:
        return []
    stripped = line.strip()
    if stripped.startswith("|"):
        stripped = stripped[1:]
    if stripped.endswith("|"):
        stripped = stripped[:-1]
    return [cell.strip() for cell in stripped.split("|")]


def _isTableSeparatorCell(cell: str) -> bool:
    return re.match(r"^:?-{3,}:?$", cell.strip()) is not None


def _tableHtml(headerLine: str, separatorLine: str, rowLines: list[str]) -> str:
    headerCells = _splitTableRow(headerLine)
    alignments = [_tableCellAlignment(cell) for cell in _splitTableRow(separatorLine)]
    rows = [_normaliseTableRow(_splitTableRow(row), len(headerCells)) for row in rowLines]
    htmlLines = ['<table class="md-table">', "<thead><tr>"]
    for index, cell in enumerate(headerCells):
        htmlLines.append(f'<th style="{_tableCellStyle(alignments[index])}">{_inlineMarkup(cell)}</th>')
    htmlLines.append("</tr></thead>")
    if rows:
        htmlLines.append("<tbody>")
        for row in rows:
            htmlLines.append("<tr>")
            for index, cell in enumerate(row):
                htmlLines.append(f'<td style="{_tableCellStyle(alignments[index])}">{_inlineMarkup(cell)}</td>')
            htmlLines.append("</tr>")
        htmlLines.append("</tbody>")
    htmlLines.append("</table>")
    return "".join(htmlLines)


def _tableCellAlignment(separatorCell: str) -> str:
    stripped = separatorCell.strip()
    if stripped.startswith(":") and stripped.endswith(":"):
        return "center"
    if stripped.endswith(":"):
        return "right"
    return "left"


def _normaliseTableRow(cells: list[str], columnCount: int) -> list[str]:
    if len(cells) < columnCount:
        return cells + [""] * (columnCount - len(cells))
    return cells[:columnCount]


def _tableCellStyle(alignment: str) -> str:
    return f"text-align:{alignment};"


def _codeBlockHtml(lang: str, code: str) -> str:
    content = _highlightCode(lang, code)
    return f"<pre><code>{content}</code></pre>"


def _highlightCode(lang: str, code: str) -> str:
    if _isPythonLang(lang):
        return _highlightPythonCode(code)
    return html.escape(code)


def _isPythonLang(lang: str) -> bool:
    normalized = lang.strip().lower()
    if normalized.startswith("{") and normalized.endswith("}"):
        normalized = normalized[1:-1].strip()
    if normalized:
        normalized = normalized.split()[0]
    return normalized in {"py", "python", "python3"}


def _highlightPythonCode(code: str) -> str:
    lineOffsets = [0]
    for line in code.splitlines(True):
        lineOffsets.append(lineOffsets[-1] + len(line))

    def toOffset(pos: tuple[int, int]) -> int:
        line, col = pos
        if line <= 0:
            return 0
        if line - 1 >= len(lineOffsets):
            return len(code)
        return min(lineOffsets[line - 1] + col, len(code))

    result: list[str] = []
    cursor = 0
    try:
        for tok in tokenize.generate_tokens(io.StringIO(code).readline):
            tokType, tokText, start, end, _ = tok
            if tokType == token.ENDMARKER:
                continue
            startOffset = toOffset(start)
            endOffset = toOffset(end)
            if startOffset > cursor:
                result.append(html.escape(code[cursor:startOffset]))
            rendered = html.escape(tokText)
            style = _pythonTokenStyle(tokType, tokText)
            if style:
                rendered = f'<span style="{style}">{rendered}</span>'
            result.append(rendered)
            cursor = max(cursor, endOffset)
    except (tokenize.TokenError, IndentationError):
        return html.escape(code)
    if cursor < len(code):
        result.append(html.escape(code[cursor:]))
    return "".join(result)


def _pythonTokenStyle(tokType: int, tokText: str) -> str:
    if tokType == token.NAME:
        isSoftKeyword = getattr(keyword, "issoftkeyword", lambda value: False)
        if keyword.iskeyword(tokText) or isSoftKeyword(tokText):
            return "color:#c586c0;font-weight:600;"
        if tokText in dir(builtins):
            return "color:#4ec9b0;"
    if tokType == token.STRING:
        return "color:#ce9178;"
    if tokType == token.NUMBER:
        return "color:#b5cea8;"
    if tokType == tokenize.COMMENT:
        return "color:#6a9955;"
    if tokType == token.OP:
        return "color:#d4d4d4;"
    return ""


def _inlineMarkup(text: str) -> str:
    text = html.escape(text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*(.+?)\*(?!\*)", r"<em>\1</em>", text)
    text = re.sub(r"`(.+?)`", rf'<code style="{_INLINE_CODE_STYLE}">\1</code>', text)
    text = re.sub(r"\[(.+?)\]\((.+?)\)", r'<a href="\2">\1</a>', text)
    return text
