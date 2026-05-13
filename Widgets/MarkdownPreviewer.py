import os
import re
import html
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtWidgets import QWidget
from Utils import File


class MarkdownPreviewer(QWidget):
    def __init__(self, parent=None, filePath: str = ""):
        super().__init__(parent=parent)
        self.resize(1080, 600)

        self.setWindowFlags(QtCore.Qt.Window)
        self._dir = os.path.abspath(filePath) if filePath else ""
        self._headings = []
        self._list = QtWidgets.QListWidget(self)
        self._list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._toc = QtWidgets.QTreeWidget(self)
        self._toc.setHeaderHidden(True)
        self._toc.setIndentation(12)
        self._toc.setMinimumWidth(100)
        self._toc.setMaximumWidth(200)
        self._toc.setFocusPolicy(QtCore.Qt.NoFocus)
        self._toc.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        tocFont = self._toc.font()
        tocFont.setPointSize(max(tocFont.pointSize() - 2, 7))
        self._toc.setFont(tocFont)
        self._toc.setStyleSheet(
            "QTreeWidget { border: none; background: transparent; }"
            "QTreeWidget::item { padding: 2px 0px; }"
            "QTreeWidget::item:hover { background: rgba(255,255,255,30); }"
            "QTreeWidget::item:selected { background: rgba(74,163,255,80); color: #4aa3ff; }"
        )
        self._toc.itemClicked.connect(self._onTocClicked)
        self._preview = QtWidgets.QTextBrowser(self)
        self._preview.setOpenExternalLinks(True)
        previewFont = self._preview.font()
        previewFont.setPointSize(max(previewFont.pointSize() - 2, 7))
        self._preview.setFont(previewFont)
        rightWidget = QtWidgets.QWidget(self)
        rightLayout = QtWidgets.QHBoxLayout(rightWidget)
        rightLayout.setContentsMargins(0, 0, 0, 0)
        rightLayout.setSpacing(4)
        rightLayout.addWidget(self._toc, 0)
        rightLayout.addWidget(self._preview, 1)
        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal, self)
        splitter.addWidget(self._list)
        splitter.addWidget(rightWidget)
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
        self._headings = self._extractHeadings(text)
        self._buildToc()
        self._useCustomHtml = False
        doc = self._preview.document()
        if not doc:
            return
        if hasattr(doc, "setBaseUrl"):
            doc.setBaseUrl(QtCore.QUrl.fromLocalFile(self._dir))
        setMdWidget = getattr(self._preview, "setMarkdown", None)
        if callable(setMdWidget):
            setMdWidget(text)
            return
        setMdDoc = getattr(doc, "setMarkdown", None)
        if callable(setMdDoc):
            setMdDoc(text)
            return
        self._useCustomHtml = True
        self._preview.setHtml(self._md2html(text))

    def _extractHeadings(self, text: str):
        headings = []
        in_code = False
        for ln in text.splitlines():
            stripped = ln.strip()
            if stripped.startswith("```"):
                in_code = not in_code
                continue
            if in_code:
                continue
            m = re.match(r"^ {0,3}(#{1,6})\s+(.*)$", ln)
            if m:
                level = len(m.group(1))
                title = m.group(2).strip()
                title = re.sub(r"\*\*(.+?)\*\*", r"\1", title)
                title = re.sub(r"(?<!\*)\*(.+?)\*(?!\*)", r"\1", title)
                title = re.sub(r"`(.+?)`", r"\1", title)
                title = re.sub(r"\[(.+?)\]\(.+?\)", r"\1", title)
                headings.append((level, title))
        return headings

    def _buildToc(self):
        self._toc.clear()
        if not self._headings:
            self._toc.setVisible(False)
            return
        self._toc.setVisible(True)
        stack = []
        for idx, (level, title) in enumerate(self._headings):
            item = QtWidgets.QTreeWidgetItem()
            item.setText(0, title)
            item.setData(0, QtCore.Qt.UserRole, idx)
            item.setToolTip(0, title)
            while stack and stack[-1][0] >= level:
                stack.pop()
            if stack:
                stack[-1][1].addChild(item)
            else:
                self._toc.addTopLevelItem(item)
            stack.append((level, item))
        self._toc.expandAll()

    def _onTocClicked(self, item, column):
        idx = item.data(0, QtCore.Qt.UserRole)
        if idx is None or idx < 0 or idx >= len(self._headings):
            return
        if getattr(self, "_useCustomHtml", False):
            anchor = f"_toc_heading_{idx}"
            self._preview.scrollToAnchor(anchor)
            return
        _, title = self._headings[idx]
        doc = self._preview.document()
        if not doc:
            return
        found = 0
        block = doc.begin()
        while block.isValid():
            blockText = block.text().strip()
            if blockText == title:
                if found == idx - self._countPriorMatches(title, idx):
                    cursor = QtGui.QTextCursor(block)
                    cursor.movePosition(QtGui.QTextCursor.StartOfBlock)
                    self._preview.setTextCursor(cursor)
                    self._preview.ensureCursorVisible()
                    return
                found += 1
            block = block.next()
        self._scrollToHeadingByIndex(idx)

    def _countPriorMatches(self, title, idx):
        count = 0
        for i in range(idx):
            if self._headings[i][1] == title:
                count += 1
        return count

    def _scrollToHeadingByIndex(self, idx):
        doc = self._preview.document()
        if not doc:
            return
        headingCount = 0
        block = doc.begin()
        while block.isValid():
            fmt = block.blockFormat()
            if fmt.headingLevel() > 0:
                if headingCount == idx:
                    cursor = QtGui.QTextCursor(block)
                    cursor.movePosition(QtGui.QTextCursor.StartOfBlock)
                    self._preview.setTextCursor(cursor)
                    self._preview.ensureCursorVisible()
                    return
                headingCount += 1
            block = block.next()

    def _md2html(self, text: str) -> str:
        lines = text.splitlines()
        out = []
        in_code = False
        code_lang = ""
        in_list = False
        heading_idx = 0

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
            m = re.match(r"^ {0,3}(#{1,6})\s+(.*)$", ln)
            if m:
                flush_list()
                level = len(m.group(1))
                content = self._inline_markup(m.group(2))
                anchor = f"_toc_heading_{heading_idx}"
                out.append(f'<h{level}><a name="{anchor}"></a>{content}</h{level}>')
                heading_idx += 1
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
            "body{font-family:Segoe UI,Arial,sans-serif;font-size:12px;}"
            "h1{font-size:22px;margin:10px 0;}h2{font-size:18px;margin:10px 0;}h3{font-size:16px;}"
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
