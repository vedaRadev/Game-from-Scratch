#include <windows.h>
#include "platform.h"

typedef struct OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    uint16_t width;
    uint16_t height;
    uint8_t bytes_per_pixel;
} OffscreenBuffer;

typedef struct GameCode {
    HMODULE dll;
    FILETIME dll_last_write_time;
    GameUpdateAndRender update_and_render;
} GameCode;

static bool game_is_running = false;
static OffscreenBuffer offscreen_buffer;
static uint64_t wall_clock_frequency; // Performance counter ticks per second

// TODO(mal): Build custom wstring type that allows for things like slices?

// TODO(mal): Testing
void copy_wstring(wchar_t *src, size_t src_len, wchar_t *dst, size_t dst_len) {
    for (size_t i = 0; i < src_len && i < dst_len; i++) {
        *dst++ = *src++;
    }
}

// TODO(mal): Testing
// TODO(mal): Probably a way to make this faster if needed. Initial thought is to maybe use a
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

GameCode load_game_code(const wchar_t *game_dll_path, const wchar_t *game_temp_dll_path, const wchar_t *game_dll_lock_path) {
    GameCode game_code = {};

    // NOTE(mal): Our build script should create a dummy file before building the game dll and
    // remove it after the dll build is finished. This guarantees that the OS has time to properly
    // update the game dll file contents in the file system cache before we try to load it.
    WIN32_FILE_ATTRIBUTE_DATA ignored;
    if (!GetFileAttributesExW(game_dll_lock_path, GetFileExInfoStandard, &ignored)) {
        WIN32_FILE_ATTRIBUTE_DATA game_dll_file_attributes;
        GetFileAttributesExW(game_dll_path, GetFileExInfoStandard, &game_dll_file_attributes);
        CopyFileW(game_dll_path, game_temp_dll_path, false);
        game_code.dll_last_write_time = game_dll_file_attributes.ftLastWriteTime;
        game_code.dll = LoadLibraryW(game_temp_dll_path);
        if (game_code.dll) {
            game_code.update_and_render = (GameUpdateAndRender)GetProcAddress(game_code.dll, "update_and_render");
        }
    }

    return game_code;
}

typedef struct WindowClientDimensions {
    int width;
    int height;
} WindowClientDimensions;

WindowClientDimensions get_window_client_dimensions(HWND window) {
    WindowClientDimensions result = {};
    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;
    return result;
}

void display_offscreen_buffer_in_window(OffscreenBuffer *buffer, HDC window_device_context, int client_width, int client_height) {
    // TODO(mal): only clear the parts of the screen we AREN'T using
    // e.g. if we're drawing to only a portion of the screen and have black bars
    // (think 4:3 fullscreen on a 16:9 monitor)
    // PatBlt(device_context, 0, 0, client_width, client_height, BLACKNESS);
    StretchDIBits(
        window_device_context,
        0, 0, client_width, client_height,
        0, 0, buffer->width, buffer->height,
        buffer->memory,
        &buffer->info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

inline uint64_t get_wall_clock() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result.QuadPart;
}

inline float get_seconds_elapsed(
    uint64_t wall_clock_end,
    uint64_t wall_clock_start
)
{
    float result = (float)(wall_clock_end - wall_clock_start) / (float)wall_clock_frequency;
    return result;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch (message) {
        // TODO(mal): Should we handle this?
        // case WM_ACTIVATEAPP:
        //     break;

        case WM_PAINT:
            WindowClientDimensions client = get_window_client_dimensions(window);
            PAINTSTRUCT ps;
            HDC window_device_context = BeginPaint(window, &ps);
            display_offscreen_buffer_in_window(&offscreen_buffer, window_device_context, client.width, client.height);
            EndPaint(window, &ps);
            break;

        case WM_CLOSE: // TODO(mal): handle WM_CLOSE as an error and recreate the window
        case WM_DESTROY:
            game_is_running = false;
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
        // TODO(mal): Better window class name
        .lpszClassName = L"GameWindowClass",
    };
    if (!RegisterClassW(&game_window_class)) {
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK);
        return 1;
    }

    // TODO(mal): clean up the following section
    // FIXME(mal): do NOT use MAX_PATH. It has a hard limit of 260 characters and will often fail
    // if people are installing the game deep in their filesystem. A solution for this is to use
    // dynamically-sized buffers but I'll have to write those myself later on.
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    wchar_t *one_past_last_slash = NULL;
    // SAFETY: This works because the bit pattern for L'\\' will never appear in a surrogate pair.
    // The value of L'\\' is 0x005C.
    // Surrogate pair bit patterns range from [D800, DBFF] (high) and [DC00, DFFF].
    // NOTE(mal): Also I believe there should always be at least one slash in any value we get from
    // GetModuleFileNameW so we don't have to worry about checking for null terminator (though maybe
    // we should, we'd just have to be careful about false positive null terminators (maybe iterate
    // two bytes at a time?))
    for (wchar_t *scan = exe_path; *scan; scan++) {
        if (*scan == L'\\') one_past_last_slash = scan + 1;
    }
    if (!one_past_last_slash) {
        // TODO(mal): show a message or something then quit
        // Maybe assert here?
        // Can this ever even fail? Can we have a case where there's no backslash???
        return 1;
    }
    // TODO(mal): do we need the base_dir_path anymore? It's kind of left over from when I was
    // trying a few different things out and needed a null-terminated base_dir_path for some win32
    // calls. Could go back to constructing exe dir paths with just the exe_path and the
    // one_past_last_slash - exe_path length calculation.
    wchar_t base_dir_path[MAX_PATH];
    size_t base_dir_path_len = one_past_last_slash - exe_path;
    wchar_t game_dll_path[MAX_PATH];
    wchar_t game_dll_lock_path[MAX_PATH];
    // TODO(mal): better name for this? Maybe game_dev_dll_path?
    wchar_t game_temp_dll_path[MAX_PATH];
    concat_wstrings(
        exe_path, base_dir_path_len,
        L"\0", sizeof(L"\0"),
        base_dir_path, sizeof(base_dir_path)
    );
    concat_wstrings(
        base_dir_path, base_dir_path_len,
        L"game.dll", sizeof(L"game.dll"),
        game_dll_path, sizeof(game_dll_path)
    );
    concat_wstrings(
        base_dir_path, base_dir_path_len,
        L"game.lock", sizeof(L"game.lock"),
        game_dll_lock_path, sizeof(game_dll_lock_path)
    );
    concat_wstrings(
        base_dir_path, base_dir_path_len,
        L"game_temp.dll", sizeof(L"game_temp.dll"),
        game_temp_dll_path, sizeof(game_temp_dll_path)
    );
    // TODO(mal): check if the game dll exists, abort if not?

    GameCode game_code = load_game_code(game_dll_path, game_temp_dll_path, game_dll_lock_path);

    // TODO(mal): check if there are different options we want to use for anything
    HWND game_window = CreateWindowExW(
        0,
        game_window_class.lpszClassName,
        // TODO(mal): different name
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
    offscreen_buffer.width = 320;
    offscreen_buffer.height = 200;
    // offscreen_buffer.width = 16 * 4;
    // offscreen_buffer.height = 9 * 4;
    offscreen_buffer.bytes_per_pixel = 4; // 1 byte per color component (RGB) + 1 byte padding (align to 32-bit boundary)
    // TODO(mal): check if this fails and handle gracefully
    offscreen_buffer.memory = VirtualAlloc(
        0,
        offscreen_buffer.width * offscreen_buffer.height * offscreen_buffer.bytes_per_pixel,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
    offscreen_buffer.info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    offscreen_buffer.info.bmiHeader.biWidth = offscreen_buffer.width;
    // NOTE(mal): When biHeight is negative, windows treats this as a TOPDOWN buffer.
    // i.e. (0, 0) is the upper-left corner of the image. 
    offscreen_buffer.info.bmiHeader.biHeight = -offscreen_buffer.height;
    offscreen_buffer.info.bmiHeader.biPlanes = 1;
    offscreen_buffer.info.bmiHeader.biBitCount = offscreen_buffer.bytes_per_pixel * 8;
    offscreen_buffer.info.bmiHeader.biCompression = BI_RGB;

    #define kibibytes(n) n * 1024ll
    #define mebibytes(n) n * kibibytes(n)
    #define gibibytes(n) n * mebibytes(n)
    const size_t GAME_STORAGE_SIZE = mebibytes(128);
    GameMemory game_memory = {};
    game_memory.storage_size = GAME_STORAGE_SIZE;
    game_memory.storage = VirtualAlloc(
        0,
        GAME_STORAGE_SIZE,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
    if (!game_memory.storage) {
        MessageBoxW(NULL, L"Failed to allocate memory for game", L"Error", MB_OK);
        return 1;
    }

    LARGE_INTEGER query_performance_frequency_result;
    QueryPerformanceFrequency(&query_performance_frequency_result);
    wall_clock_frequency = query_performance_frequency_result.QuadPart;

    // NOTE(mal): Default sleep granularity is ~16ms. If we do all our work for a frame and have
    // time to spare before hitting our target frametime, we want to sleep the thread to avoid
    // burning CPU cycles. Basically here we're trying to set a finer sleep granularity here. It may
    // or may not happen, in which case we'll just have to spin and burn cycles in the event that we
    // have time to spare.
    const UINT DESIRED_SCHEDULER_MS = 1;
    bool is_sleep_granular = timeBeginPeriod(DESIRED_SCHEDULER_MS) == TIMERR_NOERROR;

    // TODO(mal): We should calculate this based on the monitor refresh rate. Eventually though
    // we'll want to allow the user to set a framerate limit or allow enable/disable of vsync.
    // Some things we'll want to consider:
    // - Most users nowadays have 1+ monitors
    // - Game might be windowed and dragged from monitor to monitor
    float target_seconds_per_frame = 1.0f / 60.0f;

    // TODO(mal): we need to be able to set a target framerate and sleep (or
    // spin, if we can't make our sleep granular) if we process everything too
    // fast.
    game_is_running = true;
    // TODO(mal): should this initialize to 0?
    uint64_t frame_start_wall_clock = get_wall_clock();
    GameInput game_input = {};
    while (game_is_running) {
        WIN32_FILE_ATTRIBUTE_DATA dll_attribs;
        GetFileAttributesExW(game_dll_path, GetFileExInfoStandard, &dll_attribs);
        if (CompareFileTime(&dll_attribs.ftLastWriteTime, &game_code.dll_last_write_time) != 0) {
            FreeLibrary(game_code.dll);
            game_code = load_game_code(game_dll_path, game_temp_dll_path, game_dll_lock_path);
        }

        MSG window_message;
        while (PeekMessageW(&window_message, 0, 0, 0, PM_REMOVE)) {
            switch (window_message.message) {
                case WM_QUIT:
                    game_is_running = false;
                    break;

                case WM_SYSKEYUP:
                case WM_SYSKEYDOWN:
                case WM_KEYUP:
                case WM_KEYDOWN:
                    bool was_down = (window_message.lParam & (1 << 30)) != 0;
                    bool is_down = (window_message.lParam & (1 << 31)) == 0;
                    if (is_down != was_down) {
                        uint32_t virtual_key_code = (uint32_t)window_message.wParam;
                        bool is_alt_pressed = (window_message.lParam & (1 << 29));
                        bool is_extended_key = (window_message.lParam & (1 << 24));

                        // TODO(mal): Should these be handled here in the platform layer or should they
                        // be handled in the game which then sends some request for the operation to the
                        // platform layer?
                        if (is_down) {
                            if (virtual_key_code == VK_F4 && is_alt_pressed) {
                                game_is_running = false;
                            }

                            if (virtual_key_code == VK_RETURN && is_alt_pressed) {
                                // TODO(mal): toggle fullscreen
                            }
                        }

                        GameKey game_key = (GameKey)virtual_key_code;
                        if (virtual_key_code == VK_MENU) {
                            game_key = is_extended_key ? GAME_KEY_ALT_R : GAME_KEY_ALT_L;
                        } else if (virtual_key_code == VK_SHIFT) {
                            // NOTE(mal): From a comment by Travis Vroman in Kohi source, bits
                            // indicating key extensions aren't set for shift.
                            uint32_t left_shift = MapVirtualKeyW(VK_LSHIFT, MAPVK_VK_TO_VSC);
                            uint32_t scancode = (window_message.lParam & (0xFF << 16)) >> 16;
                            game_key = scancode == left_shift ? GAME_KEY_SHIFT_L : GAME_KEY_SHIFT_R;
                        } else if (virtual_key_code == VK_CONTROL) {
                            game_key = is_extended_key ? GAME_KEY_CTRL_R : GAME_KEY_CTRL_L;
                        }

                        game_input.keys_down[game_key] = is_down;
                    }

                    break;

                default:
                    TranslateMessage(&window_message);
                    DispatchMessageW(&window_message);
                    break;
            }
        }

        if (game_code.update_and_render) {
            GameOffscreenBuffer buf;
            buf.memory = offscreen_buffer.memory;
            buf.width = offscreen_buffer.width;
            buf.height = offscreen_buffer.height;
            buf.bytes_per_pixel = offscreen_buffer.bytes_per_pixel;

            game_code.update_and_render(&buf, &game_input, &game_memory);
        }

        HDC device_context = GetDC(game_window);
        WindowClientDimensions client = get_window_client_dimensions(game_window);
        display_offscreen_buffer_in_window(&offscreen_buffer, device_context, client.width, client.height);
        ReleaseDC(game_window, device_context);

        uint64_t work_end_wall_clock = get_wall_clock();
        float frame_work_seconds_elapsed = get_seconds_elapsed(work_end_wall_clock, frame_start_wall_clock);
        if (frame_work_seconds_elapsed < target_seconds_per_frame) {
            float sleep_seconds = 0.0f;
            if (is_sleep_granular) {
                sleep_seconds = target_seconds_per_frame - frame_work_seconds_elapsed;
                DWORD sleep_ms = (DWORD)(1000.0f * sleep_seconds);
                if (sleep_ms > 0) {
                    Sleep(sleep_ms);
                }
            }

            float frame_test_seconds_elapsed = get_seconds_elapsed(get_wall_clock(), frame_start_wall_clock);
            // TODO(mal): do want want to check the commented-out code or if our sleep_seconds were 0?
            // If the former, it'll also alert us if we've woken up early from a sleep, which
            // does seem to happen fairly often.
            // if (frame_test_seconds_elapsed < target_seconds_per_frame) {
            if (is_sleep_granular && sleep_seconds <= 0.0f) { 
                // TODO(mal): log missed sleep
                OutputDebugStringW(L"Missed sleep!\n");
            }
            while (frame_test_seconds_elapsed < target_seconds_per_frame) {
                frame_test_seconds_elapsed = get_seconds_elapsed(get_wall_clock(), frame_start_wall_clock);
            }

        } else {
            // TODO(mal): log missed target seconds per frame
            // i.e. something really hefty happened this frame!
            OutputDebugStringW(L"We missed our target frame time!\n");
        }

        uint64_t frame_end_wall_clock = get_wall_clock();
        // TODO(mal): eventually we'll want a way to display this in-game
        float frame_ms_elapsed = 1000.0f * get_seconds_elapsed(frame_end_wall_clock, frame_start_wall_clock);
        frame_start_wall_clock = frame_end_wall_clock;
    }

    return 0;

}
