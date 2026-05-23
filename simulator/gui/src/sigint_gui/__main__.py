
"""Entry point for `python -m sigint_gui`."""
import os
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))

from PyQt5.QtWidgets import QApplication

from sigint_gui.views.main_window import MainWindow, apply_dark_theme
from sigint_gui import _sigint_sim_core as _core


def main() -> None:
    output_dir = Path.cwd() / "output"
    output_dir.mkdir(exist_ok=True)
    _core.init_logger(str(output_dir / "sim_run.log"))
    
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
    
    output_dir = Path.cwd() / "output"
    output_dir.mkdir(exist_ok=True)
    _core.init_logger(str(output_dir / "sim_run.log"))


if __name__ == "__main__":
    main()