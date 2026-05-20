"""Interactive network graph view (QGraphicsScene + QGraphicsView)."""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

from PyQt5.QtCore import QPointF, QRectF, Qt
from PyQt5.QtGui import (
    QBrush,
    QColor,
    QFont,
    QPainter,
    QPen,
    QRadialGradient,
)
from PyQt5.QtWidgets import (
    QGraphicsEllipseItem,
    QGraphicsItem,
    QGraphicsLineItem,
    QGraphicsScene,
    QGraphicsTextItem,
    QGraphicsView,
    QToolTip,
)

from ..models import LinkState, NodeMode, NodeState

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
NODE_RADIUS: float = 20.0
NODE_COLORS: Dict[NodeMode, QColor] = {
    NodeMode.IDLE: QColor(200, 200, 200),
    NodeMode.SCAN: QColor(100, 200, 100),
    NodeMode.PROCESS: QColor(100, 100, 255),
    NodeMode.TRANSMIT: QColor(255, 80, 80),
}
LINK_ACTIVE_PEN = QPen(QColor(0, 180, 0), 2)
LINK_INACTIVE_PEN = QPen(QColor(120, 120, 120), 1, Qt.DashLine)
FONT = QFont("Sans", 8)

# Scene margin (world coordinates)
MARGIN: float = 50.0


# ---------------------------------------------------------------------------
# Custom items
# ---------------------------------------------------------------------------

class _NodeItem(QGraphicsEllipseItem):
    """A node in the network graph, drawn as a coloured circle."""

    def __init__(self, node_state: NodeState) -> None:
        super().__init__(-NODE_RADIUS, -NODE_RADIUS, 2 * NODE_RADIUS, 2 * NODE_RADIUS)
        self._node = node_state
        self.setFlag(QGraphicsItem.ItemIsSelectable, True)
        self.setAcceptHoverEvents(True)
        self._refresh_brush()
        self._label = QGraphicsTextItem(str(node_state.id), self)
        self._label.setFont(FONT)
        self._label.setDefaultTextColor(Qt.white)
        self._label.setPos(-5, -8)

    def _refresh_brush(self) -> None:
        colour = NODE_COLORS.get(self._node.mode, NODE_COLORS[NodeMode.IDLE])
        gradient = QRadialGradient(0, 0, NODE_RADIUS)
        gradient.setColorAt(0, colour.lighter(130))
        gradient.setColorAt(1, colour.darker(150))
        self.setBrush(QBrush(gradient))
        self.setPen(QPen(colour.darker(200), 1.5))

    def update_from_state(self, node_state: NodeState) -> None:
        """Update appearance when the node state changes."""
        self._node = node_state
        self._refresh_brush()
        self._label.setPlainText(str(node_state.id))

    def hoverEnterEvent(self, event: object) -> None:
        msg = (
            f"Node {self._node.id}: {self._node.name}\n"
            f"Mode: {self._node.mode.name}\n"
            f"Buffer: {self._node.buffer_size}\n"
            f"Energy: {self._node.energy_used:.3e}"
        )
        QToolTip.showText(event.screenPos(), msg)  # type: ignore[attr-defined]


class _LinkItem(QGraphicsLineItem):
    """A link between two node items, drawn as a line."""

    def __init__(self, link_state: LinkState) -> None:
        super().__init__()
        self._link = link_state
        self._update_pen()

    def _update_pen(self) -> None:
        self.setPen(LINK_ACTIVE_PEN if self._link.active else LINK_INACTIVE_PEN)

    def update_from_state(self, link_state: LinkState) -> None:
        self._link = link_state
        self._update_pen()

    def set_endpoints(self, p1: QPointF, p2: QPointF) -> None:
        self.setLine(p1.x(), p1.y(), p2.x(), p2.y())


# ---------------------------------------------------------------------------
# NetworkGraphView (QGraphicsView)
# ---------------------------------------------------------------------------

class NetworkGraphView(QGraphicsView):
    """View that holds the scene and updates nodes/links from simulation state."""

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._scene = QGraphicsScene(self)
        self.setScene(self._scene)
        self.setRenderHints(QPainter.Antialiasing)
        self.setDragMode(QGraphicsView.ScrollHandDrag)
        self.setViewportUpdateMode(QGraphicsView.SmartViewportUpdate)
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)

        # Internal cache of scene items, keyed by node/link id
        self._node_items: Dict[int, _NodeItem] = {}
        self._link_items: Dict[int, _LinkItem] = {}

        # Simple fixed layout (will be replaced by layout.py later)
        self._positions: Dict[int, QPointF] = {}

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def update_state(
        self,
        nodes: List[NodeState],
        links: List[LinkState],
        sim_time: float,
    ) -> None:
        """Update the graph to reflect the current simulator state."""
        assert isinstance(nodes, list)
        assert isinstance(links, list)

        # Assign positions if new nodes appear
        for ns in nodes:
            if ns.id not in self._positions:
                self._positions[ns.id] = self._default_position(ns.id, nodes)

        # Create or update node items
        for ns in nodes:
            if ns.id not in self._node_items:
                item = _NodeItem(ns)
                self._scene.addItem(item)
                self._node_items[ns.id] = item
            else:
                self._node_items[ns.id].update_from_state(ns)
            # Move node to its layout position
            pos = self._positions[ns.id]
            self._node_items[ns.id].setPos(pos)

        # Remove nodes that no longer exist
        for nid in list(self._node_items):
            if not any(n.id == nid for n in nodes):
                self._scene.removeItem(self._node_items[nid])
                del self._node_items[nid]

        # Create or update link items
        for ls in links:
            lid = ls.id
            if lid not in self._link_items:
                item = _LinkItem(ls)
                self._scene.addItem(item)
                self._link_items[lid] = item
            else:
                self._link_items[lid].update_from_state(ls)
            # Update endpoint positions
            p1 = self._positions.get(ls.from_id, QPointF(0, 0))
            p2 = self._positions.get(ls.to_id, QPointF(0, 0))
            self._link_items[lid].set_endpoints(p1, p2)

        # Remove links that no longer exist
        for lid in list(self._link_items):
            if not any(l.id == lid for l in links):
                self._scene.removeItem(self._link_items[lid])
                del self._link_items[lid]

    def export_png(self, path: str) -> None:
        """Render the current scene to a PNG file."""
        rect = self._scene.itemsBoundingRect()
        image = self.grab().toImage()
        image.save(path, "PNG")
        logger.info("Graph exported to %s", path)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _default_position(
        self, node_id: int, nodes: List[NodeState]
    ) -> QPointF:
        """Arrange nodes in a circle if no layout is set."""
        n = max(1, len(nodes))
        angle = (2 * math.pi / n) * node_id
        radius = 150.0
        x = radius * math.cos(angle) + 200
        y = radius * math.sin(angle) + 200
        return QPointF(x, y)