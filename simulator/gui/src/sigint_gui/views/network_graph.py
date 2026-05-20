"""Interactive network graph using NetworkX, Pyvis, and QWebEngineView."""

from __future__ import annotations

import math
from typing import Dict, FrozenSet, List, Optional, Tuple

from PyQt5.QtCore import QLineF, QPointF, QRectF, Qt
from PyQt5.QtGui import (
    QBrush, QColor, QFont, QImage, QPainter, QPen, QPolygonF,
)
from PyQt5.QtWidgets import (
    QGraphicsEllipseItem,
    QGraphicsItem,
    QGraphicsScene,
    QGraphicsTextItem,
    QGraphicsView,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from ..models import LinkState, NodeMode, NodeState
from ..utils.layout import compute_layout

# ---------------------------------------------------------------------------
# Visual constants
# ---------------------------------------------------------------------------
NODE_RADIUS: float = 22.0
ARROW_SIZE: float = 11.0
EDGE_WIDTH_ACTIVE: float = 2.5
EDGE_WIDTH_INACTIVE: float = 1.0
LABEL_FONT = QFont("Arial", 9, QFont.Bold)

BACKGROUND_COLOR = QColor("#1e1e1e")
EDGE_ACTIVE_COLOR = QColor("#00c853")    # vivid green
EDGE_INACTIVE_COLOR = QColor("#555555")  # dim grey

NODE_COLORS: Dict[NodeMode, QColor] = {
    NodeMode.IDLE:     QColor("#9e9e9e"),   # light grey
    NodeMode.SCAN:     QColor("#69f0ae"),   # limegreen
    NodeMode.PROCESS:  QColor("#40c4ff"),   # dodgerblue
    NodeMode.TRANSMIT: QColor("#ff5252"),   # tomato
}


# ---------------------------------------------------------------------------
# Graphics items
# ---------------------------------------------------------------------------

class EdgeItem(QGraphicsItem):
    """Directed edge rendered as a line with an arrowhead at the destination.

    The line is shortened at both ends by NODE_RADIUS so it meets the node
    circumference rather than the centre.
    """

    def __init__(
        self,
        src_pos: QPointF,
        dst_pos: QPointF,
        active: bool,
        tooltip: str,
    ) -> None:
        super().__init__()
        self.setAcceptHoverEvents(True)
        self.setToolTip(tooltip)
        self.setZValue(-1)          # draw behind nodes

        self._color = EDGE_ACTIVE_COLOR if active else EDGE_INACTIVE_COLOR
        self._width = EDGE_WIDTH_ACTIVE if active else EDGE_WIDTH_INACTIVE
        self._line = QLineF()
        self._arrow = QPolygonF()

        vec = QLineF(src_pos, dst_pos)
        length = vec.length()
        if length < 2.0 * NODE_RADIUS + 1.0:
            return  # nodes overlap; skip drawing

        # Unit direction vector
        dx = (dst_pos.x() - src_pos.x()) / length
        dy = (dst_pos.y() - src_pos.y()) / length

        p1 = QPointF(src_pos.x() + dx * NODE_RADIUS,
                     src_pos.y() + dy * NODE_RADIUS)
        p2 = QPointF(dst_pos.x() - dx * NODE_RADIUS,
                     dst_pos.y() - dy * NODE_RADIUS)
        self._line = QLineF(p1, p2)

        # Arrowhead — two points fanning back from p2
        angle = math.atan2(-self._line.dy(), self._line.dx())
        a1 = QPointF(
            p2.x() + math.cos(angle + math.pi / 6.0) * ARROW_SIZE,
            p2.y() - math.sin(angle + math.pi / 6.0) * ARROW_SIZE,
        )
        a2 = QPointF(
            p2.x() + math.cos(angle - math.pi / 6.0) * ARROW_SIZE,
            p2.y() - math.sin(angle - math.pi / 6.0) * ARROW_SIZE,
        )
        self._arrow = QPolygonF([p2, a1, a2])

    # QGraphicsItem protocol ------------------------------------------------

    def boundingRect(self) -> QRectF:
        if self._line.isNull():
            return QRectF()
        extra = ARROW_SIZE + EDGE_WIDTH_ACTIVE
        return (
            QRectF(self._line.p1(), self._line.p2())
            .normalized()
            .adjusted(-extra, -extra, extra, extra)
        )

    def paint(self, painter: QPainter, option, widget=None) -> None:
        if self._line.isNull():
            return
        painter.setRenderHint(QPainter.Antialiasing)
        pen = QPen(self._color, self._width, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)
        painter.setPen(pen)
        painter.setBrush(QBrush(self._color))
        painter.drawLine(self._line)
        painter.drawPolygon(self._arrow)


class NodeItem(QGraphicsEllipseItem):
    """Circular node with a centred label; draggable and selectable."""

    def __init__(
        self,
        node_id: int,
        pos: QPointF,
        color: QColor,
        tooltip: str,
    ) -> None:
        r = NODE_RADIUS
        super().__init__(-r, -r, 2.0 * r, 2.0 * r)
        self.node_id = node_id          # public: used by scene for id lookup

        self.setPos(pos)
        self.setBrush(QBrush(color))
        self.setPen(QPen(color.lighter(160), 1.5))
        self.setToolTip(tooltip)
        self.setFlag(QGraphicsItem.ItemIsMovable, True)
        self.setFlag(QGraphicsItem.ItemIsSelectable, True)
        self.setFlag(QGraphicsItem.ItemSendsGeometryChanges, True)
        self.setAcceptHoverEvents(True)
        self.setZValue(1)               # draw above edges

        # Centred text label
        label = QGraphicsTextItem(str(node_id), self)
        label.setDefaultTextColor(Qt.white)
        label.setFont(LABEL_FONT)
        br = label.boundingRect()
        label.setPos(-br.width() / 2.0, -br.height() / 2.0)

    # Hover highlight -------------------------------------------------------

    def hoverEnterEvent(self, event) -> None:
        self.setPen(QPen(Qt.white, 2.5))
        super().hoverEnterEvent(event)

    def hoverLeaveEvent(self, event) -> None:
        color = self.brush().color()
        self.setPen(QPen(color.lighter(160), 1.5))
        super().hoverLeaveEvent(event)


# ---------------------------------------------------------------------------
# View (zoom + pan)
# ---------------------------------------------------------------------------

class _ZoomableView(QGraphicsView):
    """QGraphicsView with smooth mouse-wheel zoom and scroll-hand pan."""

    _ZOOM_FACTOR = 1.15

    def __init__(self, scene: QGraphicsScene, parent: Optional[QWidget] = None) -> None:
        super().__init__(scene, parent)
        self.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
        self.setBackgroundBrush(QBrush(BACKGROUND_COLOR))
        self.setDragMode(QGraphicsView.ScrollHandDrag)
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        self.setResizeAnchor(QGraphicsView.AnchorViewCenter)
        self.setViewportUpdateMode(QGraphicsView.BoundingRectViewportUpdate)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

    def wheelEvent(self, event) -> None:
        factor = (
            self._ZOOM_FACTOR
            if event.angleDelta().y() > 0
            else 1.0 / self._ZOOM_FACTOR
        )
        self.scale(factor, factor)


# ---------------------------------------------------------------------------
# Public widget
# ---------------------------------------------------------------------------

class NetworkGraphView(QWidget):
    """Drop-in replacement for the Pyvis/WebEngine graph view.

    The public API (update_state, export_png) is identical to the previous
    implementation so MainWindow requires no changes.
    """

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        self._scene = QGraphicsScene(self)
        self._scene.setBackgroundBrush(QBrush(BACKGROUND_COLOR))

        self._view = _ZoomableView(self._scene, self)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self._view)

        # Persisted node positions survive state updates (preserves drag layout)
        self._node_positions: Dict[int, QPointF] = {}
        # Track topology to know when fitInView is needed
        self._prev_node_ids: FrozenSet[int] = frozenset()

    # ------------------------------------------------------------------
    # Public API (matches old WebEngine-based interface)
    # ------------------------------------------------------------------

    def update_state(
        self,
        nodes: List[NodeState],
        links: List[LinkState],
        sim_time: float,
    ) -> None:
        """Rebuild the scene from fresh simulation state."""
        self._rebuild_scene(nodes, links)

    def export_png(self, path: str) -> None:
        """Render the current view to a PNG file."""
        img = QImage(self._view.viewport().size(), QImage.Format_ARGB32)
        img.fill(BACKGROUND_COLOR.rgb())
        painter = QPainter(img)
        self._view.render(painter)
        painter.end()
        img.save(path, "PNG")

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _rebuild_scene(
        self,
        nodes: List[NodeState],
        links: List[LinkState],
    ) -> None:
        """Clear and repopulate the QGraphicsScene.

        Dragged node positions are captured before the clear and restored
        when items are re-created, so user-adjusted layouts survive ticks.
        """
        # 1. Snapshot positions of any dragged nodes before clearing
        for item in self._scene.items():
            if isinstance(item, NodeItem):
                self._node_positions[item.node_id] = item.pos()

        self._scene.clear()

        if not nodes:
            return

        # 2. Run layout only for nodes without a known position
        node_ids = [ns.id for ns in nodes]
        edge_pairs = [(ls.from_id, ls.to_id) for ls in links]
        unknown = [nid for nid in node_ids if nid not in self._node_positions]
        if unknown:
            computed = compute_layout(
                node_ids=node_ids,
                edges=edge_pairs,
                seed=42,
                width=800,
                height=600,
            )
            for nid, (x, y) in computed.items():
                if nid not in self._node_positions:
                    self._node_positions[nid] = QPointF(x, y)

        pos: Dict[int, QPointF] = {
            ns.id: self._node_positions.get(ns.id, QPointF(0.0, 0.0))
            for ns in nodes
        }

        # 3. Edges first — drawn behind nodes (ZValue = -1)
        for ls in links:
            if ls.from_id not in pos or ls.to_id not in pos:
                continue
            tooltip = (
                f"SNR: {ls.snr:.1f} dB\n"
                f"Capacity: {ls.capacity_bps / 1e6:.1f} Mbps\n"
                f"Active: {ls.active}"
            )
            self._scene.addItem(
                EdgeItem(pos[ls.from_id], pos[ls.to_id], ls.active, tooltip)
            )

        # 4. Nodes on top (ZValue = 1)
        for ns in nodes:
            color = NODE_COLORS.get(ns.mode, QColor("gray"))
            tooltip = (
                f"Node {ns.id}: {ns.name}\n"
                f"Mode: {ns.mode.name}\n"
                f"Buffer: {ns.buffer_size}\n"
                f"Energy: {ns.energy_used:.3e}"
            )
            self._scene.addItem(NodeItem(ns.id, pos[ns.id], color, tooltip))

        # 5. Fit view only when the topology changes — not on every colour update
        current_ids = frozenset(node_ids)
        if current_ids != self._prev_node_ids:
            self._prev_node_ids = current_ids
            self._view.fitInView(
                self._scene.itemsBoundingRect().adjusted(
                    -NODE_RADIUS, -NODE_RADIUS, NODE_RADIUS, NODE_RADIUS
                ),
                Qt.KeepAspectRatio,
            )
