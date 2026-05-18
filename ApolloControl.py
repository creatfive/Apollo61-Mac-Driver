#!/usr/bin/env python3
import tkinter as tk
from tkinter import colorchooser
import subprocess
import os

class ApolloApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Apollo61 Mac Control")
        self.root.geometry("300x200")
        
        # Title Label
        tk.Label(root, text="Apollo61 Lighting Control", font=("Helvetica", 16, "bold")).pack(pady=20)
        
        # Color Preview Block
        self.color_preview = tk.Frame(root, width=100, height=50, bg="black", relief=tk.SUNKEN, borderwidth=2)
        self.color_preview.pack(pady=10)
        
        # Choose Color Button
        tk.Button(root, text="Pick Color & Apply", command=self.pick_color, font=("Helvetica", 14)).pack(pady=10)
        
    def pick_color(self):
        # Open macOS native color picker
        color_code = colorchooser.askcolor(title="Choose Lighting Color")
        
        if color_code and color_code[0]:
            # Convert to integers
            r, g, b = [int(c) for c in color_code[0]]
            
            # Update preview block
            hex_color = f"#{r:02x}{g:02x}{b:02x}"
            self.color_preview.config(bg=hex_color)
            
            # Format arguments for set_color.py
            r_hex = f"{r:02x}"
            g_hex = f"{g:02x}"
            b_hex = f"{b:02x}"
            
            # Call the script
            print(f"Applying color: {r_hex} {g_hex} {b_hex}...")
            
            # Disable button briefly while pushing
            script_path = os.path.join(os.path.dirname(__file__), "set_color.py")
            subprocess.run([script_path, r_hex, g_hex, b_hex])
            print("Done!")

if __name__ == "__main__":
    # Ensure we are running from the script's directory
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    root = tk.Tk()
    app = ApolloApp(root)
    root.mainloop()
