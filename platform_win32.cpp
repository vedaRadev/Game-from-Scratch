#include <windows.h>
#include <stdint.h>
#include "platform.h"

struct Win32OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    uint16_t width;
    uint16_t height;
    uint8_t bytes_per_pixel;
};

struct GameCode {
    HMODULE dll;
    FILETIME dll_last_write_time;
    GameUpdateAndRender *update_and_render;
};

static bool global_game_is_running = false;
static Win32OffscreenBuffer win32_offscreen_buffer;

// TODO(ryan): Build custom wstring type that allows for things like slices?

// TODO(ryan): Testing
void copy_wstring(wchar_t *src, size_t src_len, wchar_t *dst, size_t dst_len) {
    for (size_t i = 0; i < src_len && i < dst_len; i++) {
        *dst++ = *src++;
    }
}

// TODO(ryan): Testing
// TODO(ryan): Probably a way to make this faster if needed. Initial thought is to maybe use a
// single loop to copy both s1 and s2 at once into the proper positions at dst? Maybe it'll increase
// a little bit of ILP if the compiler doesn't do it for us?
void concat_wstrings(
    const wchar_t *s1, size_t s1_len,
    const wchar_t *s2, size_t s2_len,
    wchar_t *dst, size_t dst_len
)
{
    size_t copied = 0;
    for (int i = 0; i < s1_len && copied < dst_len; i++, copied++) {
        *dst++ = *s1++;
        copied++;
    }
    for (int i = 0; i < s2_len && copied < dst_len; i++, copied++) {
        *dst++ = *s2++;
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch (message) {
        case WM_CLOSE: // TODO(ryan): handle WM_CLOSE as an error and recreate the window
        case WM_DESTROY:
            global_game_is_running = false;
            break;

        default:
            result = DefWindowProcW(window, message, w_param, l_param);
            break;
    }

    return result;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show) {
    WNDCLASSW game_window_class = {
        .lpfnWndProc = window_proc,
        .hInstance = instance,
        // TODO(ryan): Better window class name
        .lpszClassName = L"GameWindowClass",
    };
    if (!RegisterClassW(&game_window_class)) {
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK);
        return 1;
    }

    // TODO(ryan): clean up the following section
    // FIXME(ryan): do NOT use MAX_PATH. It has a hard limit of 260 characters and will often fail
    // if people are installing the game deep in their filesystem. A solution for this is to use
    // dynamically-sized buffers but I'll have to write those myself later on.
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    wchar_t *one_past_last_slash = NULL;
    // SAFETY: This works because the bit pattern for L'\\' will never appear in a surrogate pair.
    // The value of L'\\' is 0x005C.
    // Surrogate pair bit patterns range from [D800, DBFF] (high) and [DC00, DFFF].
    for (wchar_t *scan = exe_path; *scan; scan++) {
        if (*scan == L'\\') one_past_last_slash = scan + 1;
    }
    if (!one_past_last_slash) {
        // TODO(ryan): show a message or something then quit
        // Maybe assert here?
        // Can this ever even fail? Can we have a case where there's no backslash???
        return 1;
    }
    wchar_t game_dll_path[MAX_PATH];
    // TODO(ryan): better name for this? Maybe game_dev_dll_path?
    wchar_t game_temp_dll_path[MAX_PATH];
    concat_wstrings(
        exe_path, one_past_last_slash - exe_path,
        L"game.dll", sizeof(L"game.dll"),
        game_dll_path, sizeof(game_dll_path)
    );
    concat_wstrings(
        exe_path, one_past_last_slash - exe_path,
        L"game_temp.dll", sizeof(L"game_temp.dll"),
        game_temp_dll_path, sizeof(game_temp_dll_path)
    );
    // TODO(ryan): check if the game dll exists, abort if not?
    CopyFileW(game_dll_path, game_temp_dll_path, false);

    GameCode game_code;
    // TODO(ryan): pull out into function
    // Loading the library and setting up the functions we'll be calling
    game_code.dll = LoadLibraryW(game_temp_dll_path);
    if (!game_code.dll) {
        MessageBoxW(NULL, L"Failed to load game dll", L"Error", MB_OK);
        return 1;
    }
    game_code.update_and_render = (GameUpdateAndRender *)GetProcAddress(game_code.dll, "update_and_render");
    if (!game_code.update_and_render) {
        MessageBoxW(NULL, L"Failed to find game update_and_render function", L"Error", MB_OK);
        return 1;
    }
    {
        // Getting write time of the dll
        WIN32_FILE_ATTRIBUTE_DATA game_dll_file_attributes;
        GetFileAttributesExW(game_dll_path, GetFileExInfoStandard, &game_dll_file_attributes);
        game_code.dll_last_write_time = game_dll_file_attributes.ftLastWriteTime;
    }

    // TODO(ryan): check if there are different options we want to use for anything
    HWND game_window = CreateWindowExW(
        0,
        game_window_class.lpszClassName,
        // TODO(ryan): different name
        L"Game From Scratch",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL
    );
    if (game_window == NULL) {
        MessageBoxW(NULL, L"Failed to create game window", L"Error", MB_OK);
        return 1;
    }
    ShowWindow(game_window, cmd_show);

    // Pixels are stored as 0x00RRGGBB with windows
    win32_offscreen_buffer = {
        .width = 16,
        .height = 16,
        .bytes_per_pixel = 4, // 1 byte per color component (RGB) + 1 byte padding (align to 32-bit boundary)
    };
    // TODO(ryan): check if this fails and handle gracefully
    win32_offscreen_buffer.memory = VirtualAlloc(
        0,
        win32_offscreen_buffer.width * win32_offscreen_buffer.height * win32_offscreen_buffer.bytes_per_pixel,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
    win32_offscreen_buffer.info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    win32_offscreen_buffer.info.bmiHeader.biWidth = win32_offscreen_buffer.width;
    // NOTE(ryan): When biHeight is negative, windows treats this as a TOPDOWN buffer.
    // i.e. (0, 0) is the upper-left corner of the image. 
    win32_offscreen_buffer.info.bmiHeader.biHeight = -win32_offscreen_buffer.height;
    win32_offscreen_buffer.info.bmiHeader.biPlanes = 1;
    win32_offscreen_buffer.info.bmiHeader.biBitCount = win32_offscreen_buffer.bytes_per_pixel * 8;
    win32_offscreen_buffer.info.bmiHeader.biCompression = BI_RGB;

    // TODO(ryan): we need to be able to set a target framerate and sleep (or
    // spin, if we can't make our sleep granular) if we process everything too
    // fast.
    global_game_is_running = true;
    while (global_game_is_running) {
        {
            WIN32_FILE_ATTRIBUTE_DATA dll_attribs;
            GetFileAttributesExW(game_dll_path, GetFileExInfoStandard, &dll_attribs);
            if (CompareFileTime(&dll_attribs.ftLastWriteTime, &game_code.dll_last_write_time) != 0) {
                FreeLibrary(game_code.dll);
                CopyFileW(game_dll_path, game_temp_dll_path, false);
                game_code = {};
                // TODO(ryan): pull out into function
                game_code.dll = LoadLibraryW(game_temp_dll_path);
                if (!game_code.dll) {
                    MessageBoxW(NULL, L"Failed to load game dll", L"Error", MB_OK);
                    return 1;
                }
                game_code.update_and_render = (GameUpdateAndRender *)GetProcAddress(game_code.dll, "update_and_render");
                if (!game_code.update_and_render) {
                    MessageBoxW(NULL, L"Failed to find game update_and_render function", L"Error", MB_OK);
                    return 1;
                }
            }
        }

        MSG window_message;
        while (PeekMessageW(&window_message, 0, 0, 0, PM_REMOVE)) {
            switch (window_message.message) {
                case WM_QUIT:
                    global_game_is_running = false;
                    break;

                default:
                    TranslateMessage(&window_message);
                    DispatchMessageW(&window_message);
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
        GameOffscreenBuffer buf;
        buf.memory = win32_offscreen_buffer.memory;
        buf.width = win32_offscreen_buffer.width;
        buf.height = win32_offscreen_buffer.height;
        buf.bytes_per_pixel = win32_offscreen_buffer.bytes_per_pixel;
        // TODO(ryan): function pointer null check!
        game_code.update_and_render(&buf);
        StretchDIBits(
            device_context,
            0, 0, client_width, client_height,
            0, 0, win32_offscreen_buffer.width, win32_offscreen_buffer.height,
            win32_offscreen_buffer.memory,
            &win32_offscreen_buffer.info,
            DIB_RGB_COLORS,
            SRCCOPY
        );
        ReleaseDC(game_window, device_context);
    }

    return 0;
}
