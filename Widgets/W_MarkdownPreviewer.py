import os
import re
import html
from PyQt5 import QtCore, QtWidgets
from PyQt5.QtWidgets import QWidget
from Utils import File


class MarkdownPreviewer(QWidget):
    def __init__(self, parent=None, filePath: str = ""):
        super().__init__(parent=parent)
        self.resize(1080, 600)

        self.setWindowFlags(QtCore.Qt.Window)
        self._dir = os.path.abspath(filePath) if filePath else ""
        self._list = QtWidgets.QListWidget(self)
        self._list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._preview = QtWidgets.QTextBrowser(self)
        self._preview.setOpenExternalLinks(True)
        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal, self)
        splitter.addWidget(self._list)
        splitter.addWidget(self._preview)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setChildrenCollapsible(False)
        splitter.setCollapsible(0, False)
        splitter.setCollapsible(1, False)
        splitter.setSizes([max(160, int(self.width() * 0.3)), max(300, int(self.width() * 0.7))])
        self._list.setMinimumWidth(160)
        lay = QtWidgets.QHBoxLayout(self)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(8)
        lay.addWidget(splitter, 1)
        self._list.itemSelectionChanged.connect(self._onSelectionChanged)
        self._list.currentRowChanged.connect(self._onCurrentRowChanged)
        self._populate()

    def set_text(self, mdContent):
        self._render(mdContent)

    def _populate(self):
        files = []
        if self._dir and os.path.isdir(self._dir):
            for n in os.listdir(self._dir):
                if n.lower().endswith(".md"):
                    files.append(n)
        files.sort()
        self._list.clear()
        for n in files:
            base, _ = os.path.splitext(n)
            item = QtWidgets.QListWidgetItem(base)
            item.setData(QtCore.Qt.UserRole, n)
            self._list.addItem(item)
        if files:
            self._list.setCurrentRow(0)
            self._loadFile(files[0])
        else:
            self._preview.setPlainText("No markdown files")

    def _onSelectionChanged(self):
        items = self._list.selectedItems()
        if not items:
            return
        item = items[0]
        name = item.data(QtCore.Qt.UserRole) or item.text()
        self._loadFile(name)

    def _onCurrentRowChanged(self, row: int):
        if row < 0:
            return
        it = self._list.item(row)
        if not it:
            return
        name = it.data(QtCore.Qt.UserRole) or it.text()
        self._loadFile(name)

    def _loadFile(self, name: str):
        p = os.path.join(self._dir, name)
        try:
            with open(p, "r", encoding="utf-8") as f:
                text = f.read()
        except Exception as e:
            self._preview.setPlainText(str(e))
            return
        self._render(text)

    def _render(self, text: str):
        doc = self._preview.document()
        if hasattr(doc, "setBaseUrl"):
            try:
                doc.setBaseUrl(QtCore.QUrl.fromLocalFile(self._dir))
            except Exception:
                pass
        setMdWidget = getattr(self._preview, "setMarkdown", None)
        if callable(setMdWidget):
            setMdWidget(text)
            return
        setMdDoc = getattr(doc, "setMarkdown", None)
        if callable(setMdDoc):
            setMdDoc(text)
            return
        self._preview.setHtml(self._md2html(text))

    def _md2html(self, text: str) -> str:
        lines = text.splitlines()
        out = []
        in_code = False
        code_lang = ""
        in_list = False

        def flush_list():
            nonlocal in_list
            if in_list:
                out.append("</ul>")
                in_list = False

        for ln in lines:
            if ln.strip().startswith("```"):
                if not in_code:
                    code_lang = ln.strip()[3:].strip()
                    out.append("<pre><code>")
                    in_code = True
                else:
                    out.append("</code></pre>")
                    in_code = False
                continue
            if in_code:
                out.append(html.escape(ln))
                continue
            m = re.match(r"^(#{1,6})\s+(.*)$", ln)
            if m:
                flush_list()
                level = len(m.group(1))
                content = self._inline_markup(m.group(2))
                out.append(f"<h{level}>{content}</h{level}>")
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
        flush_list()
        style = (
            "<style>"
            "body{font-family:Segoe UI,Arial,sans-serif;font-size:14px;}"
            "h1{font-size:24px;margin:10px 0;}h2{font-size:20px;margin:10px 0;}h3{font-size:18px;}"
            "pre{background:#1e1e1e;color:#dcdcdc;padding:10px;border-radius:6px;overflow:auto;}"
            "code{font-family:Consolas,monospace;}"
            "ul{padding-left:20px;}"
            "p{line-height:1.6;}"
            "a{color:#4aa3ff;text-decoration:none;}"
            "</style>"
        )
        return "<html><head>" + style + "</head><body>" + "\n".join(out) + "</body></html>"

    def _inline_markup(self, s: str) -> str:
        s = html.escape(s)
        s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)
        s = re.sub(r"(?<!\*)\*(.+?)\*(?!\*)", r"<em>\1</em>", s)
        s = re.sub(r"`(.+?)`", r"<code>\1</code>", s)
        s = re.sub(r"\[(.+?)\]\((.+?)\)", r'<a href="\2">\1</a>', s)
        return s
