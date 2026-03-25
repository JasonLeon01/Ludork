# -*- encoding: utf-8 -*-

import os
import sys
import runpy
import traceback
import pysf


def _handleUnexpectedException(exc_type, exc_value, exc_tb):
    from PyQt5 import QtCore, QtWidgets

    traceback.print_exception(exc_type, exc_value, exc_tb, file=sys.stderr)
    try:
        sys.stderr.flush()
    except Exception:
        pass
    QtWidgets.QMessageBox.critical(
        None,
        ELOC("ERROR"),
        f"{ELOC('UNEXPECTED_ERROR')}\n\n{''.join(traceback.format_exception(exc_type, exc_value, exc_tb))}",
    )
    sys.exit(1)


if __name__ == "__main__":
    try:
        params = sys.argv.copy()
        if not os.environ.get("WINDOWHANDLE", None) is None:
            sys.argv = sys.argv[1:]
            sys.argv[0] = os.path.abspath(params[1])
            if not os.getcwd() in sys.path:
                sys.path.append(os.getcwd())
            runpy.run_path(sys.argv[0], run_name="__main__")
        else:
            import LoadEditor

            if len(params) > 1:
                arg1 = params[1]
                if isinstance(arg1, str) and arg1.lower().endswith(".proj") and os.path.isfile(arg1):
                    LoadEditor.START_PROJ_FILE = os.path.abspath(arg1)
            LoadEditor.main()
    except Exception:
        _handleUnexpectedException(*sys.exc_info())
