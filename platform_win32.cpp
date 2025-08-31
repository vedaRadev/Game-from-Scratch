#include <stdint.h>
#include <windows.h>

static bool global_game_is_running = false;

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch (message) {
        case WM_CLOSE: // TODO(ryan): handle WM_CLOSE as an error and recreate the window
        case WM_DESTROY:
            global_game_is_running = false;
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

    // TODO(ryan): Create a ScreenBuffer (or similarly named) struct to hold all this information in.
    // Pixels are stored as 0x00RRGGBB with windows
    const int bytes_per_pixel = 4; // 1 byte per color component (RGB) + 1 byte padding (align to 32-bit boundary)
    BITMAPINFO bitmap_info = {
        .bmiHeader = BITMAPINFOHEADER {
            .biSize = sizeof(BITMAPINFOHEADER),
            .biWidth = 16,
            // NOTE(ryan): When biHeight is negative, windows treats this as a TOPDOWN buffer.
            // i.e. (0, 0) is the upper-left corner of the image. 
            .biHeight = -16,
            .biPlanes = 1,
            .biBitCount = bytes_per_pixel * 8,
            .biCompression = BI_RGB,
        },
    };
    DWORD buffer_size = 16 * 16 * 4;
    char *buffer = (char *)VirtualAlloc(
        0,
        buffer_size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
    {
        uint8_t *row = (uint8_t *)buffer;
        for (int y = 0; y < 16; y++) {
            uint32_t *pixel = (uint32_t *)row;
            for (int x = 0; x < 16; x++) {
                *pixel = 0x00FF0000;
                pixel++;
            }
            row += (16 * bytes_per_pixel);
        }
    }

    // TODO(ryan): we need to be able to set a target framerate and sleep (or
    // spin, if we can't make our sleep granular) if we process everything too
    // fast.
    global_game_is_running = true;
    while (global_game_is_running) {
        MSG window_message;
        while (PeekMessage(&window_message, 0, 0, 0, PM_REMOVE)) {
            switch (window_message.message) {
                case WM_QUIT:
                    global_game_is_running = false;
                    break;

                default:
                    TranslateMessage(&window_message);
                    DispatchMessage(&window_message);
                    break;
            }
        }

        RECT client_rect;
        GetClientRect(game_window, &client_rect);
        int client_width = client_rect.right - client_rect.left;
        int client_height = client_rect.bottom - client_rect.top;
        HDC device_context = GetDC(game_window);
        // TODO(ryan): only clear the parts of the screen we AREN'T using
        // e.g. if we're drawing to only a portion of the screen and have black bars
        // (think 4:3 fullscreen on a 16:9 monitor)
        // PatBlt(device_context, 0, 0, client_width, client_height, BLACKNESS);
        StretchDIBits(
            device_context,
            0, 0, client_width, client_height,
            0, 0, 16, 16,
            buffer,
            &bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY
        );
        ReleaseDC(game_window, device_context);
    }

    return 0;
}
