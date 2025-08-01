
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

#define MAX_CHARS 65536

char textBuffer[MAX_CHARS];
int bufferLen = 0;
int caretX = 0, caretY = 0, lineHeight = 20;
int scrollPos = 0;
int windowWidth, windowHeight;

int selStart = -1, selEnd = -1;
int darkMode = 0;

char *keywords[] = {"int", "char", "return", "void", "if", "else", "for", "while", "break", "continue"};

COLORREF bgColorLight = RGB(255, 255, 255);
COLORREF textColorLight = RGB(160, 0, 0);
COLORREF keywordColorLight = RGB(110, 0, 0);

COLORREF bgColorDark = RGB(60, 0, 0);
COLORREF textColorDark = RGB(255, 200, 200);
COLORREF keywordColorDark = RGB(255, 255, 255);

void UpdateCaret(HWND hwnd);
void LoadFile(HWND hwnd);
void SaveFile(HWND hwnd);
void AddMenus(HWND hwnd);
int CountWords();
void ToggleTheme(HWND hwnd);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateCaret(hwnd, NULL, 2, lineHeight);
        ShowCaret(hwnd);
        break;

    case WM_CHAR:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wParam == 3) {
                if (selStart >= 0 && selEnd > selStart) {
                    int len = selEnd - selStart;
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
                    memcpy(GlobalLock(hMem), &textBuffer[selStart], len);
                    GlobalUnlock(hMem);
                    OpenClipboard(hwnd);
                    EmptyClipboard();
                    SetClipboardData(CF_TEXT, hMem);
                    CloseClipboard();
                }
            } else if (wParam == 22) {
                OpenClipboard(hwnd);
                HANDLE hData = GetClipboardData(CF_TEXT);
                if (hData) {
                    char* clipText = GlobalLock(hData);
                    int clipLen = strlen(clipText);
                    if (bufferLen + clipLen < MAX_CHARS) {
                        memmove(&textBuffer[caretY], clipText, clipLen);
                        bufferLen += clipLen;
                        textBuffer[bufferLen] = '\0';
                    }
                    GlobalUnlock(hData);
                }
                CloseClipboard();
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else if (wParam == 8) {
            if (bufferLen > 0) {
                bufferLen--;
                textBuffer[bufferLen] = '\0';
                caretX = max(0, caretX - 1);
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else if (wParam == 13) {
            if (bufferLen + 2 < MAX_CHARS) {
                textBuffer[bufferLen++] = '\r';
                textBuffer[bufferLen++] = '\n';
                textBuffer[bufferLen] = '\0';
                caretX = 0;
                caretY++;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else if (wParam == 9) {
            if (bufferLen + 4 < MAX_CHARS) {
                textBuffer[bufferLen++] = ' ';
                textBuffer[bufferLen++] = ' ';
                textBuffer[bufferLen++] = ' ';
                textBuffer[bufferLen++] = ' ';
                textBuffer[bufferLen] = '\0';
                caretX += 4;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else if (wParam >= 32 && wParam <= 126) {
            if (bufferLen + 1 < MAX_CHARS) {
                textBuffer[bufferLen++] = (char)wParam;
                textBuffer[bufferLen] = '\0';
                caretX++;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        UpdateCaret(hwnd);
        break;

    case WM_LBUTTONDOWN:
        selStart = caretY;
        SetCapture(hwnd);
        break;

    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) {
            selEnd = caretY + GET_SC_WPARAM(lParam) / lineHeight;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        break;

    case WM_MOUSEWHEEL:
        scrollPos -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        scrollPos = max(0, scrollPos);
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1: LoadFile(hwnd); break;
        case 2: SaveFile(hwnd); break;
        case 3: PostQuitMessage(0); break;
        case 4: ToggleTheme(hwnd); break;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HBRUSH bg = CreateSolidBrush(darkMode ? bgColorDark : bgColorLight);
        FillRect(hdc, &ps.rcPaint, bg);
        DeleteObject(bg);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, darkMode ? textColorDark : textColorLight);
        SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

        int y = -scrollPos * lineHeight;
        char *copy = _strdup(textBuffer);
        char *line = strtok(copy, "\n");
        while (line) {
            int isKeyword = 0;
            for (int i = 0; i < sizeof(keywords)/sizeof(keywords[0]); i++) {
                if (strstr(line, keywords[i])) {
                    isKeyword = 1;
                    break;
                }
            }
            SetTextColor(hdc, isKeyword ? (darkMode ? keywordColorDark : keywordColorLight)
                                        : (darkMode ? textColorDark : textColorLight));
            TextOutA(hdc, 10, y, line, strlen(line));
            y += lineHeight;
            line = strtok(NULL, "\n");
        }
        free(copy);

        // Word count
        char wordInfo[64];
        sprintf(wordInfo, "Words: %d", CountWords());
        SetTextColor(hdc, darkMode ? RGB(255, 255, 255) : RGB(150, 0, 0));
        TextOutA(hdc, 10, windowHeight - 20, wordInfo, strlen(wordInfo));

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_SETFOCUS:
        CreateCaret(hwnd, NULL, 2, lineHeight);
        ShowCaret(hwnd);
        UpdateCaret(hwnd);
        break;

    case WM_KILLFOCUS:
        HideCaret(hwnd);
        DestroyCaret();
        break;

    case WM_SIZE:
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ToggleTheme(HWND hwnd) {
    darkMode = !darkMode;
    InvalidateRect(hwnd, NULL, TRUE);
}

int CountWords() {
    int count = 0, inWord = 0;
    for (int i = 0; i < bufferLen; i++) {
        if ((textBuffer[i] >= 'a' && textBuffer[i] <= 'z') || (textBuffer[i] >= 'A' && textBuffer[i] <= 'Z')) {
            if (!inWord) {
                count++;
                inWord = 1;
            }
        } else {
            inWord = 0;
        }
    }
    return count;
}

void LoadFile(HWND hwnd) {
    OPENFILENAME ofn = {0};
    char filename[MAX_PATH] = "";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        HANDLE hFile = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD bytesRead;
            ReadFile(hFile, textBuffer, MAX_CHARS - 1, &bytesRead, NULL);
            textBuffer[bytesRead] = '\0';
            bufferLen = bytesRead;
            CloseHandle(hFile);
            caretX = caretY = 0;
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }
}

void SaveFile(HWND hwnd) {
    OPENFILENAME ofn = {0};
    char filename[MAX_PATH] = "";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn)) {
        HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten;
            WriteFile(hFile, textBuffer, bufferLen, &bytesWritten, NULL);
            CloseHandle(hFile);
        }
    }
}

void AddMenus(HWND hwnd) {
    HMENU hMenuBar = CreateMenu();
    HMENU hFile = CreateMenu();

    AppendMenu(hFile, MF_STRING, 1, "Open");
    AppendMenu(hFile, MF_STRING, 2, "Save");
    AppendMenu(hFile, MF_STRING, 4, "Toggle Theme");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, 3, "Exit");

    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFile, "File");
    SetMenu(hwnd, hMenuBar);
}

void UpdateCaret(HWND hwnd) {
    SetCaretPos(caretX * 8 + 10, caretY * lineHeight - scrollPos * lineHeight);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "TextEditor";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "C-project Text Editor",
                               WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                               CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
                               NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    AddMenus(hwnd);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
