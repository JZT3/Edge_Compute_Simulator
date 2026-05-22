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
        
        self.ax = self.figure.add_subplot(111)
        self.ax.set_xlabel("Time (s)")
        self.ax.set_ylabel("Cumulative Intelligence")
        self.line, = self.ax.plot([], [])
        self._x_data = []
        self._y_data = []

    def update_data(self, x: list[float], y: list[float]) -> None:
        self.line.set_data(x, y)
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw_idle()