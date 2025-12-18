from PyQt5 import QtWidgets, QtGui
from Utils import Locale, System


class SingleRowDialog(QtWidgets.QDialog):
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        title: str,
        message: str,
        initial_text: str = "",
        input_mode: str = None,
        min_value: float = None,
        max_value: float = None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle(title)
        layout = QtWidgets.QFormLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)
        System.setStyle(self, "singleRow.qss")
        self.input = QtWidgets.QLineEdit(self)
        if isinstance(initial_text, str):
            self.input.setText(initial_text)
        if input_mode == "int":
            validator = QtGui.QIntValidator(self)
            if min_value is not None:
                validator.setBottom(int(min_value))
            if max_value is not None:
                validator.setTop(int(max_value))
            self.input.setValidator(validator)
        elif input_mode in ("float", "number"):
            validator = QtGui.QDoubleValidator(self)
            if min_value is not None and max_value is not None:
                validator.setRange(float(min_value), float(max_value))
            validator.setNotation(QtGui.QDoubleValidator.StandardNotation)
            validator.setDecimals(6)
            self.input.setValidator(validator)
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
