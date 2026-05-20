"""Entry point for `python -m sigint_gui`."""

import sys
from PyQt5.QtWidgets import QApplication
from sigint_gui.views.main_window import MainWindow


def main() -> None:
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()