#include <windows.h>

static bool game_is_running = false;

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch (message) {
        case WM_CLOSE: // TODO(ryan): handle WM_CLOSE as an error and recreate the window
        case WM_DESTROY:
            game_is_running = false;
            break;

        default:
            result = DefWindowProc(window, message, w_param, l_param);
            break;
    }

    return result;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show) {
    WNDCLASS game_window_class = {
        .lpfnWndProc = window_proc,
        .hInstance = instance,
        // TODO(ryan): Better window class name
        .lpszClassName = TEXT("GameWindowClass"),
    };
    if (!RegisterClass(&game_window_class)) {
        MessageBox(NULL, TEXT("Failed to register window class"), TEXT("Error"), MB_OK);
        return 1;
    }

    // TODO(ryan): check if there are different options we want to use for anything
    HWND game_window = CreateWindowEx(
        0,
        game_window_class.lpszClassName,
        // TODO(ryan): different name
        TEXT("Game From Scratch"),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL
    );
    if (game_window == NULL) {
        MessageBox(NULL, TEXT("Failed to create game window"), TEXT("Error"), MB_OK);
        return 1;
    }
    ShowWindow(game_window, cmd_show);

    // TODO(ryan): we need to be able to set a target framerate and sleep (or
    // spin, if we can't make our sleep granular) if we process everything too
    // fast.
    game_is_running = true;
    while (game_is_running) {
        MSG window_message;
        while (PeekMessage(&window_message, 0, 0, 0, PM_REMOVE)) {
            switch (window_message.message) {
                case WM_QUIT:
                    game_is_running = false;
                    break;

                default:
                    TranslateMessage(&window_message);
                    DispatchMessage(&window_message);
                    break;
            }
        }
    }

    MessageBox(NULL, TEXT("Game is shutting down"), TEXT("Info"), MB_OK);

    return 0;
}
