#!/usr/bin/env python3
import sys
import os
import subprocess
import threading
import webview

class Api:
    def __init__(self):
        self.is_applying = False

    def apply_settings(self, mode_id, r, g, b, bright, speed, rainbow):
        if self.is_applying:
            return
            
        self.is_applying = True
        
        args = [
            f"{mode_id:02x}", f"{r:02x}", f"{g:02x}",
            f"{b:02x}", f"{bright:02x}", f"{speed:02x}", f"{rainbow}"
        ]
        
        def push_to_keyboard():
            script_path = os.path.join(os.path.dirname(__file__), "set_color.py")
            subprocess.run([script_path] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.is_applying = False
            
        thread = threading.Thread(target=push_to_keyboard, daemon=True)
        thread.start()
        # Wait for thread to finish for pywebview promise to resolve properly
        thread.join()

if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    html_path = os.path.join(os.getcwd(), 'KeyCraft Pro', 'dist', 'index.html')
    
    if not os.path.exists(html_path):
        print(f"Error: Could not find {html_path}")
        print("Please run 'npm run build' inside the 'KeyCraft Pro' directory first.")
        sys.exit(1)

    api = Api()
    
    webview.create_window(
        'KeyCraft Pro Native Driver', 
        url=f'file://{html_path}',
        js_api=api,
        width=1100,
        height=750,
        min_size=(900, 600),
        background_color='#09090b',
        confirm_close=True
    )
    
    webview.start(debug=True)
