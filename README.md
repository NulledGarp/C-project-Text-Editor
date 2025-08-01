# C Text Editor

A simple Windows text editor written in pure C using the Win32 API.  
Features basic text editing, word counting, theme toggle, syntax highlighting for keywords, and clipboard support.

## Features

- Open and Save `.txt` files
- Light and Dark mode toggle
- Word counter display
- Basic syntax highlighting for C keywords
- Caret navigation and mouse text selection
- Copy (Ctrl+C) and Paste (Ctrl+V) support

## Build Instructions

### Prerequisites
- Windows OS
- A C compiler like `cl.exe` (MSVC) or `gcc` (MinGW)

### Compile with MinGW (example):
```bash
gcc editor.c -o editor.exe -mwindows
