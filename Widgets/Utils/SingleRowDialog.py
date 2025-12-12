from PyQt5 import QtWidgets
from Utils import Locale
import EditorStatus


class SingleRowDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget, title: str, message: str, initial_text: str = "") -> None:
        super().__init__(parent)
        self.setWindowTitle(title)
        layout = QtWidgets.QFormLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)
        self.setStyleSheet(
            "QDialog { background-color: #2b2b2b; }"
            "QLabel { color: white; }"
            "QLineEdit { color: white; background-color: #3a3a3a; }"
            "QDialogButtonBox QPushButton { color: white; }"
        )
        self.input = QtWidgets.QLineEdit(self)
        self.input.setText(initial_text)
        layout.addRow(message, self.input)
        self.btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        layout.addRow(self.btns)
        confirm_label = Locale.getContent("CONFIRM")
        cancel_label = Locale.getContent("CANCEL")
        ok_btn = self.btns.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = self.btns.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(confirm_label)
        if cancel_btn:
            cancel_btn.setText(cancel_label)
        self.btns.accepted.connect(self.accept)
        self.btns.rejected.connect(self.reject)

    def execGetText(self):
        if self.exec_() != QtWidgets.QDialog.Accepted:
            return False, ""
        return True, self.input.text()
