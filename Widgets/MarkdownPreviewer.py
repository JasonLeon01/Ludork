from __future__ import annotations

import builtins
import html
import io
import keyword
import os
import re
import token
import tokenize
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtWidgets import QWidget
from Utils import File


class MarkdownPreviewer(QWidget):
    def __init__(self, parent: QtWidgets.QWidget | None = None, filePath: str = "") -> None:
        super().__init__(parent=parent)
        self.resize(1080, 600)

        self.setWindowFlags(QtCore.Qt.Window)
        self._dir = os.path.abspath(filePath) if filePath else ""
        self._currentText = ""
        self._currentBaseDir = ""
        self._collapsedHeadings: set[str] = set()
        self._list = QtWidgets.QTreeWidget(self)
        self._list.setHeaderHidden(True)
        self._list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._preview = QtWidgets.QTextBrowser(self)
        self._preview.setOpenExternalLinks(True)
        self._preview.setOpenLinks(False)
        previewFont = self._preview.font()
        previewFont.setPointSize(max(previewFont.pointSize(), 9))
        self._preview.setFont(previewFont)
        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal, self)
        splitter.addWidget(self._list)
        splitter.addWidget(self._preview)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setChildrenCollapsible(False)
        splitter.setCollapsible(0, False)
        splitter.setCollapsible(1, False)
        splitter.setSizes([max(72, int(self.width() * 0.2)), max(300, int(self.width() * 0.8))])
        self._list.setMinimumWidth(72)
        lay = QtWidgets.QHBoxLayout(self)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(8)
        lay.addWidget(splitter, 1)
        self._list.currentItemChanged.connect(self._onCurrentItemChanged)
        self._preview.anchorClicked.connect(self._onAnchorClicked)
        self._populate()

    def set_text(self, mdContent: str) -> None:
        self._render(mdContent)

    def _populate(self) -> None:
        entries = []
        if self._dir and os.path.isdir(self._dir):
            entries = self._collectEntries(self._dir)
        self._list.clear()
        firstItem = None
        parents: dict[int, QtWidgets.QTreeWidgetItem] = {}
        for entry in entries:
            text = entry["display"] + ("/" if entry["isDir"] else "")
            parent = parents.get(entry["depth"] - 1)
            item = QtWidgets.QTreeWidgetItem(parent or self._list, [text])
            item.setData(0, QtCore.Qt.UserRole, entry["path"])
            item.setData(0, QtCore.Qt.UserRole + 1, entry["isDir"])
            parents[entry["depth"]] = item
            for depth in list(parents):
                if depth > entry["depth"]:
                    parents.pop(depth, None)
            if firstItem is None:
                firstItem = item
        if entries:
            self._list.expandAll()
            if firstItem is not None:
                self._list.setCurrentItem(firstItem)
            self._loadEntry(entries[0]["path"], entries[0]["isDir"])
        else:
            self._preview.setPlainText("No markdown files")

    def _collectEntries(self, directory: str, relativeDir: str = "", depth: int = 0) -> list[dict]:
        entries = []
        try:
            names = os.listdir(directory)
        except OSError:
            return entries
        for name in sorted(names, key=self._docSortKey):
            path = os.path.join(directory, name)
            relativePath = os.path.join(relativeDir, name) if relativeDir else name
            if os.path.isdir(path):
                entries.append(
                    {
                        "display": name,
                        "path": relativePath,
                        "isDir": True,
                        "depth": depth,
                    }
                )
                entries.extend(self._collectEntries(path, relativePath, depth + 1))
            elif name.lower().endswith(".md"):
                base, _ = os.path.splitext(name)
                entries.append(
                    {
                        "display": base,
                        "path": relativePath,
                        "isDir": False,
                        "depth": depth,
                    }
                )
        return entries

    def _docSortKey(self, name: str) -> tuple[int, int, str]:
        base, ext = os.path.splitext(name)
        target = base if ext.lower() == ".md" else name
        match = re.match(r"^(\d+)[\.\s_-]*(.*)$", target)
        if match:
            return (0, int(match.group(1)), match.group(2).lower())
        return (1, 0, target.lower())

    def _onCurrentItemChanged(
        self,
        current: QtWidgets.QTreeWidgetItem | None,
        previous: QtWidgets.QTreeWidgetItem | None,
    ) -> None:
        if not current:
            return
        name = current.data(0, QtCore.Qt.UserRole) or current.text(0).strip()
        isDir = bool(current.data(0, QtCore.Qt.UserRole + 1))
        self._loadEntry(name, isDir)

    def _loadEntry(self, name: str, isDir: bool) -> None:
        if isDir:
            self._renderDirectory(name)
            return
        self._loadFile(name)

    def _loadFile(self, name: str) -> None:
        p = os.path.join(self._dir, name)
        try:
            with open(p, "r", encoding="utf-8") as f:
                text = f.read()
        except Exception as e:
            self._preview.setPlainText(str(e))
            return
        self._render(text, os.path.dirname(p))

    def _renderDirectory(self, name: str) -> None:
        p = os.path.join(self._dir, name)
        title = os.path.basename(name.rstrip(os.sep))
        lines = [f"# {title}", "", "## Contents"]
        children = []
        try:
            children = os.listdir(p)
        except OSError as e:
            self._preview.setPlainText(str(e))
            return
        for child in sorted(children, key=self._docSortKey):
            childPath = os.path.join(p, child)
            if os.path.isdir(childPath):
                lines.append(f"- {child}/")
            elif child.lower().endswith(".md"):
                childBase, _ = os.path.splitext(child)
                lines.append(f"- {childBase}")
        self._render("\n".join(lines), p)

    def _render(self, text: str, baseDir: str | None = None, keepCollapse: bool = False) -> None:
        doc = self._preview.document()
        if not doc:
            return
        self._currentText = text
        self._currentBaseDir = baseDir or self._dir
        if not keepCollapse:
            self._collapsedHeadings.clear()
        if isinstance(doc, QtGui.QTextDocument):
            doc.setBaseUrl(QtCore.QUrl.fromLocalFile(self._currentBaseDir))
        self._preview.setHtml(self._md2html(text))

    def _onAnchorClicked(self, url: QtCore.QUrl) -> None:
        if url.scheme() == "ludork-collapse":
            headingId = url.path().lstrip("/")
            if not headingId:
                headingId = url.toString().split(":", 1)[-1]
            if headingId:
                self._toggleHeading(headingId)
            return
        target = url
        if target.isRelative() and self._currentBaseDir:
            baseUrl = QtCore.QUrl.fromLocalFile(os.path.join(self._currentBaseDir, ""))
            target = baseUrl.resolved(target)
        QtGui.QDesktopServices.openUrl(target)

    def _toggleHeading(self, headingId: str) -> None:
        if headingId in self._collapsedHeadings:
            self._collapsedHeadings.remove(headingId)
        else:
            self._collapsedHeadings.add(headingId)
        self._render(self._currentText, self._currentBaseDir, True)

    def _md2html(self, text: str) -> str:
        lines = text.splitlines()
        out = []
        in_code = False
        code_lang = ""
        code_lines: list[str] = []
        in_list = False
        headingIndex = 0
        hiddenLevels: list[int] = []

        def flush_list() -> None:
            nonlocal in_list
            if in_list:
                out.append("</ul>")
                in_list = False

        for ln in lines:
            if ln.strip().startswith("```"):
                if hiddenLevels:
                    in_code = not in_code
                    continue
                if not in_code:
                    flush_list()
                    code_lang = ln.strip()[3:].strip()
                    code_lines = []
                    in_code = True
                else:
                    out.append(self._codeBlockHtml(code_lang, "\n".join(code_lines)))
                    code_lang = ""
                    code_lines = []
                    in_code = False
                continue
            if in_code:
                if hiddenLevels:
                    continue
                code_lines.append(ln)
                continue
            m = re.match(r"^ {0,3}(#{1,6})\s+(.*)$", ln)
            if m:
                level = len(m.group(1))
                hiddenLevels = [hiddenLevel for hiddenLevel in hiddenLevels if hiddenLevel < level]
                headingId = f"h{headingIndex}"
                headingIndex += 1
                if hiddenLevels:
                    continue
                flush_list()
                content = self._inline_markup(m.group(2))
                collapsed = headingId in self._collapsedHeadings
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
                continue
            if hiddenLevels:
                continue
            m = re.match(r"^\s*[-*]\s+(.*)$", ln)
            if m:
                if not in_list:
                    out.append("<ul>")
                    in_list = True
                content = self._inline_markup(m.group(1))
                out.append(f"<li>{content}</li>")
                continue
            if not ln.strip():
                flush_list()
                out.append("")
                continue
            flush_list()
            out.append(f"<p>{self._inline_markup(ln)}</p>")
        if in_code:
            out.append(self._codeBlockHtml(code_lang, "\n".join(code_lines)))
        flush_list()
        style = (
            "<style>"
            "body{font-family:Segoe UI,Arial,sans-serif;font-size:14px;color:#dcdcdc;}"
            "h1,h2,h3,h4,h5,h6{color:#f0f0f0;}"
            "h1{font-size:24px;margin:10px 0;}h2{font-size:20px;margin:10px 0;}h3{font-size:18px;}"
            "pre{background:#1e1e1e;color:#dcdcdc;border:1px solid #444;padding:10px;margin:10px 0;}"
            "code{font-family:Consolas,monospace;}"
            "ul{padding-left:20px;}"
            "p{line-height:1.6;}"
            "a{color:#4aa3ff;text-decoration:none;}"
            "</style>"
        )
        return "<html><head>" + style + "</head><body>" + "\n".join(out) + "</body></html>"

    def _codeBlockHtml(self, lang: str, code: str) -> str:
        content = self._highlightCode(lang, code)
        return f'<pre><code>{content}</code></pre>'

    def _highlightCode(self, lang: str, code: str) -> str:
        if self._isPythonLang(lang):
            return self._highlightPythonCode(code)
        return html.escape(code)

    def _isPythonLang(self, lang: str) -> bool:
        normalized = lang.strip().lower()
        if normalized.startswith("{") and normalized.endswith("}"):
            normalized = normalized[1:-1].strip()
        if normalized:
            normalized = normalized.split()[0]
        return normalized in {"py", "python", "python3"}

    def _highlightPythonCode(self, code: str) -> str:
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

        result = []
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
                style = self._pythonTokenStyle(tokType, tokText)
                if style:
                    rendered = f'<span style="{style}">{rendered}</span>'
                result.append(rendered)
                cursor = max(cursor, endOffset)
        except (tokenize.TokenError, IndentationError):
            return html.escape(code)
        if cursor < len(code):
            result.append(html.escape(code[cursor:]))
        return "".join(result)

    def _pythonTokenStyle(self, tokType: int, tokText: str) -> str:
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

    def _inline_markup(self, s: str) -> str:
        s = html.escape(s)
        s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)
        s = re.sub(r"(?<!\*)\*(.+?)\*(?!\*)", r"<em>\1</em>", s)
        s = re.sub(r"`(.+?)`", r"<code>\1</code>", s)
        s = re.sub(r"\[(.+?)\]\((.+?)\)", r'<a href="\2">\1</a>', s)
        return s
