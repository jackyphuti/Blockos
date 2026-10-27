#pragma once

#include <stdint.h>
#include <stddef.h>

#ifndef COLOR_ARGB
#define COLOR_ARGB(a,r,g,b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#endif

struct Framebuffer;

// Window descriptors
struct GuiWindow {
    int id;
    int x, y, w, h;
    const char* title;
    bool visible;
    bool minimized;
};

// Minesweeper state
struct MinesweeperGame {
    static constexpr int W = 9;
    static constexpr int H = 9;
    static constexpr int TOTAL_MINES = 10;

    bool is_mine[W][H];
    bool revealed[W][H];
    bool flagged[W][H];
    int count[W][H];

    bool game_over = false;
    bool victory = false;
    bool face_pressed = false;
    int flags_left = 10;
    int timer_seconds = 0;
    bool timer_running = false;

    void init();
    void reset();
    void reveal(int gx, int gy);
    void toggle_flag(int gx, int gy);
    void flood_fill(int gx, int gy);
    void check_victory();
};

// Terminal shell state
struct TerminalShell {
    static constexpr int MAX_HISTORY = 18;
    static constexpr int LINE_LEN = 80;

    char history[MAX_HISTORY][LINE_LEN];
    uint32_t history_color[MAX_HISTORY];
    int history_count = 0;

    char input_buf[LINE_LEN];
    int input_len = 0;
    bool cursor_blink = true;

    void init();
    void add_line(const char* text, uint32_t color = 0x00FFFFFF);
    void handle_char(char c);
    void handle_backspace();
    void execute();
    void clear();
};

// Notepad text editor state
struct NotepadApp {
    static constexpr int MAX_LINES = 14;
    static constexpr int LINE_LEN = 64;

    char lines[MAX_LINES][LINE_LEN];
    int line_count = 0;
    int cursor_row = 0;
    int cursor_col = 0;
    const char* status_msg = "Normal Mode";

    void init();
    void handle_char(char c);
    void handle_backspace();
    void handle_enter();
    void save();
};

// Bluetooth device manager state
struct BluetoothDev {
    const char* name;
    const char* mac;
    const char* type;
    int rssi;
    bool connected;
};

struct BluetoothManager {
    static constexpr int MAX_DEVS = 4;
    BluetoothDev devices[MAX_DEVS];
    int selected_idx = 0;
    bool scanning = false;
    int scan_ticks = 0;
    const char* controller_name = "Intel/Broadcom HCI 5.2 (PCI Autoprobe)";
    const char* radio_status = "Discoverable / Ready";

    void init();
    void start_scan();
    void toggle_connect();
};

class GuiEngine {
public:
    Framebuffer* fb_ptr = nullptr;
    uint8_t* backbuf = nullptr;
    uint32_t fb_width = 1280;
    uint32_t fb_height = 800;
    bool is_initialized = false;
    bool is_running = true;

    // Mouse state
    int cursor_x = 520;
    int cursor_y = 280;
    bool mouse_left = false;
    bool mouse_right = false;
    bool prev_left = false;
    bool prev_right = false;

    // Dragging state
    int dragging_win_id = -1;
    int drag_off_x = 0;
    int drag_off_y = 0;

    // Active window: 0=Terminal, 1=Minesweeper, 2=Notepad, 3=Bluetooth
    int active_win_id = 0;

    // Windows
    GuiWindow win_terminal{0, 50, 25, 680, 480, "BlockOS Terminal [x86-64 UEFI]", true, false};
    GuiWindow win_mines{1, 755, 25, 230, 285, "Minesweeper", true, false};
    GuiWindow win_notepad{2, 755, 325, 490, 225, "Readme.txt - Notepad", true, false};
    GuiWindow win_bluetooth{3, 380, 140, 420, 310, "Bluetooth & Wireless Devices", false, false};

    // Subsystem states
    MinesweeperGame minesweeper;
    TerminalShell terminal;
    NotepadApp notepad;
    BluetoothManager bluetooth;

    // Start Menu & Taskbar state
    bool start_menu_open = false;
    bool sound_muted = false;
    uint32_t clock_seconds = 0;
    uint32_t tick_counter = 0;

    GuiEngine() = default;

    void init(Framebuffer* fb, uint8_t* buffer, uint32_t w, uint32_t h);
    void render();
    void run_desktop();

    // Drawing primitives
    void draw_rect(int x, int y, int w, int h, uint32_t color);
    void draw_text(int x, int y, const char* text, uint32_t color);
    void draw_pixel(int x, int y, uint32_t color);
    void draw_3d_box(int x, int y, int w, int h, bool sunken = false, uint32_t face_color = 0x00C0C0C0);
    void draw_window(int x, int y, int w, int h, const char* title, bool active = true);
    void draw_button(int x, int y, int w, int h, const char* text, bool pressed = false);
    void draw_icon(int x, int y, const char* label, int icon_type);
    void draw_cursor(int x, int y);

    // Audio / Sound effects
    void play_sound(uint32_t freq_hz, uint32_t duration_ms);
    void play_click();
    void play_chime();
    void play_error();
    void play_explosion();
    void play_fanfare();

    // Application renderers
    void draw_desktop_background();
    void draw_taskbar();
    void draw_start_menu();
    void draw_terminal_window();
    void draw_minesweeper_window();
    void draw_notepad_window();
    void draw_bluetooth_window();

    // Event handlers
    void on_mouse_move(int dx, int dy);
    void on_mouse_click(bool left_pressed, bool right_pressed);
    void on_key(char ch, uint32_t scancode, bool pressed);
    void on_tick();

    // Window helpers
    GuiWindow* get_window(int id);
    void focus_window(int id);
    void toggle_window(int id);
};

extern GuiEngine desktop;
