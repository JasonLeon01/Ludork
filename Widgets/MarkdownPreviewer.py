from __future__ import annotations

import os
import re
from typing import cast
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtWidgets import QWidget
from .MarkdownRender import MarkdownToHtml
from .SearchLineEdit import AddSearchIcon


class MarkdownPreviewer(QWidget):
    def __init__(self, parent: QtWidgets.QWidget | None = None, filePath: str = "", title: str = "") -> None:
        super().__init__(parent=parent)
        self.resize(1080, 600)

        self.setWindowFlags(QtCore.Qt.Window)
        if title:
            self.setWindowTitle(title)
        filePath = os.path.abspath(filePath) if filePath else ""
        if filePath and os.path.isfile(filePath):
            self._dir = os.path.dirname(filePath)
            self._initialEntry = os.path.basename(filePath)
            self._singleFile = True
        else:
            self._dir = filePath
            self._initialEntry = ""
            self._singleFile = False
        self._currentText = ""
        self._currentBaseDir = ""
        self._collapsedHeadings: set[str] = set()
        self._searchMatches: list[QtGui.QTextCursor] = []
        self._entrySearchCache: dict[str, str] = {}
        self._suppressSelectionLoad = False
        self._shortcuts: list[QtWidgets.QShortcut] = []
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
        lay = QtWidgets.QVBoxLayout(self)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(8)
        lay.addWidget(self._createToolbar())
        lay.addWidget(splitter, 1)
        self._list.currentItemChanged.connect(self._onCurrentItemChanged)
        self._preview.anchorClicked.connect(self._onAnchorClicked)
        self._initShortcuts()
        self._populate()

    def setText(self, mdContent: str) -> None:
        self._render(mdContent)

    def _createToolbar(self) -> QtWidgets.QWidget:
        bar = QtWidgets.QWidget(self)
        lay = QtWidgets.QHBoxLayout(bar)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(4)

        self._search = QtWidgets.QLineEdit(bar)
        AddSearchIcon(self._search)
        self._search.setPlaceholderText(ELOC("SEARCH"))
        self._search.setClearButtonEnabled(True)
        self._search.setFixedWidth(220)
        self._search.textChanged.connect(self._refreshSearch)
        lay.addWidget(self._search)

        self._searchCountLabel = QtWidgets.QLabel("", bar)
        self._searchCountLabel.setMinimumWidth(52)
        self._searchCountLabel.setAlignment(QtCore.Qt.AlignCenter)
        lay.addWidget(self._searchCountLabel)

        lay.addStretch(1)

        self._updateSearchState()
        return bar

    def _initShortcuts(self) -> None:
        self._addShortcut(QtGui.QKeySequence.Find, self._focusSearch)

    def _addShortcut(self, sequence: QtGui.QKeySequence, slot) -> None:
        shortcut = QtWidgets.QShortcut(sequence, self)
        shortcut.activated.connect(slot)
        self._shortcuts.append(shortcut)

    def _populate(self) -> None:
        entries = []
        if self._dir and self._singleFile:
            base, _ = os.path.splitext(self._initialEntry)
            entries = [
                {
                    "display": base,
                    "path": self._initialEntry,
                    "isDir": False,
                    "depth": 0,
                }
            ]
        elif self._dir and os.path.isdir(self._dir):
            entries = self._collectEntries(self._dir)
        self._list.clear()
        self._entrySearchCache.clear()
        firstItem = None
        initialItem = None
        initialEntry = None
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
            if self._initialEntry and entry["path"] == self._initialEntry:
                initialItem = item
                initialEntry = entry
        if entries:
            self._list.expandAll()
            selectedItem = initialItem or firstItem
            selectedEntry = initialEntry or entries[0]
            if selectedItem is not None:
                self._suppressSelectionLoad = True
                self._list.setCurrentItem(selectedItem)
                self._suppressSelectionLoad = False
            self._loadEntry(selectedEntry["path"], selectedEntry["isDir"])
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
        if not current or self._suppressSelectionLoad:
            return
        name = current.data(0, QtCore.Qt.UserRole) or current.text(0).strip()
        isDir = bool(current.data(0, QtCore.Qt.UserRole + 1))
        self._loadEntry(name, isDir)

    def _loadEntry(self, name: str, isDir: bool) -> None:
        if isDir:
            self._renderDirectory(name)
        else:
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
        doc = cast(QtGui.QTextDocument, doc)
        self._currentText = text
        self._currentBaseDir = baseDir or self._dir
        if not keepCollapse:
            self._collapsedHeadings.clear()
        doc.setBaseUrl(QtCore.QUrl.fromLocalFile(self._currentBaseDir))
        self._preview.setHtml(
            MarkdownToHtml(
                text,
                collapsedHeadings=self._collapsedHeadings,
                collapsibleHeadings=True,
            )
        )
        self._highlightSearchMatches()

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

    def _focusSearch(self) -> None:
        self._search.setFocus()
        self._search.selectAll()

    def _refreshSearch(self) -> None:
        if not hasattr(self, "_search"):
            return
        query = self._search.text().strip()
        visibleCount = self._filterEntries(query)
        current = self._list.currentItem()
        if current is None or current.isHidden():
            firstItem = self._firstVisibleItem()
            if firstItem is not None:
                self._suppressSelectionLoad = True
                self._list.setCurrentItem(firstItem)
                self._list.scrollToItem(firstItem)
                self._suppressSelectionLoad = False
                name = firstItem.data(0, QtCore.Qt.UserRole) or firstItem.text(0).strip()
                isDir = bool(firstItem.data(0, QtCore.Qt.UserRole + 1))
                self._loadEntry(name, isDir)
        self._highlightSearchMatches()
        self._updateSearchState(visibleCount)

    def _filterEntries(self, query: str) -> int:
        visibleCount = 0

        def filterItem(item: QtWidgets.QTreeWidgetItem) -> bool:
            nonlocal visibleCount
            childVisible = False
            for i in range(item.childCount()):
                childVisible = filterItem(item.child(i)) or childVisible
            itemVisible = not query or self._entryMatchesQuery(item, query)
            visible = itemVisible or childVisible
            item.setHidden(not visible)
            if query and itemVisible:
                visibleCount += 1
            if visible:
                if childVisible:
                    item.setExpanded(True)
            return visible

        for i in range(self._list.topLevelItemCount()):
            filterItem(self._list.topLevelItem(i))
        return visibleCount

    def _entryMatchesQuery(self, item: QtWidgets.QTreeWidgetItem, query: str) -> bool:
        if not query:
            return True
        normalized = query.casefold()
        if normalized in item.text(0).casefold():
            return True
        path = item.data(0, QtCore.Qt.UserRole)
        if not isinstance(path, str) or bool(item.data(0, QtCore.Qt.UserRole + 1)):
            return False
        return normalized in self._entrySearchText(path)

    def _entrySearchText(self, name: str) -> str:
        if name in self._entrySearchCache:
            return self._entrySearchCache[name]
        p = os.path.join(self._dir, name)
        try:
            with open(p, "r", encoding="utf-8") as f:
                text = f.read()
        except OSError:
            text = ""
        text = text.casefold()
        self._entrySearchCache[name] = text
        return text

    def _firstVisibleItem(self) -> QtWidgets.QTreeWidgetItem | None:
        def findInItem(item: QtWidgets.QTreeWidgetItem) -> QtWidgets.QTreeWidgetItem | None:
            isDir = bool(item.data(0, QtCore.Qt.UserRole + 1))
            if not item.isHidden() and not isDir:
                return item
            for i in range(item.childCount()):
                found = findInItem(item.child(i))
                if found is not None:
                    return found
            return None

        for i in range(self._list.topLevelItemCount()):
            found = findInItem(self._list.topLevelItem(i))
            if found is not None:
                return found

        def findAnyInItem(item: QtWidgets.QTreeWidgetItem) -> QtWidgets.QTreeWidgetItem | None:
            if not item.isHidden():
                return item
            for i in range(item.childCount()):
                found = findAnyInItem(item.child(i))
                if found is not None:
                    return found
            return None

        for i in range(self._list.topLevelItemCount()):
            found = findAnyInItem(self._list.topLevelItem(i))
            if found is not None:
                return found
        return None

    def _highlightSearchMatches(self) -> None:
        self._searchMatches.clear()
        query = self._search.text().strip()
        if query:
            doc = self._preview.document()
            if not doc:
                self._applySearchHighlights()
                return
            doc = cast(QtGui.QTextDocument, doc)
            cursor = QtGui.QTextCursor(doc)
            while True:
                cursor = doc.find(query, cursor)
                if cursor.isNull():
                    break
                self._searchMatches.append(QtGui.QTextCursor(cursor))
        self._applySearchHighlights()

    def _applySearchHighlights(self) -> None:
        selections = []
        for cursor in self._searchMatches:
            selection = QtWidgets.QTextEdit.ExtraSelection()
            selection.cursor = cursor
            fmt = QtGui.QTextCharFormat()
            fmt.setBackground(QtGui.QColor("#c9a227"))
            fmt.setForeground(QtGui.QColor("#111111"))
            selection.format = fmt
            selections.append(selection)
        self._preview.setExtraSelections(selections)

    def _updateSearchState(self, visibleCount: int | None = None) -> None:
        hasQuery = bool(self._search.text())
        self._searchCountLabel.setText(str(visibleCount) if hasQuery and visibleCount is not None else "")
