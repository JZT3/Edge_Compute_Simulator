from PyQt5.QtWidgets import QWidget, QVBoxLayout, QCheckBox, QHBoxLayout
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from typing import Optional, List


class TimeSeriesChart(QWidget):
    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        
        # ---- Matplotlib setup ----
        self.figure = Figure(figsize=(8, 4))
        self.canvas = FigureCanvas(self.figure)
        self.ax = self.figure.add_subplot(111)
        self.ax2 = self.ax.twinx()

        # Four empty lines (data set later)
        (self.line_raw,)     = self.ax.plot([], [], 'g-',  label='Raw Intel')
        (self.line_net,)     = self.ax.plot([], [], 'b-',  label='Net Mission')
        (self.line_lpd,)     = self.ax.plot([], [], 'r-',  label='LPD Penalty')
        (self.line_success,) = self.ax2.plot([], [], 'k--', label='TX Success %')

        self.ax.set_xlabel('Time (s)')
        self.ax.set_ylabel('Intelligence')
        self.ax2.set_ylabel('Success Rate (%)')
        self.ax2.set_ylim(-5, 105)

        # Combine lines for legend & toggles
        self._all_lines = [self.line_raw, self.line_net, self.line_lpd, self.line_success]
        labels = [line.get_label() for line in self._all_lines]
        self.ax.legend(self._all_lines, labels, loc='upper left')

        # ---- Checkboxes (horizontal) ----
        check_layout = QHBoxLayout()
        for line in self._all_lines:
            cb = QCheckBox(line.get_label())
            cb.setChecked(True)
            # Use default argument to capture current line correctly
            cb.toggled.connect(lambda checked, l=line: self._on_toggle_line(l, checked))
            check_layout.addWidget(cb)

        # ---- Main layout ----
        main_layout = QVBoxLayout(self)
        main_layout.addWidget(self.canvas)
        main_layout.addLayout(check_layout)

    # ------------------------------------------------------------------
    # Toggle line visibility & redraw
    # ------------------------------------------------------------------
    def _on_toggle_line(self, line, visible: bool) -> None:
        line.set_visible(visible)
        self.canvas.draw_idle()

    # ------------------------------------------------------------------
    # Clear all data and reset the axes
    # ------------------------------------------------------------------
    def reset_chart(self) -> None:
        for line in self._all_lines:
            line.set_data([], [])
        self.ax.relim()
        self.ax.autoscale_view()
        self.ax2.relim()
        self.ax2.autoscale_view()
        self.canvas.draw_idle()

    # ------------------------------------------------------------------
    # Update the lines with *complete* new data arrays
    # ------------------------------------------------------------------
    def update_data(self, times: List[float], raw: List[float],
                    lpd: List[float], net: List[float], success: List[float]) -> None:
        """Replace the entire data set of all lines and refresh."""
        self.line_raw.set_data(times, raw)
        self.line_net.set_data(times, net)
        self.line_lpd.set_data(times, lpd)
        self.line_success.set_data(times, success)

        self.ax.relim()
        self.ax.autoscale_view()
        self.ax2.relim()
        self.ax2.autoscale_view()
        self.canvas.draw_idle()