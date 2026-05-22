
"""Entry point for `python -m sigint_gui`."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))

from PyQt5.QtWidgets import QApplication

from sigint_gui.views.main_window import MainWindow, apply_dark_theme


def main() -> None:
    app = QApplication(sys.argv)
    app.setStyleSheet("""
    QToolTip {
        color: #ffffff;
        background-color: #2a2a2a;
        border: 1px solid #555555;
        padding: 4px;
        font-size: 12px;
    }
""")
    apply_dark_theme(app)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()