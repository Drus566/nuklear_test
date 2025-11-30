/* nuklear - 1.32.0 - public domain */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

// --- NK DEFINITIONS ---
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_D3D11_IMPLEMENTATION
#include "Nuklear/nuklear.h"
#include "Nuklear/nuklear_d3d11.h"

/* --- D3D11 Globals --- */
static IDXGISwapChain *swap_chain;
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static ID3D11RenderTargetView* rt_view;

/* --- Nuklear Context --- */
struct nk_context *ctx;
struct nk_colorf bg = { 0.10f, 0.18f, 0.24f, 1.0f };

/* --- Слой данных: Структура и статические Сигналы (имитация) --- */
#define MAX_NAME_LEN 64
#define NUM_SIGNALS 5

typedef struct {
    char name[MAX_NAME_LEN];
    int value;
} signal_t;

// Статические данные для демонстрации
static signal_t signals[NUM_SIGNALS] = {
    {"Temperature", 25},
    {"Pressure", 1013},
    {"Flow Rate", 50},
    {"Level", 75},
    {"Status Word", 1}
};

/*
 * Функция: setSwapChainSize
 * Назначение: Пересоздание буферов отрисовки при изменении размера окна.
 */
static void setSwapChainSize(int width, int height) {
    ID3D11Texture2D *back_buffer;
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    HRESULT hr;

    if (rt_view) ID3D11RenderTargetView_Release(rt_view);

    ID3D11DeviceContext_OMSetRenderTargets(context, 0, NULL, NULL);

    hr = IDXGISwapChain_ResizeBuffers(swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR) {
        MessageBoxW(NULL, L"DXGI device is removed or reset!", L"Error", 0);
        exit(0);
    }
    assert(SUCCEEDED(hr));

    memset(&desc, 0, sizeof(desc));
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void **)&back_buffer);
    assert(SUCCEEDED(hr));

    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)back_buffer, &desc, &rt_view);
    assert(SUCCEEDED(hr));

    ID3D11Texture2D_Release(back_buffer);
}

/*
 * Функция: WindowProc
 * Назначение: Обработчик сообщений Windows. Включает логику ресайза и ввода Nuklear.
 */
static LRESULT CALLBACK WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (swap_chain) {
                int width = LOWORD(lparam);
                int height = HIWORD(lparam);
                setSwapChainSize(width, height);
                nk_d3d11_resize(context, width, height);
            }
            break;
    }

    if (nk_d3d11_handle_event(wnd, msg, wparam, lparam)) return 0;

    return DefWindowProcW(wnd, msg, wparam, lparam);
}

/*
 * Функция: draw_gui
 * Назначение: Здесь будет весь наш код Nuklear.
 * Реализована редактируемая таблица сигналов.
 */
static void draw_gui(void) {
    if (nk_begin(ctx, "Конфигурация Modbus Сигналов", nk_rect(50, 50, 450, 300),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE|NK_WINDOW_TITLE|NK_WINDOW_SCALABLE))
    {
        // 1. Заголовок таблицы (Title Row)
        // Начать ряд с фиксированной высотой 25px и 2 колонками
        nk_layout_row_begin(ctx, NK_STATIC, 25, 2);
        
        // Первая колонка заголовка: "Имя" (ширина 150px)
        nk_layout_row_push(ctx, 150);
        nk_label(ctx, "Имя", NK_TEXT_LEFT); // Обновлено для кириллицы
        
        // Вторая колонка заголовка: "Значение" (ширина 120px)
        nk_layout_row_push(ctx, 120);
        nk_label(ctx, "Значение", NK_TEXT_LEFT); // Обновлено для кириллицы
        
        nk_layout_row_end(ctx);
        
        // Добавляем горизонтальный разделитель
        nk_layout_row_dynamic(ctx, 2, 1);
        {
            struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
            struct nk_rect r = nk_window_get_content_region(ctx);
            // nk_stroke_line ожидает 4 float-координаты.
            nk_stroke_line(canvas, r.x, r.y, r.x + r.w, r.y, 1.0f, nk_rgb(100, 100, 100));
        }

        // 2. Рендеринг строк данных (Data Rows)
        for (int i = 0; i < NUM_SIGNALS; ++i) {
            // Настройка ряда для данных (высота 30px, 2 колонки)
            nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
            
            // 2.1. Колонка "Имя" (Редактируемое текстовое поле)
            nk_layout_row_push(ctx, 150);
            nk_edit_string_zero_terminated(ctx, 
                NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, 
                signals[i].name, MAX_NAME_LEN, nk_filter_default);
                
            // 2.2. Колонка "Значение" (Редактируемое числовое поле)
            nk_layout_row_push(ctx, 120);
            // # - означает, что лейбл будет скрыт, что идеально для столбца таблицы
            nk_property_int(ctx, "#", 0, &signals[i].value, 65535, 1, 1);
            
            nk_layout_row_end(ctx);
        }
    }
    // Обязательно: завершаем окно.
    nk_end(ctx);
}


int main(void) {
    WNDCLASSW wc;
    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exstyle = WS_EX_APPWINDOW;
    HWND wnd;
    int running = 1;
    HRESULT hr;
    D3D_FEATURE_LEVEL feature_level;
    DXGI_SWAP_CHAIN_DESC swap_chain_desc;

    /* Win32 Setup */
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(0);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"NuklearWindowClass";
    RegisterClassW(&wc);

    AdjustWindowRectEx(&rect, style, FALSE, exstyle);

    wnd = CreateWindowExW(exstyle, wc.lpszClassName, L"Nuklear Modbus Demo",
        style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.left,
        NULL, NULL, wc.hInstance, NULL);

    /* D3D11 setup */
    memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.OutputWindow = wnd;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Flags = 0;
    
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
        NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
        &swap_chain, &device, &feature_level, &context);
    
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP,
            NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
            &swap_chain, &device, &feature_level, &context);
        assert(SUCCEEDED(hr));
    }
    setSwapChainSize(WINDOW_WIDTH, WINDOW_HEIGHT);

    /* GUI Initialization */
    ctx = nk_d3d11_init(device, WINDOW_WIDTH, WINDOW_HEIGHT, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);
    
    /* Инициализация и загрузка шрифта с поддержкой кириллицы */
    {
        struct nk_font_atlas *atlas;
        struct nk_font *default_font = NULL; 
        struct nk_font *cyrillic_font = NULL;
        
        /* 1. Настройка конфигурации шрифта */
        struct nk_font_config cfg = nk_font_config(14.0f); 

        /* 2. Диапазоны кириллицы (используем статический массив) */
        static const nk_rune cyrillic_ranges[] = {
            0x0020, 0x00FF, /* Базовая латиница и дополнение Latin-1 */
            0x0400, 0x052F, /* Кириллица и дополнение */
            0x2DE0, 0x2DFF, /* Расширенная кириллица А */
            0xA640, 0xA69F, /* Расширенная кириллица В */
            0, 0
        };
        cfg.range = cyrillic_ranges; 

        nk_d3d11_font_stash_begin(&atlas); 
        
        // *** ИСПРАВЛЕНИЕ: Гарантированная загрузка шрифта по умолчанию ПЕРВЫМ ***
        // Это гарантирует, что атлас шрифтов не будет пустым, предотвращая assertion.
        default_font = nk_font_atlas_add_default(atlas, 13.0f, 0); 
        
        // Пытаемся загрузить шрифт пользователя (arial.ttf) с кириллицей
        cyrillic_font = nk_font_atlas_add_from_file(atlas, 
            "arial.ttf", 14.0f, &cfg); 

        // Завершаем процесс загрузки шрифтов
        nk_d3d11_font_stash_end();

        // 7. Устанавливаем шрифт для контекста: 
        // Если cyrillic_font загрузился, используем его; иначе используем шрифт по умолчанию.
        if (cyrillic_font) {
             printf("Success: Loaded custom font 'arial.ttf' with Cyrillic support.\n");
            nk_style_set_font(ctx, &cyrillic_font->handle);
        } else if (default_font) {
            printf("Warning: Font 'arial.ttf' not found or could not be loaded. Using default (ASCII only).\n");
            nk_style_set_font(ctx, &default_font->handle);
        } else {
            // Этот блок не должен срабатывать, но он здесь для надежности.
            fprintf(stderr, "Fatal Error: Failed to load any font.\n");
            running = 0; // Останавливаем цикл, если не удалось загрузить даже дефолтный шрифт.
        }
    }

    while (running)
    {
        /* Input Handling */
        MSG msg;
        nk_input_begin(ctx);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                running = 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        nk_input_end(ctx);

        /* GUI Logic and Layout */
        draw_gui();

        /* Draw */
        ID3D11DeviceContext_ClearRenderTargetView(context, rt_view, &bg.r);
        ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rt_view, NULL);
        nk_d3d11_render(context, NK_ANTI_ALIASING_ON);
        hr = IDXGISwapChain_Present(swap_chain, 1, 0);
        if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
            MessageBoxW(NULL, L"D3D11 device is lost or removed!", L"Error", 0);
            break;
        } else if (hr == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
        }
        assert(SUCCEEDED(hr));
    }

    /* Shutdown */
    ID3D11DeviceContext_ClearState(context);
    nk_d3d11_shutdown();
    ID3D11RenderTargetView_Release(rt_view);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    IDXGISwapChain_Release(swap_chain);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}