#!/usr/bin/env python3
import tkinter as tk
from tkinter import colorchooser, ttk
import subprocess
import os
import threading

class ApolloApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Apollo61 Mac Control")
        self.root.geometry("400x500")
        
        # Current color state
        self.current_r = 255
        self.current_g = 0
        self.current_b = 0
        
        # Title Label
        tk.Label(root, text="Apollo61 Lighting Control", font=("Helvetica", 18, "bold")).pack(pady=15)
        
        # Mode Selection
        tk.Label(root, text="Lighting Mode:", font=("Helvetica", 14)).pack()
        self.mode_var = tk.StringVar(value="1")
        
        modes = [
            ("Mode 1: Static", "1"),
            ("Mode 2: Single On (Reactive)", "2"),
            ("Mode 3: Single Off", "3"),
            ("Mode 4: Glittering", "4"),
            ("Mode 5: Falling", "5"),
            ("Mode 6: Colourful", "6"),
            ("Mode 7: Breath", "7"),
            ("Mode 8: Spectrum", "8"),
            ("Mode 9: Outward", "9"),
            ("Mode 10: Scrolling", "10"),
            ("Mode 11: Rolling", "11"),
            ("Mode 12: Rotating", "12"),
            ("Mode 13: Explode", "13"),
            ("Mode 14: Launch", "14"),
            ("Mode 15: Ripples", "15"),
            ("Mode 16: Flowing", "16"),
            ("Mode 17: Pulsating", "17"),
            ("Mode 18: Tilt", "18"),
            ("Mode 19: Shuttle", "19"),
        ]
        
        self.mode_combo = ttk.Combobox(root, textvariable=self.mode_var, values=[m[0] for m in modes], state="readonly", width=25)
        self.mode_combo.pack(pady=5)
        self.mode_combo.current(0)
        self.mode_map = {m[0]: m[1] for m in modes}
        
        # Color Preview Block
        tk.Label(root, text="Current Color:", font=("Helvetica", 14)).pack(pady=(15, 0))
        self.color_preview = tk.Frame(root, width=150, height=40, bg="#ff0000", relief=tk.SUNKEN, borderwidth=2)
        self.color_preview.pack(pady=5)
        
        # Choose Color Button
        tk.Button(root, text="Pick Custom Color", command=self.pick_color, font=("Helvetica", 12)).pack(pady=5)
        
        # Brightness Slider
        tk.Label(root, text="Brightness (0 - 15):", font=("Helvetica", 12)).pack(pady=(15, 0))
        self.bright_scale = tk.Scale(root, from_=0, to=15, orient=tk.HORIZONTAL, length=200)
        self.bright_scale.set(15)
        self.bright_scale.pack()

        # Speed Slider
        tk.Label(root, text="Animation Speed (0 - 10):", font=("Helvetica", 12)).pack(pady=(10, 0))
        self.speed_scale = tk.Scale(root, from_=0, to=10, orient=tk.HORIZONTAL, length=200)
        self.speed_scale.set(10)
        self.speed_scale.pack()
        
        # Apply Button
        self.apply_btn = tk.Button(root, text="Apply to Keyboard", command=self.apply_settings, font=("Helvetica", 14, "bold"), bg="blue", fg="white")
        self.apply_btn.pack(pady=20)
        
    def pick_color(self):
        # Open macOS native color picker
        color_code = colorchooser.askcolor(title="Choose Lighting Color")
        
        if color_code and color_code[0]:
            self.current_r, self.current_g, self.current_b = [int(c) for c in color_code[0]]
            hex_color = f"#{self.current_r:02x}{self.current_g:02x}{self.current_b:02x}"
            self.color_preview.config(bg=hex_color)
            
    def apply_settings(self):
        # Disable button to prevent spamming
        self.apply_btn.config(state="disabled", text="Applying...")
        
        # Get Mode Hex
        selected_mode_str = self.mode_var.get()
        mode_id = int(self.mode_map[selected_mode_str])
        
        # Get Sliders
        bright = int(self.bright_scale.get())
        speed = int(self.speed_scale.get())
        
        # Format arguments
        args = [
            f"{mode_id:02x}",
            f"{self.current_r:02x}",
            f"{self.current_g:02x}",
            f"{self.current_b:02x}",
            f"{bright:02x}",
            f"{speed:02x}"
        ]
        
        print(f"Applying Mode {mode_id} | RGB: {self.current_r},{self.current_g},{self.current_b} | Brightness: {bright} | Speed: {speed}")
        
        # Run in a background thread so the UI doesn't freeze
        def push_to_keyboard():
            script_path = os.path.join(os.path.dirname(__file__), "set_color.py")
            subprocess.run([script_path] + args)
            print("Done pushing to keyboard!")
            
            # Re-enable button
            self.root.after(0, lambda: self.apply_btn.config(state="normal", text="Apply to Keyboard"))
            
        threading.Thread(target=push_to_keyboard, daemon=True).start()

if __name__ == "__main__":
    # Ensure we are running from the script's directory
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    root = tk.Tk()
    app = ApolloApp(root)
    root.mainloop()
