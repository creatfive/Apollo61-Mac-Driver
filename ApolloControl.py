#!/usr/bin/env python3
import sys
import os
import subprocess
import threading
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                               QHBoxLayout, QLabel, QComboBox, QPushButton, 
                               QSlider, QCheckBox, QColorDialog, QFrame,
                               QStackedWidget, QGridLayout, QScrollArea, QSizePolicy)
from PySide6.QtCore import Qt, Signal, QObject, QSize, QPropertyAnimation, QEasingCurve
from PySide6.QtGui import QColor, QFont, QIcon, QPainter, QBrush, QPen, QPainterPath

class WorkerSignals(QObject):
    finished = Signal()

# Custom UI Elements mimicking the KeyCraft Pro React design
class Card(QFrame):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("card")
        self.setStyleSheet("""
            QFrame#card {
                background-color: rgba(255, 255, 255, 0.03);
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 16px;
            }
        """)

class SidebarButton(QPushButton):
    def __init__(self, text, active=False):
        super().__init__(text)
        self.setCheckable(True)
        self.setChecked(active)
        self.setFixedHeight(48)
        self.setCursor(Qt.PointingHandCursor)
        self.setStyleSheet("""
            QPushButton {
                background-color: transparent;
                color: rgba(255, 255, 255, 0.4);
                text-align: left;
                padding-left: 20px;
                border-radius: 12px;
                font-size: 14px;
                font-weight: bold;
                font-family: 'Helvetica Neue';
            }
            QPushButton:hover {
                background-color: rgba(255, 255, 255, 0.05);
                color: rgba(255, 255, 255, 0.8);
            }
            QPushButton:checked {
                background-color: rgba(255, 255, 255, 0.1);
                color: #FFFFFF;
            }
        """)

class ApolloApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("KeyCraft Pro")
        self.resize(1000, 700)
        self.setMinimumSize(900, 600)
        
        self.current_r = 255
        self.current_g = 0
        self.current_b = 102 # #ff0066 pinkish like the react app
        self.is_applying = False
        
        self.signals = WorkerSignals()
        self.signals.finished.connect(self.on_sync_complete)
        
        self.setup_ui()
        self.apply_theme()
        
    def setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        
        # --- SIDEBAR ---
        sidebar = QFrame()
        sidebar.setFixedWidth(240)
        sidebar.setObjectName("sidebar")
        sidebar_layout = QVBoxLayout(sidebar)
        sidebar_layout.setContentsMargins(20, 30, 20, 20)
        sidebar_layout.setSpacing(10)
        
        # Logo
        logo_layout = QHBoxLayout()
        logo_icon = QFrame()
        logo_icon.setFixedSize(40, 40)
        logo_icon.setStyleSheet("background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #00E5FF, stop:1 #9D00FF); border-radius: 12px;")
        
        title = QLabel("KeyCraft<span style='color: #00E5FF;'>Pro</span>")
        title.setStyleSheet("font-size: 20px; font-weight: bold; color: white;")
        
        logo_layout.addWidget(logo_icon)
        logo_layout.addWidget(title)
        logo_layout.addStretch()
        sidebar_layout.addLayout(logo_layout)
        sidebar_layout.addSpacing(30)
        
        # Navigation
        self.nav_lighting = SidebarButton("  Lighting", active=True)
        self.nav_settings = SidebarButton("  Settings")
        sidebar_layout.addWidget(self.nav_lighting)
        sidebar_layout.addWidget(self.nav_settings)
        sidebar_layout.addStretch()
        
        # Connection Status
        conn_card = Card()
        conn_layout = QVBoxLayout(conn_card)
        conn_layout.setContentsMargins(15, 15, 15, 15)
        conn_label = QLabel("● CONNECTED")
        conn_label.setStyleSheet("color: #00FF66; font-size: 10px; font-weight: bold; letter-spacing: 1px;")
        device_label = QLabel("Apollo61 Native")
        device_label.setStyleSheet("color: white; font-size: 13px; font-weight: bold;")
        conn_layout.addWidget(conn_label)
        conn_layout.addWidget(device_label)
        sidebar_layout.addWidget(conn_card)
        
        main_layout.addWidget(sidebar)
        
        # --- MAIN CONTENT AREA ---
        content_area = QFrame()
        content_area.setObjectName("contentArea")
        content_layout = QVBoxLayout(content_area)
        content_layout.setContentsMargins(0, 0, 0, 0)
        content_layout.setSpacing(0)
        
        # Header
        header = QFrame()
        header.setFixedHeight(70)
        header.setObjectName("header")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(30, 0, 30, 0)
        
        view_label = QLabel("ACTIVE VIEW / <span style='color: white;'>LIGHTING</span>")
        view_label.setStyleSheet("color: rgba(255, 255, 255, 0.4); font-size: 12px; font-weight: bold; letter-spacing: 1px;")
        
        self.apply_btn = QPushButton("  Apply Changes")
        self.apply_btn.setObjectName("applyBtn")
        self.apply_btn.setFixedSize(160, 40)
        self.apply_btn.setCursor(Qt.PointingHandCursor)
        self.apply_btn.clicked.connect(self.trigger_sync)
        
        header_layout.addWidget(view_label)
        header_layout.addStretch()
        header_layout.addWidget(self.apply_btn)
        
        content_layout.addWidget(header)
        
        # Scrollable Content
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border: none; background: transparent; } QWidget#scrollContent { background: transparent; }")
        
        scroll_content = QWidget()
        scroll_content.setObjectName("scrollContent")
        scroll_layout = QVBoxLayout(scroll_content)
        scroll_layout.setContentsMargins(40, 40, 40, 40)
        scroll_layout.setSpacing(30)
        
        # Section Title
        sec_title = QLabel("Lighting Studio")
        sec_title.setStyleSheet("font-size: 28px; font-weight: bold; color: white;")
        sec_sub = QLabel("Customize zones and dynamic effects")
        sec_sub.setStyleSheet("font-size: 14px; color: rgba(255, 255, 255, 0.4);")
        scroll_layout.addWidget(sec_title)
        scroll_layout.addWidget(sec_sub)
        scroll_layout.addSpacing(10)
        
        # Cards Grid
        grid = QGridLayout()
        grid.setSpacing(20)
        
        # Card 1: Color Picker & Mode
        left_card = Card()
        left_layout = QVBoxLayout(left_card)
        left_layout.setContentsMargins(25, 25, 25, 25)
        left_layout.setSpacing(20)
        
        mode_lbl = QLabel("LIGHTING MODE")
        mode_lbl.setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 11px; font-weight: bold; letter-spacing: 1px;")
        
        self.mode_combo = QComboBox()
        self.mode_combo.setFixedHeight(40)
        self.modes = [
            ("Static", 1), ("Reactive", 2), ("Single Off", 3), ("Glittering", 4),
            ("Falling", 5), ("Colourful", 6), ("Breath", 7), ("Spectrum", 8),
            ("Outward", 9), ("Scrolling", 10), ("Rolling", 11), ("Rotating", 12),
            ("Explode", 13), ("Launch", 14), ("Ripples", 15), ("Flowing", 16),
            ("Pulsating", 17), ("Tilt", 18), ("Shuttle", 19)
        ]
        for name, _ in self.modes:
            self.mode_combo.addItem(name)
            
        color_lbl = QLabel("GLOBAL COLOR")
        color_lbl.setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 11px; font-weight: bold; letter-spacing: 1px;")
        
        color_row = QHBoxLayout()
        self.color_preview = QFrame()
        self.color_preview.setFixedSize(60, 40)
        self.color_preview.setStyleSheet(f"background-color: #ff0066; border-radius: 8px; border: 1px solid rgba(255,255,255,0.1);")
        
        self.pick_btn = QPushButton("Choose Color")
        self.pick_btn.setFixedHeight(40)
        self.pick_btn.setCursor(Qt.PointingHandCursor)
        self.pick_btn.clicked.connect(self.pick_color)
        
        color_row.addWidget(self.color_preview)
        color_row.addWidget(self.pick_btn)
        
        left_layout.addWidget(mode_lbl)
        left_layout.addWidget(self.mode_combo)
        left_layout.addSpacing(10)
        left_layout.addWidget(color_lbl)
        left_layout.addLayout(color_row)
        left_layout.addStretch()
        
        # Card 2: Sliders
        right_card = Card()
        right_layout = QVBoxLayout(right_card)
        right_layout.setContentsMargins(25, 25, 25, 25)
        right_layout.setSpacing(20)
        
        self.bright_lbl = QLabel("BRIGHTNESS: 100%")
        self.bright_lbl.setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 11px; font-weight: bold; letter-spacing: 1px;")
        self.bright_slider = QSlider(Qt.Horizontal)
        self.bright_slider.setRange(0, 15)
        self.bright_slider.setValue(15)
        self.bright_slider.valueChanged.connect(self.update_bright_label)
        
        self.speed_lbl = QLabel("ANIMATION SPEED: 100%")
        self.speed_lbl.setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 11px; font-weight: bold; letter-spacing: 1px;")
        self.speed_slider = QSlider(Qt.Horizontal)
        self.speed_slider.setRange(0, 10)
        self.speed_slider.setValue(10)
        self.speed_slider.valueChanged.connect(self.update_speed_label)
        
        self.rainbow_check = QCheckBox("Force Rainbow Cycle")
        self.rainbow_check.setStyleSheet("""
            QCheckBox { color: white; font-size: 13px; font-weight: bold; }
            QCheckBox::indicator { width: 20px; height: 20px; border-radius: 6px; background-color: rgba(255,255,255,0.1); }
            QCheckBox::indicator:checked { background-color: #00E5FF; }
        """)
        
        right_layout.addWidget(self.bright_lbl)
        right_layout.addWidget(self.bright_slider)
        right_layout.addSpacing(10)
        right_layout.addWidget(self.speed_lbl)
        right_layout.addWidget(self.speed_slider)
        right_layout.addSpacing(20)
        right_layout.addWidget(self.rainbow_check)
        right_layout.addStretch()
        
        grid.addWidget(left_card, 0, 0)
        grid.addWidget(right_card, 0, 1)
        grid.setColumnStretch(0, 1)
        grid.setColumnStretch(1, 1)
        
        scroll_layout.addLayout(grid)
        
        # Preview Card
        preview_card = Card()
        preview_layout = QVBoxLayout(preview_card)
        preview_layout.setContentsMargins(40, 60, 40, 60)
        prev_title = QLabel("Live Preview Active")
        prev_title.setAlignment(Qt.AlignCenter)
        prev_title.setStyleSheet("color: white; font-size: 18px; font-weight: bold;")
        prev_sub = QLabel("Hardware lighting updates dynamically on apply")
        prev_sub.setAlignment(Qt.AlignCenter)
        prev_sub.setStyleSheet("color: rgba(255,255,255,0.4); font-size: 13px;")
        preview_layout.addWidget(prev_title)
        preview_layout.addWidget(prev_sub)
        
        scroll_layout.addWidget(preview_card)
        scroll_layout.addStretch()
        
        scroll.setWidget(scroll_content)
        content_layout.addWidget(scroll)
        main_layout.addWidget(content_area)

    def apply_theme(self):
        qss = """
        QMainWindow {
            background-color: #09090B;
        }
        QFrame#sidebar {
            background-color: rgba(0, 0, 0, 0.4);
            border-right: 1px solid rgba(255, 255, 255, 0.05);
        }
        QFrame#header {
            background-color: rgba(0, 0, 0, 0.4);
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
        }
        QComboBox {
            background-color: rgba(255, 255, 255, 0.05);
            color: #FFFFFF;
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 8px;
            padding: 8px 15px;
            font-size: 14px;
            font-weight: bold;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox QAbstractItemView {
            background-color: #1E1E1E;
            color: white;
            selection-background-color: #00E5FF;
            selection-color: black;
            border-radius: 8px;
        }
        QPushButton {
            background-color: rgba(255, 255, 255, 0.05);
            color: #FFFFFF;
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 8px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: rgba(255, 255, 255, 0.1);
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        QPushButton#applyBtn {
            background-color: white;
            color: black;
            border: none;
            border-radius: 8px;
        }
        QPushButton#applyBtn:hover {
            background-color: rgba(255, 255, 255, 0.9);
        }
        QPushButton#applyBtn:disabled {
            background-color: #00E5FF;
            color: black;
        }
        QSlider::groove:horizontal {
            border: none;
            height: 6px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: white;
            width: 18px;
            height: 18px;
            margin: -6px 0;
            border-radius: 9px;
        }
        QSlider::sub-page:horizontal {
            background: #00E5FF;
            border-radius: 3px;
        }
        """
        self.setStyleSheet(qss)
        
    def update_bright_label(self, value):
        percent = int((value / 15) * 100)
        self.bright_lbl.setText(f"BRIGHTNESS: {percent}%")
        
    def update_speed_label(self, value):
        percent = int((value / 10) * 100)
        self.speed_lbl.setText(f"ANIMATION SPEED: {percent}%")
        
    def pick_color(self):
        color = QColorDialog.getColor(QColor(self.current_r, self.current_g, self.current_b), self, "Choose Lighting Color")
        if color.isValid():
            self.current_r = color.red()
            self.current_g = color.green()
            self.current_b = color.blue()
            hex_color = color.name()
            self.color_preview.setStyleSheet(f"background-color: {hex_color}; border-radius: 8px; border: 1px solid rgba(255,255,255,0.1);")
            self.rainbow_check.setChecked(False)

    def trigger_sync(self):
        if self.is_applying:
            return
            
        self.is_applying = True
        self.apply_btn.setEnabled(False)
        self.apply_btn.setText("  Syncing...")
        
        mode_idx = self.mode_combo.currentIndex()
        mode_id = self.modes[mode_idx][1]
        bright = self.bright_slider.value()
        speed = self.speed_slider.value()
        is_rainbow = 1 if self.rainbow_check.isChecked() else 0
        
        args = [
            f"{mode_id:02x}", f"{self.current_r:02x}", f"{self.current_g:02x}",
            f"{self.current_b:02x}", f"{bright:02x}", f"{speed:02x}", f"{is_rainbow}"
        ]
        
        def push_to_keyboard():
            script_path = os.path.join(os.path.dirname(__file__), "set_color.py")
            subprocess.run([script_path] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.signals.finished.emit()
            
        threading.Thread(target=push_to_keyboard, daemon=True).start()

    def on_sync_complete(self):
        self.is_applying = False
        self.apply_btn.setEnabled(True)
        self.apply_btn.setText("  Apply Changes")

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    app = QApplication(sys.argv)
    window = ApolloApp()
    window.show()
    sys.exit(app.exec())
