#!/usr/bin/env python3
import sys
import os
import subprocess
import threading
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                               QHBoxLayout, QLabel, QComboBox, QPushButton, 
                               QSlider, QCheckBox, QColorDialog, QFrame)
from PySide6.QtCore import Qt, Signal, QObject
from PySide6.QtGui import QColor, QFont

class WorkerSignals(QObject):
    finished = Signal()

class ApolloApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Apollo61 Command Center")
        self.setFixedSize(450, 680)
        
        self.current_r = 255
        self.current_g = 0
        self.current_b = 0
        self.is_applying = False
        
        self.signals = WorkerSignals()
        self.signals.finished.connect(self.on_sync_complete)
        
        self.setup_ui()
        self.apply_theme()
        
    def setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(30, 30, 30, 30)
        main_layout.setSpacing(20)
        
        # Header
        header_layout = QVBoxLayout()
        header_layout.setSpacing(5)
        title = QLabel("APOLLO61")
        title.setObjectName("titleLabel")
        title.setAlignment(Qt.AlignCenter)
        
        subtitle = QLabel("macOS Native Driver")
        subtitle.setObjectName("subtitleLabel")
        subtitle.setAlignment(Qt.AlignCenter)
        
        header_layout.addWidget(title)
        header_layout.addWidget(subtitle)
        main_layout.addLayout(header_layout)
        
        # Separator
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setFrameShadow(QFrame.Sunken)
        line.setObjectName("separator")
        main_layout.addWidget(line)
        
        # Mode Selection
        mode_layout = QVBoxLayout()
        mode_layout.setSpacing(8)
        mode_label = QLabel("Lighting Mode")
        mode_label.setObjectName("sectionLabel")
        
        self.mode_combo = QComboBox()
        self.modes = [
            ("Mode 1: Static", 1), ("Mode 2: Single On (Reactive)", 2),
            ("Mode 3: Single Off", 3), ("Mode 4: Glittering", 4),
            ("Mode 5: Falling", 5), ("Mode 6: Colourful", 6),
            ("Mode 7: Breath", 7), ("Mode 8: Spectrum", 8),
            ("Mode 9: Outward", 9), ("Mode 10: Scrolling", 10),
            ("Mode 11: Rolling", 11), ("Mode 12: Rotating", 12),
            ("Mode 13: Explode", 13), ("Mode 14: Launch", 14),
            ("Mode 15: Ripples", 15), ("Mode 16: Flowing", 16),
            ("Mode 17: Pulsating", 17), ("Mode 18: Tilt", 18),
            ("Mode 19: Shuttle", 19),
        ]
        for name, _ in self.modes:
            self.mode_combo.addItem(name)
        self.mode_combo.currentIndexChanged.connect(self.trigger_sync)
        
        mode_layout.addWidget(mode_label)
        mode_layout.addWidget(self.mode_combo)
        main_layout.addLayout(mode_layout)
        
        # Color Selection
        color_layout = QHBoxLayout()
        color_label = QLabel("Global Color")
        color_label.setObjectName("sectionLabel")
        
        self.color_preview = QFrame()
        self.color_preview.setFixedSize(50, 30)
        self.color_preview.setStyleSheet("background-color: #ff0000; border-radius: 4px;")
        
        self.color_btn = QPushButton("Choose Color")
        self.color_btn.clicked.connect(self.pick_color)
        
        color_layout.addWidget(color_label)
        color_layout.addStretch()
        color_layout.addWidget(self.color_preview)
        color_layout.addWidget(self.color_btn)
        main_layout.addLayout(color_layout)
        
        # Sliders
        slider_layout = QVBoxLayout()
        slider_layout.setSpacing(15)
        
        # Brightness
        bright_box = QVBoxLayout()
        bright_box.setSpacing(5)
        self.bright_label = QLabel("Brightness: 100%")
        self.bright_label.setObjectName("sectionLabel")
        self.bright_slider = QSlider(Qt.Horizontal)
        self.bright_slider.setRange(0, 15)
        self.bright_slider.setValue(15)
        self.bright_slider.valueChanged.connect(self.update_bright_label)
        self.bright_slider.sliderReleased.connect(self.trigger_sync)
        bright_box.addWidget(self.bright_label)
        bright_box.addWidget(self.bright_slider)
        
        # Speed
        speed_box = QVBoxLayout()
        speed_box.setSpacing(5)
        self.speed_label = QLabel("Animation Speed: 100%")
        self.speed_label.setObjectName("sectionLabel")
        self.speed_slider = QSlider(Qt.Horizontal)
        self.speed_slider.setRange(0, 10)
        self.speed_slider.setValue(10)
        self.speed_slider.valueChanged.connect(self.update_speed_label)
        self.speed_slider.sliderReleased.connect(self.trigger_sync)
        speed_box.addWidget(self.speed_label)
        speed_box.addWidget(self.speed_slider)
        
        slider_layout.addLayout(bright_box)
        slider_layout.addLayout(speed_box)
        main_layout.addLayout(slider_layout)
        
        # Rainbow Checkbox
        self.rainbow_check = QCheckBox("Force Rainbow Cycle (Overrides Color)")
        self.rainbow_check.stateChanged.connect(self.trigger_sync)
        main_layout.addWidget(self.rainbow_check)
        
        main_layout.addStretch()
        
        # Status Bar
        self.status_lbl = QLabel("● Keyboard Ready")
        self.status_lbl.setObjectName("statusReady")
        self.status_lbl.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(self.status_lbl)

    def apply_theme(self):
        # Modern Dark Theme QSS
        qss = """
        QMainWindow {
            background-color: #121212;
        }
        QLabel {
            color: #E0E0E0;
            font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif;
            font-size: 13px;
        }
        QLabel#titleLabel {
            font-size: 28px;
            font-weight: bold;
            color: #00E5FF;
            letter-spacing: 2px;
        }
        QLabel#subtitleLabel {
            font-size: 12px;
            color: #888888;
            margin-bottom: 10px;
        }
        QLabel#sectionLabel {
            font-size: 14px;
            font-weight: 600;
            color: #FFFFFF;
        }
        QFrame#separator {
            background-color: #2A2A2A;
            max-height: 1px;
        }
        QComboBox {
            background-color: #1E1E1E;
            color: #FFFFFF;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox:hover {
            border: 1px solid #00E5FF;
        }
        QComboBox QAbstractItemView {
            background-color: #1E1E1E;
            color: #FFFFFF;
            selection-background-color: #00E5FF;
            selection-color: #000000;
            border: 1px solid #333333;
        }
        QPushButton {
            background-color: #2A2A2A;
            color: #FFFFFF;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #3A3A3A;
            border: 1px solid #00E5FF;
        }
        QPushButton:pressed {
            background-color: #00E5FF;
            color: #000000;
        }
        QSlider::groove:horizontal {
            border: none;
            height: 6px;
            background: #2A2A2A;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #00E5FF;
            border: 2px solid #121212;
            width: 16px;
            height: 16px;
            margin: -6px 0;
            border-radius: 8px;
        }
        QSlider::handle:horizontal:hover {
            background: #FFFFFF;
        }
        QSlider::sub-page:horizontal {
            background: #00E5FF;
            border-radius: 3px;
        }
        QCheckBox {
            font-size: 13px;
            color: #E0E0E0;
            spacing: 10px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #444444;
            background: #1E1E1E;
        }
        QCheckBox::indicator:checked {
            background: #00E5FF;
            border: 1px solid #00E5FF;
        }
        QLabel#statusReady {
            font-size: 14px;
            font-weight: bold;
            color: #00FF66;
            padding: 10px;
            background-color: #1A2E1A;
            border-radius: 8px;
        }
        QLabel#statusSyncing {
            font-size: 14px;
            font-weight: bold;
            color: #00E5FF;
            padding: 10px;
            background-color: #1A2833;
            border-radius: 8px;
        }
        """
        self.setStyleSheet(qss)
        
    def update_bright_label(self, value):
        percent = int((value / 15) * 100)
        self.bright_label.setText(f"Brightness: {percent}%")
        
    def update_speed_label(self, value):
        percent = int((value / 10) * 100)
        self.speed_label.setText(f"Animation Speed: {percent}%")
        
    def pick_color(self):
        color = QColorDialog.getColor(QColor(self.current_r, self.current_g, self.current_b), self, "Choose Lighting Color")
        if color.isValid():
            self.current_r = color.red()
            self.current_g = color.green()
            self.current_b = color.blue()
            hex_color = color.name()
            self.color_preview.setStyleSheet(f"background-color: {hex_color}; border-radius: 4px;")
            
            self.rainbow_check.blockSignals(True)
            self.rainbow_check.setChecked(False)
            self.rainbow_check.blockSignals(False)
            self.trigger_sync()

    def trigger_sync(self):
        if self.is_applying:
            return
            
        self.is_applying = True
        self.status_lbl.setObjectName("statusSyncing")
        self.status_lbl.setText("● Syncing to Device...")
        self.status_lbl.style().unpolish(self.status_lbl)
        self.status_lbl.style().polish(self.status_lbl)
        
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
        self.status_lbl.setObjectName("statusReady")
        self.status_lbl.setText("● Sync Complete")
        self.status_lbl.style().unpolish(self.status_lbl)
        self.status_lbl.style().polish(self.status_lbl)

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    app = QApplication(sys.argv)
    window = ApolloApp()
    window.show()
    sys.exit(app.exec())
