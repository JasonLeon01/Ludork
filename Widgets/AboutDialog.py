# -*- encoding: utf-8 -*-

import os
from typing import Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal import EditorStatus
from EditorGlobal.QmlDialogHost import QmlDialogHost
from Utils import File


class AboutDialog(QmlDialogHost):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(
            parent,
            ELOC("ABOUT_TITLE"),
            QtCore.QSize(500, 360),
            QtCore.QSize(400, 280),
        )
        self._licenseWindow: Optional[QtWidgets.QWidget] = None
        self.loadQml(
            "Dialogs/AboutDialog.qml",
            {
                "aboutAppName": EditorStatus.APP_NAME,
                "aboutVersion": f"Version {EditorStatus.VERSION}",
            },
        )

    @QtCore.pyqtSlot()
    def openLicenses(self) -> None:
        from Widgets.MarkdownPreviewer import MarkdownPreviewer
        self._licenseWindow = MarkdownPreviewer(
            self,
            os.path.join(File.GetRootPath(), "LICENSE.md"),
            ELOC("ABOUT_LICENSES"),
        )
        self._licenseWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._licenseWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._licenseWindow.show()
