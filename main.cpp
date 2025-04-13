#include <windows.h>
#include <unordered_map>

using namespace std;

struct KeyState {
    bool registered = false;
    bool keydown = false;
    int group;
    bool simulated = false;
    bool flashtap = false;
    int flashtaptimer = 0;
};

struct GroupState {
    int previousKey = 0;
    int activeKey = 0;
};

unordered_map<int, GroupState> GroupInfo;
unordered_map<int, KeyState> KeyInfo;

HHOOK hHook = NULL;
HANDLE hMutex = NULL;
bool islocked = false; 
const int flashtap = 0; 
HWND targetWindow = NULL; 

// Function declarations
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
void SendKey(int target, bool keyDown);
void InitializeKeys();
bool SetupInstance();
void RunMessageLoop();

void InitializeKeys() {
    KeyInfo[0x41].registered = true;
    KeyInfo[0x41].flashtap = true;
    KeyInfo[0x41].group = 1; 

    KeyInfo[0x44].registered = true;
    KeyInfo[0x44].flashtap = true;
    KeyInfo[0x44].group = 1; 
}

bool SetupInstance() {
    hMutex = CreateMutex(NULL, TRUE, TEXT("SnapKeyMutex"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return false; 
    }

    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (hHook == NULL) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return false;
    }

    return true;
}

void RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool isSimulatedKeyEvent(DWORD flags) { return flags & 0x10; }

void SendKey(int targetKey, bool keyDown) {
    if (targetWindow == NULL) {
        targetWindow = GetForegroundWindow();
    }

    UINT message = keyDown ? WM_KEYDOWN : WM_KEYUP;
    PostMessage(targetWindow, message, targetKey, 0);
}

void handleKeyDown(int keyCode) {
    KeyState& currentKeyInfo = KeyInfo[keyCode];
    GroupState& currentGroupInfo = GroupInfo[currentKeyInfo.group];

    if (!currentKeyInfo.keydown) {
        currentKeyInfo.keydown = true;

        if (currentKeyInfo.flashtap) {
            currentKeyInfo.flashtaptimer = GetTickCount();
        }
        SendKey(keyCode, true);

        if (currentGroupInfo.activeKey == 0 ||
            currentGroupInfo.activeKey == keyCode) {
            currentGroupInfo.activeKey = keyCode;
        }
        else {
            currentGroupInfo.previousKey = currentGroupInfo.activeKey;
            currentGroupInfo.activeKey = keyCode;
            SendKey(currentGroupInfo.previousKey, false);
        }
    }
}

void handleKeyUp(int keyCode) {
    KeyState& currentKeyInfo = KeyInfo[keyCode];
    GroupState& currentGroupInfo = GroupInfo[currentKeyInfo.group];

    if (currentGroupInfo.previousKey == keyCode && !currentKeyInfo.keydown) {
        currentGroupInfo.previousKey = 0;
    }

    if (currentKeyInfo.keydown) {
        currentKeyInfo.keydown = false;

        if (currentKeyInfo.flashtap) {
            DWORD timeDiff = GetTickCount() - currentKeyInfo.flashtaptimer;
            if (timeDiff <= flashtap) {
                SendKey(keyCode, true);
                SendKey(keyCode, false);
            }
        }

        if (currentGroupInfo.activeKey == keyCode &&
            currentGroupInfo.previousKey != 0) {
            SendKey(keyCode, false);

            currentGroupInfo.activeKey = currentGroupInfo.previousKey;
            currentGroupInfo.previousKey = 0;

            SendKey(currentGroupInfo.activeKey, true);
        }
        else {
            currentGroupInfo.previousKey = 0;
            if (currentGroupInfo.activeKey == keyCode)
                currentGroupInfo.activeKey = 0;
            SendKey(keyCode, false);
        }
    }
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!islocked && nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        if (!isSimulatedKeyEvent(pKeyBoard->flags)) {
            if (KeyInfo[pKeyBoard->vkCode].registered) {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                    handleKeyDown(pKeyBoard->vkCode);
                if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                    handleKeyUp(pKeyBoard->vkCode);
                return 1;
            }
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

int main() {
    if (!SetupInstance()) return 1; 

    InitializeKeys();
    RunMessageLoop(); 

    UnhookWindowsHookEx(hHook);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return 0;
}
