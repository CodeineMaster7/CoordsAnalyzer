#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <string>
#include <commctrl.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_HOTKEY_SAVE 1
#define ID_BTN_CLOSE 2
#define ID_BTN_MINIMIZE 3
#define IDT_UPDATE_COORDS 1
#define IDT_CLOSE_SETTINGS 2
#define IDT_CLOSE_MSGBOX 3

HWND hwnd, settingsHwnd;
HWND hMsgWnd = NULL; // окно для сообщения
NOTIFYICONDATA nid = {};
HICON hIcon;
UINT saveColorHotkey = MOD_CONTROL | MOD_SHIFT;
UINT saveColorKey = 'C';

// Храним текущие координаты курсора
POINT currentPoint = { 0, 0 };

// Загружаем иконку из PNG-файла
HICON LoadPngIcon(const wchar_t* filename) {
    Gdiplus::Bitmap bitmap(filename);
    HICON hIcon = NULL;
    bitmap.GetHICON(&hIcon);
    return hIcon;
}

// Обновляем координаты курсора
void updateCoordinates() {
    static DWORD lastTrayUpdate = 0;
    
    // Получаем координаты курсора
    GetCursorPos(&currentPoint);

    DWORD now = GetTickCount();
    if (now - lastTrayUpdate > 1000) { // Обновляем трей не чаще раза в секунду
        char tooltip[32];
        sprintf(tooltip, "X: %d, Y: %d", currentPoint.x, currentPoint.y);
        strncpy(nid.szTip, tooltip, sizeof(nid.szTip)-1); 
        Shell_NotifyIcon(NIM_MODIFY, &nid);
        lastTrayUpdate = now;
    }

    // Перемещаем окно рядом с курсором
    SetWindowPos(hwnd, HWND_TOPMOST, currentPoint.x + 20, currentPoint.y, 120, 50, SWP_NOZORDER | SWP_NOSIZE | SWP_NOREDRAW);
    InvalidateRect(hwnd, NULL, FALSE);
}

// Создаем иконку в трее
void addTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    hIcon = LoadPngIcon(L"coordspicker.png");  // Загружаем иконку
    nid.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    strcpy_s(nid.szTip, "Coordinates Analyzer");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// Удаляем иконку из трея
void removeTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (hIcon) DestroyIcon(hIcon);
}

LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            DrawTextA(hdc, "Coordinates saved!", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            EndPaint(hwnd, &ps);
        } break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BTN_CLOSE) {
                PostQuitMessage(0);
            } else if (LOWORD(wParam) == ID_BTN_MINIMIZE) {
                ShowWindow(hwnd, SW_MINIMIZE);
            }
            break;
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CopyTextToClipboard(const char* text) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        size_t len = strlen(text) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            memcpy(GlobalLock(hMem), text, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

// Обработчик сообщений главного окна
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_HOTKEY:
            if (wParam == ID_HOTKEY_SAVE) {
                char coordsText[32];
                sprintf(coordsText, "X: %d, Y: %d", currentPoint.x, currentPoint.y);
                CopyTextToClipboard(coordsText);
                
                // Создаем окно для сообщения вместо стандартного MessageBox
                hMsgWnd = CreateWindowExA(WS_EX_TOPMOST, "MyMsgBox", "Info",
                                            WS_POPUP | WS_BORDER | WS_VISIBLE,
                                            CW_USEDEFAULT, CW_USEDEFAULT, 200, 100,
                                            hwnd, NULL, GetModuleHandle(NULL), NULL);
                // Центрируем окно на экране:
                RECT rc;
                GetWindowRect(hMsgWnd, &rc);
                int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
                int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
                SetWindowPos(hMsgWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                        
                SetTimer(hwnd, IDT_CLOSE_MSGBOX, 1000, NULL); // Закрыть сообщение через 1 секунду
                SetTimer(hwnd, IDT_CLOSE_SETTINGS, 2000, NULL); // Если нужно скрыть настройки
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
        
            RECT rect;
            GetClientRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
        
            // Заполняем окно белым фоном
            HBRUSH hBrush = CreateSolidBrush(RGB(255,255,255));
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        
            // Рисуем текст с координатами
            char coordsText[32];
            sprintf(coordsText, "X: %d, Y: %d", currentPoint.x, currentPoint.y);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0,0,0));
            DrawTextA(hdc, coordsText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
            EndPaint(hwnd, &ps);
        } break;

        case WM_TIMER:
            switch (wParam) {
                case IDT_CLOSE_MSGBOX:
                    if (hMsgWnd) {
                        DestroyWindow(hMsgWnd);
                        hMsgWnd = NULL;
                    }
                    KillTimer(hwnd, IDT_CLOSE_MSGBOX);
                    break;
                
                case IDT_CLOSE_SETTINGS:
                    ShowWindow(settingsHwnd, SW_HIDE);
                    KillTimer(hwnd, IDT_CLOSE_SETTINGS);
                    break;
                
                case IDT_UPDATE_COORDS:
                    updateCoordinates();
                    break;
            }
            return 0;
    
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, 1, "Settings");
                AppendMenu(hMenu, MF_STRING, 2, "Exit Analyzer");
                POINT p;
                GetCursorPos(&p);
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
                if (cmd == 1) ShowWindow(settingsHwnd, SW_SHOW);
                if (cmd == 2) PostQuitMessage(0);
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, IDT_UPDATE_COORDS);
            removeTrayIcon();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void createSettingsWindow(HINSTANCE hInstance) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "SettingsWindow";
    RegisterClass(&wc);

    settingsHwnd = CreateWindow("SettingsWindow", "Settings", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, NULL, NULL, hInstance, NULL);

    CreateWindow("BUTTON", "Close", WS_VISIBLE | WS_CHILD, 50, 100, 80, 30, settingsHwnd, (HMENU)ID_BTN_CLOSE, hInstance, NULL);
    CreateWindow("BUTTON", "Minimize", WS_VISIBLE | WS_CHILD, 150, 100, 80, 30, settingsHwnd, (HMENU)ID_BTN_MINIMIZE, hInstance, NULL);
}

// Главная функция
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASS mwc = {};
    mwc.lpfnWndProc = MsgWndProc;
    mwc.hInstance = hInstance;
    mwc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    mwc.lpszClassName = "MyMsgBox";
    RegisterClass(&mwc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ColorAnalyzer";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "ColorAnalyzer", "Coordinates Analyzer", WS_POPUP,
                          100, 100, 120, 50, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    addTrayIcon(hwnd);
    SetTimer(hwnd, IDT_UPDATE_COORDS, 0, NULL); // Обновляем координаты
    RegisterHotKey(hwnd, ID_HOTKEY_SAVE, saveColorHotkey, saveColorKey);

    createSettingsWindow(hInstance);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}