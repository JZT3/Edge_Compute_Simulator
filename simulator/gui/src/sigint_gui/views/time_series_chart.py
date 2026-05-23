from PyQt5.QtWidgets import QWidget, QVBoxLayout
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from matplotlib.figure import Figure

class TimeSeriesChart(QWidget):
    def __init__(self, title="Cumulative Intelligence", parent=None):
        super().__init__(parent)
        self.setMinimumSize(400, 250)
        
        self.figure = Figure(figsize=(5, 3), dpi=100)
        self.canvas = FigureCanvasQTAgg(self.figure)
        layout = QVBoxLayout(self)
        layout.addWidget(self.canvas)
        
        # Primary axis (Mission Value)
        self.ax = self.figure.add_subplot(111)
        self.ax.set_xlabel("Simulation Time (s)")
        self.ax.set_ylabel("Mission Value (net intel)")
        
        # Secondary axis (TX Success Rate)
        self.ax2 = self.ax.twinx()
        self.ax2.set_ylabel("TX Success Rate (%)")
        
        # Lines
        self.line_mission, = self.ax.plot([], [], 'b-', label="Mission Value")
        self.line_success, = self.ax2.plot([], [], 'r--', label="TX Success Rate")

        # Legends
        self.ax.legend(loc='upper left')
        self.ax2.legend(loc='upper right')

    def update_data(self, x, y_mission, y_success):
        """Update both curves.

        Args:
            x: list of simulation times.
            y_mission: list of cumulative mission values.
            y_success: list of transmission success rates (0‑100).
        """
        self.line_mission.set_data(x, y_mission)
        self.line_success.set_data(x, y_success)

        # Adjust y-limits for success rate to always show 0-100
        self.ax2.set_ylim(-5, 105)   # fixed range for percentage

        self.ax.relim()
        self.ax.autoscale_view()

        self.canvas.draw_idle()