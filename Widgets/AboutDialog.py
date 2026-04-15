# -*- encoding: utf-8 -*-

from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import File
from .MarkdownPreviewer import MarkdownPreviewer
from EditorGlobal import EditorStatus


class AboutDialog(QtWidgets.QDialog):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.setWindowTitle(ELOC("ABOUT_TITLE"))
        self.setFixedSize(500, 400)
        self.setWindowFlags(self.windowFlags() & ~QtCore.Qt.WindowContextHelpButtonHint)
        self._licenseWindow: Optional[MarkdownPreviewer] = None

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(15)

        titleLabel = QtWidgets.QLabel(EditorStatus.APP_NAME)
        font = QtGui.QFont()
        font.setPointSize(24)
        font.setBold(True)
        titleLabel.setFont(font)
        titleLabel.setAlignment(QtCore.Qt.AlignCenter)
        layout.addWidget(titleLabel)

        versionLabel = QtWidgets.QLabel(f"Version {EditorStatus.VERSION}")
        versionLabel.setAlignment(QtCore.Qt.AlignCenter)
        layout.addWidget(versionLabel)

        descLabel = QtWidgets.QLabel(ELOC("ABOUT_DESC"))
        descLabel.setWordWrap(True)
        descLabel.setAlignment(QtCore.Qt.AlignCenter)
        layout.addWidget(descLabel)

        copyrightLabel = QtWidgets.QLabel(ELOC("ABOUT_COPYRIGHT"))
        copyrightLabel.setAlignment(QtCore.Qt.AlignCenter)
        copyrightLabel.setStyleSheet("color: gray;")
        layout.addWidget(copyrightLabel)

        layout.addStretch()

        btnBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Close)
        licensesBtn = btnBox.addButton(ELOC("ABOUT_LICENSES"), QtWidgets.QDialogButtonBox.ActionRole)
        licensesBtn.clicked.connect(self._onOpenLicenses)
        btnBox.rejected.connect(self.reject)
        layout.addWidget(btnBox)

    def _onOpenLicenses(self) -> None:
        self._licenseWindow = MarkdownPreviewer(self, File.getRootPath())
        self._licenseWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._licenseWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._licenseWindow.show()
