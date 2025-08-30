#include <windows.h>

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    return DefWindowProc(hwnd, msg, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show) {
    WNDCLASS game_window_class = {
        .lpfnWndProc = window_proc,
        .hInstance = instance,
        // TODO(ryan): Better window class name
        .lpszClassName = "GameWindowClass",
    };
    if (!RegisterClassA(&game_window_class)) {
        MessageBoxA(NULL, "Failed to register window class", "Error", MB_OK);
        return 1;
    }

    // TODO(ryan): check if there are different options we want to use for anything
    HWND game_window = CreateWindowExA(
        0,
        game_window_class.lpszClassName,
        // TODO(ryan): different name
        "Game From Scratch",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL
    );
    if (game_window == NULL) {
        MessageBoxA(NULL, "Failed to create game window", "Error", MB_OK);
        return 1;
    }
    ShowWindow(game_window, cmd_show);

    bool game_is_running = true;
    // TODO(ryan): we need to be able to set a target framerate and sleep (or
    // spin, if we can't make our sleep granular) if we process everything too
    // fast.
    while (game_is_running) {
        MSG window_message;
        while (PeekMessageA(&window_message, 0, 0, 0, PM_REMOVE)) {
            switch (window_message.message) {
                case WM_QUIT:
                    game_is_running = false;
                    break;

                default:
                    TranslateMessage(&window_message);
                    DispatchMessageA(&window_message);
                    break;
            }
        }
    }
    

    return 0;
}
