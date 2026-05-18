#!/usr/bin/env python3
import tkinter as tk
from tkinter import colorchooser, ttk
import subprocess
import os
import threading

class ApolloApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Apollo61 Command Center")
        self.root.geometry("450x600")
        self.root.configure(bg="#1E1E1E")
        self.root.resizable(False, False)
        
        # Configure TTK Styles for a modern dark theme
        style = ttk.Style()
        if "clam" in style.theme_names():
            style.theme_use("clam")
            
        style.configure("TFrame", background="#1E1E1E")
        style.configure("TLabel", background="#1E1E1E", foreground="#FFFFFF", font=("Helvetica Neue", 12))
        style.configure("Header.TLabel", font=("Helvetica Neue", 20, "bold"), foreground="#00C7FF")
        style.configure("SubHeader.TLabel", font=("Helvetica Neue", 10), foreground="#888888")
        style.configure("TCombobox", fieldbackground="#2D2D2D", background="#2D2D2D", foreground="#FFFFFF", borderwidth=0)
        style.configure("TScale", background="#1E1E1E", troughcolor="#333333", slidercolor="#00C7FF")
        style.configure("TCheckbutton", background="#1E1E1E", foreground="#FFFFFF", font=("Helvetica Neue", 12))
        style.map("TCheckbutton",
                  foreground=[('active', '#00C7FF')],
                  background=[('active', '#1E1E1E')])
        
        # Current color state
        self.current_r = 255
        self.current_g = 0
        self.current_b = 0
        
        # Main Container with Padding
        main_frame = ttk.Frame(root, padding=20)
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Header
        ttk.Label(main_frame, text="APOLLO61", style="Header.TLabel").pack(pady=(0, 0))
        ttk.Label(main_frame, text="macOS Native Driver", style="SubHeader.TLabel").pack(pady=(0, 20))
        
        # Mode Selection
        ttk.Label(main_frame, text="Lighting Mode").pack(anchor=tk.W)
        self.mode_var = tk.StringVar(value="1")
        
        modes = [
            ("Mode 1: Static", "1"), ("Mode 2: Single On (Reactive)", "2"),
            ("Mode 3: Single Off", "3"), ("Mode 4: Glittering", "4"),
            ("Mode 5: Falling", "5"), ("Mode 6: Colourful", "6"),
            ("Mode 7: Breath", "7"), ("Mode 8: Spectrum", "8"),
            ("Mode 9: Outward", "9"), ("Mode 10: Scrolling", "10"),
            ("Mode 11: Rolling", "11"), ("Mode 12: Rotating", "12"),
            ("Mode 13: Explode", "13"), ("Mode 14: Launch", "14"),
            ("Mode 15: Ripples", "15"), ("Mode 16: Flowing", "16"),
            ("Mode 17: Pulsating", "17"), ("Mode 18: Tilt", "18"),
            ("Mode 19: Shuttle", "19"),
        ]
        
        self.mode_combo = ttk.Combobox(main_frame, textvariable=self.mode_var, values=[m[0] for m in modes], state="readonly", width=30, font=("Helvetica Neue", 12))
        self.mode_combo.pack(fill=tk.X, pady=(5, 20))
        self.mode_combo.current(0)
        self.mode_map = {m[0]: m[1] for m in modes}
        self.mode_combo.bind("<<ComboboxSelected>>", lambda e: self.apply_settings())
        
        # Color Section
        color_frame = ttk.Frame(main_frame)
        color_frame.pack(fill=tk.X, pady=10)
        
        ttk.Label(color_frame, text="Global Color").pack(side=tk.LEFT)
        
        self.color_btn = tk.Button(color_frame, text="Pick Color", command=self.pick_color, bg="#333333", fg="#FFFFFF", font=("Helvetica Neue", 12), relief=tk.FLAT, highlightthickness=0, borderwidth=0, padx=10, pady=5)
        self.color_btn.pack(side=tk.RIGHT)
        
        self.color_preview = tk.Frame(color_frame, width=40, height=25, bg="#ff0000", relief=tk.FLAT)
        self.color_preview.pack(side=tk.RIGHT, padx=10)
        
        # Sliders Section
        ttk.Label(main_frame, text="Brightness").pack(anchor=tk.W, pady=(15, 0))
        self.bright_scale = ttk.Scale(main_frame, from_=0, to=15, orient=tk.HORIZONTAL)
        self.bright_scale.set(15)
        self.bright_scale.pack(fill=tk.X, pady=(5, 10))
        self.bright_scale.bind("<ButtonRelease-1>", lambda e: self.apply_settings())

        ttk.Label(main_frame, text="Animation Speed").pack(anchor=tk.W, pady=(10, 0))
        self.speed_scale = ttk.Scale(main_frame, from_=0, to=10, orient=tk.HORIZONTAL)
        self.speed_scale.set(10)
        self.speed_scale.pack(fill=tk.X, pady=(5, 20))
        self.speed_scale.bind("<ButtonRelease-1>", lambda e: self.apply_settings())
        
        # Settings
        self.rainbow_var = tk.IntVar(value=0)
        ttk.Checkbutton(main_frame, text="Force Rainbow Cycle (Overrides Color)", variable=self.rainbow_var, command=self.apply_settings).pack(anchor=tk.W, pady=10)
        
        # Status Bar
        status_frame = ttk.Frame(root)
        status_frame.pack(side=tk.BOTTOM, fill=tk.X)
        self.status_bg = tk.Frame(status_frame, bg="#2D2D2D", height=40)
        self.status_bg.pack(fill=tk.X)
        
        self.status_lbl = tk.Label(self.status_bg, text="● Keyboard Ready", font=("Helvetica Neue", 11, "bold"), fg="#00FF00", bg="#2D2D2D")
        self.status_lbl.pack(pady=10)
        
        self.is_applying = False
        
    def pick_color(self):
        color_code = colorchooser.askcolor(title="Choose Lighting Color")
        if color_code and color_code[0]:
            self.current_r, self.current_g, self.current_b = [int(c) for c in color_code[0]]
            hex_color = f"#{self.current_r:02x}{self.current_g:02x}{self.current_b:02x}"
            self.color_preview.config(bg=hex_color)
            self.rainbow_var.set(0)
            self.apply_settings()
            
    def apply_settings(self):
        if self.is_applying:
            return
            
        self.is_applying = True
        self.status_lbl.config(text="● Syncing to Device...", fg="#00C7FF")
        
        selected_mode_str = self.mode_var.get()
        mode_id = int(self.mode_map[selected_mode_str])
        bright = int(self.bright_scale.get())
        speed = int(self.speed_scale.get())
        is_rainbow = self.rainbow_var.get()
        
        args = [
            f"{mode_id:02x}", f"{self.current_r:02x}", f"{self.current_g:02x}",
            f"{self.current_b:02x}", f"{bright:02x}", f"{speed:02x}", f"{is_rainbow}"
        ]
        
        def push_to_keyboard():
            script_path = os.path.join(os.path.dirname(__file__), "set_color.py")
            subprocess.run([script_path] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.is_applying = False
            self.root.after(0, lambda: self.status_lbl.config(text="● Sync Complete", fg="#00FF00"))
            
        threading.Thread(target=push_to_keyboard, daemon=True).start()

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    root = tk.Tk()
    app = ApolloApp(root)
    root.mainloop()
