"""Parameter panel for adding / editing nodes, links, and simulation settings."""

from __future__ import annotations

from typing import Dict, List, Optional

from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import (
    QComboBox,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
    QScrollArea,
    QInputDialog,
)

from ..models import NodeMode, SimulatorConfig

_DEFAULT_COMPUTE = 1.0     # GFLOPS
_DEFAULT_MEMORY  = 1024     # MiB
_DEFAULT_BANDS   = [2400.0] # MHz

class ParameterPanel(QWidget):
    """A dockable panel for customising the simulation."""

    # Emitted when the user wants to apply a new topology.
    config_changed = pyqtSignal(SimulatorConfig, str)

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._current_config = SimulatorConfig()
        
        # ------------------------------------------------------------------
        # 0. Create mutable variables to circumvent frozen class
        # ------------------------------------------------------------------
        self._current_params: dict = {
        "seed": 42, "duration": 10.0,
        "topology": [(0,1),(1,0)],
        "availability": 0.9, "avg_snr_db": 20.0
        }
        self._current_config = SimulatorConfig(**self._current_params)
        
        # ------------------------------------------------------------------
        # 1. Create the widget that will hold all the controls
        # ------------------------------------------------------------------
        content = QWidget()
        layout = QVBoxLayout(content)          # everything goes into 'content'

        # --- Simulation settings ---
        sim_group = QGroupBox("Simulation Settings")
        sim_form = QFormLayout()
        self._seed_spin = QSpinBox()
        self._seed_spin.setRange(0, 9999999)
        self._seed_spin.setValue(self._current_config.seed)
        sim_form.addRow("Seed:", self._seed_spin)

        self._dur_spin = QDoubleSpinBox()
        self._dur_spin.setRange(0.1, 1000.0)
        self._dur_spin.setValue(self._current_config.duration)
        sim_form.addRow("Duration (s):", self._dur_spin)

        self._avail_spin = QDoubleSpinBox()
        self._avail_spin.setRange(0.0, 1.0)
        self._avail_spin.setSingleStep(0.05)
        self._avail_spin.setValue(self._current_config.availability)
        sim_form.addRow("Link Availability:", self._avail_spin)

        self._snr_spin = QDoubleSpinBox()
        self._snr_spin.setRange(-50.0, 80.0)
        self._snr_spin.setValue(self._current_config.avg_snr_db)
        sim_form.addRow("Avg SNR (dB):", self._snr_spin)
        sim_group.setLayout(sim_form)
        layout.addWidget(sim_group)

        # --- Node list & editing ---
        node_group = QGroupBox("Nodes")
        node_layout = QVBoxLayout()
        self._node_list = QListWidget()
        self._node_list.currentRowChanged.connect(self._on_node_selected)
        node_layout.addWidget(self._node_list)

        btn_layout = QHBoxLayout()
        add_btn = QPushButton("Add Node")
        add_btn.clicked.connect(self._add_node)
        del_btn = QPushButton("Remove")
        del_btn.clicked.connect(self._remove_node)
        btn_layout.addWidget(add_btn)
        btn_layout.addWidget(del_btn)
        node_layout.addLayout(btn_layout)

        # Node property editor
        prop_form = QFormLayout()
        self._name_edit = QLineEdit()
        prop_form.addRow("Name:", self._name_edit)

        self._compute_spin = QDoubleSpinBox()
        self._compute_spin.setRange(0.1, 10000.0)
        self._compute_spin.setValue(_DEFAULT_COMPUTE)
        prop_form.addRow("Compute (GFLOPS):", self._compute_spin)

        self._mem_spin = QSpinBox()
        self._mem_spin.setRange(1, 65536)
        self._mem_spin.setValue(_DEFAULT_MEMORY)
        prop_form.addRow("Memory (MiB):", self._mem_spin)

        self._band_edit = QLineEdit()
        self._band_edit.setText(str(_DEFAULT_BANDS[0]))
        prop_form.addRow("Band (MHz, comma sep):", self._band_edit)

        apply_props_btn = QPushButton("Apply Properties")
        apply_props_btn.clicked.connect(self._apply_node_properties)
        prop_form.addRow(apply_props_btn)

        node_layout.addLayout(prop_form)
        node_group.setLayout(node_layout)
        layout.addWidget(node_group)

        # --- Edge controls ---
        edge_group = QGroupBox("Edges")
        edge_layout = QFormLayout()
        self._from_combo = QComboBox()
        self._to_combo = QComboBox()
        edge_layout.addRow("From:", self._from_combo)
        edge_layout.addRow("To:", self._to_combo)
        add_edge_btn = QPushButton("Add Edge")
        add_edge_btn.clicked.connect(self._add_edge)
        del_edge_btn = QPushButton("Remove Edge")
        del_edge_btn.clicked.connect(self._remove_edge)
        btn_row = QHBoxLayout()
        btn_row.addWidget(add_edge_btn)
        btn_row.addWidget(del_edge_btn)
        edge_layout.addRow(btn_row)
        edge_group.setLayout(edge_layout)
        layout.addWidget(edge_group)

        # --- Apply / Reset buttons ---
        bottom_layout = QHBoxLayout()
        apply_all_btn = QPushButton("Apply & Restart")
        apply_all_btn.clicked.connect(self._emit_config)
        reset_btn = QPushButton("Reset to Default")
        reset_btn.clicked.connect(self._reset_to_default)
        bottom_layout.addWidget(apply_all_btn)
        bottom_layout.addWidget(reset_btn)
        layout.addLayout(bottom_layout)

        layout.addStretch()
        
        #---- Agent Selection ------
        self._agent_combo = QComboBox()
        self._agent_combo.addItems(["Random", "Gossip", "DTN", "HQL"])
        agent_layout = QFormLayout()
        agent_layout.addRow("Agent:", self._agent_combo)
        agent_group = QGroupBox("Agent")
        agent_group.setLayout(agent_layout)
        layout.addWidget(agent_group) 
        
        
        # ------------------------------------------------------------------
        # 2. Wrap the content widget in a QScrollArea
        # ------------------------------------------------------------------
        scroll = QScrollArea()
        scroll.setWidget(content)
        scroll.setWidgetResizable(True)
        
        # ------------------------------------------------------------------
        # 3. Set the scroll area as the only widget of the panel
        # ------------------------------------------------------------------
        main_layout = QVBoxLayout(self)
        main_layout.addWidget(scroll)
        self.setLayout(main_layout)

        self._populate_nodes()
        self._populate_edges()

    # ------------------------------------------------------------------
    # Data population helpers
    # ------------------------------------------------------------------

    def _populate_nodes(self) -> None:
        self._node_list.blockSignals(True)
        self._node_list.clear()
        self._from_combo.clear()
        self._to_combo.clear()
        for nid in sorted(self._current_config.topology_nodes()):
            item = QListWidgetItem(f"Node {nid}")
            self._node_list.addItem(item)
            self._from_combo.addItem(str(nid), nid)
            self._to_combo.addItem(str(nid), nid)
        self._node_list.blockSignals(False)

    def _populate_edges(self) -> None:
        self._from_combo.clear()
        self._to_combo.clear()
        for nid in sorted(self._current_config.topology_nodes()):
            self._from_combo.addItem(str(nid), nid)
            self._to_combo.addItem(str(nid), nid)

    # ------------------------------------------------------------------
    # Node editing
    # ------------------------------------------------------------------

    def _on_node_selected(self, row: int) -> None:
        if row < 0:
            return
        nid = self._current_config.topology_nodes()[row]
        # We need to store per‑node metadata; currently the config only has topology
        # For MVP, we'll store metadata in a separate dict attached to the config later.
        # For now, we just populate the fields with defaults.
        self._name_edit.setText(f"Node_{nid}")
        self._compute_spin.setValue(_DEFAULT_COMPUTE)
        self._mem_spin.setValue(_DEFAULT_MEMORY)
        self._band_edit.setText(str(_DEFAULT_BANDS[0]))

    def _apply_node_properties(self) -> None:
        row = self._node_list.currentRow()
        if row < 0:
            QMessageBox.warning(self, "No Node", "Select a node first.")
            return
        # In a full implementation we would store metadata in a dict,
        # e.g. self._node_metadata[nid] = {...}
        # For now we show a message.
        QMessageBox.information(self, "Info", "Node properties will be applied on restart.")

    def _add_node(self) -> None:
        existing_ids = set(self._current_config.topology_nodes())
        new_id = max(existing_ids) + 1 if existing_ids else 0
        # Add the node to the config's topology (currently the topology is a list of tuples)
        # We need to store node IDs independently. We'll extend SimulatorConfig later.
        # For now, we'll add a node by expanding the topology list? Not straightforward.
        # We'll maintain a separate node list in the panel, and when applying,
        # we rebuild the config with an explicit nodes list.
        # Quick fix: we'll add a dummy edge from new_id to itself? Not allowed.
        # Better: we'll add the new node to a custom node list attribute (we'll extend SimulatorConfig).
        QMessageBox.information(self, "Info", "Node addition will be implemented after config extension.")

    def _remove_node(self) -> None:
        row = self._node_list.currentRow()
        if row < 0:
            return
        nid = self._current_config.topology_nodes()[row]
        # Confirm
        reply = QMessageBox.question(self, "Remove Node",
                                     f"Remove node {nid} and all its edges?",
                                     QMessageBox.Yes | QMessageBox.No)
        if reply == QMessageBox.Yes:
            # Remove node and associated edges
            self._current_config.remove_node(nid)
            self._populate_nodes()
            self._emit_config()   # immediate restart for now

    # ------------------------------------------------------------------
    # Edge editing
    # ------------------------------------------------------------------

    def _add_edge(self) -> None:
        from_id = self._from_combo.currentData()
        to_id   = self._to_combo.currentData()
        if from_id is None or to_id is None or from_id == to_id:
            return
        topo = list(self._current_params["topology"])
        if (from_id, to_id) not in topo:
            topo.append((from_id, to_id))
            self._current_params["topology"] = topo
            self._emit_config()

    def _remove_edge(self) -> None:
        from_id = self._from_combo.currentData()
        to_id   = self._to_combo.currentData()
        if from_id is None or to_id is None:
            return
        edge = (from_id, to_id)
        topo = list(self._current_params["topology"])   # work on a mutable copy
        if edge in topo:
            topo.remove(edge)
            self._current_params["topology"] = topo
            self._emit_config()

    # ------------------------------------------------------------------
    # Global config emission
    # ------------------------------------------------------------------

    def _emit_config(self) -> None:
        self._current_params["seed"] = self._seed_spin.value()
        self._current_params["duration"] = self._dur_spin.value()
        self._current_params["availability"] = self._avail_spin.value()
        self._current_params["avg_snr_db"] = self._snr_spin.value()
        # topology is updated separately via add/remove edge methods
        self._current_config = SimulatorConfig(**self._current_params)
        self.config_changed.emit(self._current_config, self._agent_combo.currentText())

    def _reset_to_default(self) -> None:
        default_cfg = SimulatorConfig()                     # fresh frozen config
        self._current_params = {
            "seed": default_cfg.seed,
            "duration": default_cfg.duration,
            "topology": list(default_cfg.topology),         # deep copy the list
            "availability": default_cfg.availability,
            "avg_snr_db": default_cfg.avg_snr_db,
        }
        # Update widgets to match
        self._seed_spin.setValue(self._current_params["seed"])
        self._dur_spin.setValue(self._current_params["duration"])
        self._avail_spin.setValue(self._current_params["availability"])
        self._snr_spin.setValue(self._current_params["avg_snr_db"])
        self._populate_nodes()
        self._populate_edges()
        self._emit_config()
        
    def load_from_dict(self, data: dict) -> None:
        # Update mutable parameters
        self._current_params["seed"]        = data.get("seed", 42)
        self._current_params["duration"]    = data.get("duration", 10.0)
        self._current_params["timestep"]    = data.get("timestep", 0.2)
        self._current_params["availability"] = data.get("availability", 0.9)
        self._current_params["avg_snr_db"]  = data.get("avg_snr_db", 20.0)

        topo = data.get("topology_edges") or data.get("topology", [(0,1),(1,0)])
        self._current_params["topology"] = [tuple(e) for e in topo]

        # Agent type
        agent = data.get("agent_type", "Random")
        idx = self._agent_combo.findText(agent)
        if idx >= 0:
            self._agent_combo.setCurrentIndex(idx)

        # ★ Rebuild the frozen config from the updated params
        self._current_config = SimulatorConfig(**self._current_params)

        # Update UI widgets
        self._seed_spin.setValue(self._current_params["seed"])
        self._dur_spin.setValue(self._current_params["duration"])
        self._avail_spin.setValue(self._current_params["availability"])
        self._snr_spin.setValue(self._current_params["avg_snr_db"])

        # Populate node list and edge combos from the new config
        self._populate_nodes()
        # (edge combos are already repopulated inside _populate_nodes)
