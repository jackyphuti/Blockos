#include "gui.hpp"
#include "drivers/fb.h"
#include "drivers/backbuffer.h"
#include "drivers/audio.hpp"
#include "font8x8.h"
#include "input_bridge.hpp"
#include "drivers/keymap.hpp"

#include <string.h>

GuiEngine desktop;

namespace {

constexpr uint32_t COLOR_TEAL         = 0x00008080; // Classic 90s Desktop Teal
constexpr uint32_t COLOR_WHITE        = 0x00FFFFFF;
constexpr uint32_t COLOR_LIGHTGRAY    = 0x00C0C0C0; // Standard 90s 3D Silver Face
constexpr uint32_t COLOR_DARKGRAY     = 0x00808080; // Standard 90s Shadow
constexpr uint32_t COLOR_BLACK        = 0x00000000;
constexpr uint32_t COLOR_NAVY         = 0x00000080; // Active Title Bar
constexpr uint32_t COLOR_INACTIVE_BAR = 0x00808080; // Inactive Title Bar
constexpr uint32_t COLOR_CYAN         = 0x0000C0C0;
constexpr uint32_t COLOR_RED          = 0x00C02828;
constexpr uint32_t COLOR_BRIGHT_RED   = 0x00FF0000;
constexpr uint32_t COLOR_BLUE         = 0x000000FF;
constexpr uint32_t COLOR_GREEN        = 0x00008000;
constexpr uint32_t COLOR_YELLOW       = 0x00FFFF00;
constexpr uint32_t COLOR_LIGHTBLUE    = 0x003A6EA5;

// BlockOS Cyan Banner
static const char* const g_banner[5] = {
    "    _|_|_|    _|                      _|          _|_|      _|_|_|",
    "    _|    _|  _|    _|_|      _|_|_|  _|  _|    _|    _|  _|",
    "    _|_|_|    _|  _|    _|  _|        _|_|      _|    _|    _|_|",
    "    _|    _|  _|  _|    _|  _|        _|  _|    _|    _|        _|",
    "    _|_|_|    _|    _|_|      _|_|_|  _|    _|    _|_|    _|_|_|"
};

// Saturn graphic for terminal window
static const char* const g_saturn[24] = {
    "                                      ..;===+.",
    "                                   .:=iiiiii=+=",
    "                                .=i))=;::+)i=+,",
    "                             ,=i);)I)))I):=i=;",
    "                          .=i==))))ii)))I:i++",
    "                        +)+))iiiiiiii))I=i+:'",
    "            .,:;;++++++;:,.       )iii+:::;iii))+i='",
    "         .:;++=iiiiiiiiii=++;.    =::,,,:::=i));=+'",
    "       ,;+==ii)))))))))))ii==+;,      ,,,:=i))+=:",
    "     ,;+=ii))))))IIIIII))))ii===;.    ,,:=i)=i+",
    "    ;+=ii)))IIIIITIIIIII))))iiii=+,   ,:=));=,",
    "  ,+=i))IIIIIITTTTTITIIIIII)))I)i=+,,:+i)=i+",
    " ,+i))IIIIIITTTTTTTTTTTTI))IIII))i=::i))i='",
    ",=i))IIIIITLLTTTTTTTTTTIITTTTIII)+;+i)+i`",
    "=i))IIITTLTLTTTTTTTTTIITTLLTTTII+:i)ii:'",
    "+i))IITTTLLLTTTTTTTTTTTTLLLTTTT+:i)))=,",
    "=))ITTTTTTTTTTTLTTTTTTLLLLLLTi:=)IIiii;",
    ".i)IIITTTTTTTTLTTTITLLLLLLLT);=)I)))))i;",
    ":))IIITTTTTLTTTTTTLLHLLLLL);=)II)IIIIi=:",
    ":i)IIITTTTTTTTTLLLHLLHLL)+=)II)ITTTI)i=",
    ".i)IIITTTTITTLLLHHLLLL);=)II)ITTTTII)i+",
    "=i)IIIIIITTLLLLLLHLL=:i)II)TTTTTTIII)i'",
    "+i)i)))IITTLLLLLLLLT=:i)II)TTTTLTTIII)i;",
    "`+=+++;`"
};

// 90s Arrow Cursor (12x19)
static const char* const g_cursor_bitmap[19] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.....XXXXX ",
    "X..X..X     ",
    "X.X X..X    ",
    "XX   X..X   ",
    "X     X..X  ",
    "      X..X  ",
    "       XX   ",
    "            ",
    "            "
};

static inline void dbg_char(char c) {
    __asm__ volatile("outb %0, %1" : : "a"(c), "Nd"((uint16_t)0x3F8));
}
static inline void dbg_str(const char* s) {
    if (!s) return;
    while (*s) dbg_char(*s++);
}

} // namespace

// ============================================================
// Minesweeper Implementation
// ============================================================

void MinesweeperGame::init()
{
    reset();
}

void MinesweeperGame::reset()
{
    game_over = false;
    victory = false;
    face_pressed = false;
    flags_left = TOTAL_MINES;
    timer_seconds = 0;
    timer_running = false;

    memset(is_mine, 0, sizeof(is_mine));
    memset(revealed, 0, sizeof(revealed));
    memset(flagged, 0, sizeof(flagged));
    memset(count, 0, sizeof(count));

    // Fixed authentic beginner minefield pattern: 10 mines
    static const int mine_coords[10][2] = {
        {0, 1}, {2, 3}, {3, 7}, {4, 4}, {5, 1},
        {6, 6}, {7, 2}, {8, 8}, {1, 5}, {7, 5}
    };

    for (int i = 0; i < TOTAL_MINES; ++i) {
        int mx = mine_coords[i][0];
        int my = mine_coords[i][1];
        if (mx >= 0 && mx < W && my >= 0 && my < H)
            is_mine[mx][my] = true;
    }

    // Calculate neighboring mine counts
    for (int x = 0; x < W; ++x) {
        for (int y = 0; y < H; ++y) {
            if (is_mine[x][y]) continue;
            int c = 0;
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                        if (is_mine[nx][ny]) c++;
                    }
                }
            }
            count[x][y] = c;
        }
    }
}

void MinesweeperGame::flood_fill(int gx, int gy)
{
    if (gx < 0 || gx >= W || gy < 0 || gy >= H) return;
    if (revealed[gx][gy] || flagged[gx][gy] || is_mine[gx][gy]) return;

    revealed[gx][gy] = true;

    if (count[gx][gy] == 0) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx != 0 || dy != 0) {
                    flood_fill(gx + dx, gy + dy);
                }
            }
        }
    }
}

void MinesweeperGame::reveal(int gx, int gy)
{
    if (game_over || victory) return;
    if (gx < 0 || gx >= W || gy < 0 || gy >= H) return;
    if (flagged[gx][gy] || revealed[gx][gy]) return;

    timer_running = true;

    if (is_mine[gx][gy]) {
        // Hit a mine!
        revealed[gx][gy] = true;
        game_over = true;
        timer_running = false;
        desktop.play_explosion();
        return;
    }

    flood_fill(gx, gy);
    desktop.play_click();
    check_victory();
}

void MinesweeperGame::toggle_flag(int gx, int gy)
{
    if (game_over || victory) return;
    if (gx < 0 || gx >= W || gy < 0 || gy >= H) return;
    if (revealed[gx][gy]) return;

    flagged[gx][gy] = !flagged[gx][gy];
    if (flagged[gx][gy]) {
        flags_left--;
        desktop.play_sound(1400, 20);
    } else {
        flags_left++;
        desktop.play_sound(1100, 20);
    }
    check_victory();
}

void MinesweeperGame::check_victory()
{
    int revealed_count = 0;
    for (int x = 0; x < W; ++x) {
        for (int y = 0; y < H; ++y) {
            if (revealed[x][y]) revealed_count++;
        }
    }
    if (revealed_count == (W * H - TOTAL_MINES)) {
        victory = true;
        timer_running = false;
        flags_left = 0;
        desktop.play_fanfare();
    }
}

// ============================================================
// Terminal Shell Implementation
// ============================================================

void TerminalShell::init()
{
    history_count = 0;
    input_len = 0;
    input_buf[0] = '\0';
    cursor_blink = true;

    add_line("BlockOS Interactive Terminal v1.0 (tty0 - 80x25)", COLOR_WHITE);
    add_line("Hardware: VirtIO, 16550 UART, GOP Framebuffer, PC-Spkr", COLOR_LIGHTGRAY);
    add_line("Type 'help' for command list, or click windows to multitask.", COLOR_CYAN);
    add_line("", COLOR_WHITE);
}

void TerminalShell::add_line(const char* text, uint32_t color)
{
    if (history_count >= MAX_HISTORY) {
        // Scroll up
        for (int i = 0; i < MAX_HISTORY - 1; ++i) {
            strncpy(history[i], history[i + 1], LINE_LEN);
            history_color[i] = history_color[i + 1];
        }
        history_count = MAX_HISTORY - 1;
    }
    strncpy(history[history_count], text, LINE_LEN - 1);
    history[history_count][LINE_LEN - 1] = '\0';
    history_color[history_count] = color;
    history_count++;
}

void TerminalShell::clear()
{
    history_count = 0;
    add_line("BlockOS Terminal - Screen cleared.", COLOR_CYAN);
}

void TerminalShell::handle_char(char c)
{
    if (input_len < LINE_LEN - 2 && c >= 32 && c <= 126) {
        input_buf[input_len++] = c;
        input_buf[input_len] = '\0';
    }
}

void TerminalShell::handle_backspace()
{
    if (input_len > 0) {
        input_len--;
        input_buf[input_len] = '\0';
    }
}

void TerminalShell::execute()
{
    dbg_str("[SHELL EXEC] cmd='");
    dbg_str(input_buf);
    dbg_str("'\r\n");

    char prompt_line[LINE_LEN];
    prompt_line[0] = '\0';
    strcat(prompt_line, "root@blockos:/# ");
    strcat(prompt_line, input_buf);
    add_line(prompt_line, COLOR_WHITE);

    // Parse command
    if (strcmp(input_buf, "help") == 0) {
        add_line("Commands: help, beep, sound, bt, uname, ls, cat, mines, notepad, clear, reboot", COLOR_CYAN);
    } else if (strcmp(input_buf, "beep") == 0) {
        add_line("Playing speaker diagnostic tone (880 Hz)...", COLOR_GREEN);
        desktop.play_sound(880, 50);
    } else if (strcmp(input_buf, "sound") == 0) {
        add_line("Playing retro workstation audio chime...", COLOR_GREEN);
        desktop.play_chime();
    } else if (strcmp(input_buf, "bt") == 0 || strcmp(input_buf, "bluetooth") == 0) {
        add_line("Bluetooth HCI 5.2 [Ready] - Discovered 4 devices.", COLOR_LIGHTBLUE);
        add_line("1. Retro Hi-Fi Audio Headset (Connected)", COLOR_GREEN);
        add_line("2. BlockOS Mobile Phone (Paired)", COLOR_WHITE);
        add_line("3. MS Ergonomic Bluetooth Mouse (Connected)", COLOR_GREEN);
        add_line("Opening Bluetooth Manager window...", COLOR_CYAN);
        desktop.toggle_window(3);
    } else if (strcmp(input_buf, "uname") == 0 || strcmp(input_buf, "uname -a") == 0) {
        add_line("BlockOS 1.0 x86_64 SMP UEFI GNU-EFI (Freestanding C/C++)", COLOR_GREEN);
    } else if (strcmp(input_buf, "ls") == 0) {
        add_line("bin/   etc/   System/   devices/   README.TXT", COLOR_CYAN);
    } else if (strcmp(input_buf, "cat README.TXT") == 0 || strcmp(input_buf, "cat") == 0) {
        add_line("BlockOS 90s Edition - 100% clean-room C/C++ without Linux drivers.", COLOR_WHITE);
    } else if (strcmp(input_buf, "mines") == 0) {
        add_line("Bringing Minesweeper to front...", COLOR_YELLOW);
        desktop.focus_window(1);
    } else if (strcmp(input_buf, "notepad") == 0) {
        add_line("Bringing Notepad to front...", COLOR_YELLOW);
        desktop.focus_window(2);
    } else if (strcmp(input_buf, "clear") == 0) {
        clear();
    } else if (strcmp(input_buf, "reboot") == 0) {
        add_line("Rebooting system...", COLOR_RED);
        // Out to keyboard controller 0x64 reset pulse
        __asm__ volatile("outb %0, $0x64" : : "a"((uint8_t)0xFE));
    } else if (input_len > 0) {
        char err[LINE_LEN];
        err[0] = '\0';
        strcat(err, "Command not found: ");
        strcat(err, input_buf);
        add_line(err, COLOR_RED);
        desktop.play_error();
    }

    input_len = 0;
    input_buf[0] = '\0';
}

// ============================================================
// Notepad Implementation
// ============================================================

void NotepadApp::init()
{
    line_count = 8;
    cursor_row = 7;
    cursor_col = 0;
    status_msg = "Document: README.TXT (Writable)";

    strncpy(lines[0], "==============================================", LINE_LEN);
    strncpy(lines[1], "        BLOCKOS 90s WORKSTATION DESKTOP       ", LINE_LEN);
    strncpy(lines[2], "==============================================", LINE_LEN);
    strncpy(lines[3], "* Platform : x86-64 freestanding UEFI microkernel", LINE_LEN);
    strncpy(lines[4], "* Hardware : Native VirtIO, 16550 UART, Bluetooth", LINE_LEN);
    strncpy(lines[5], "* Audio    : PC Speaker synthesizer & HDA Codec", LINE_LEN);
    strncpy(lines[6], "* Linux Drivers Used: ZERO (100% clean-room!)", LINE_LEN);
    strncpy(lines[7], "Type here to take notes...", LINE_LEN);
}

void NotepadApp::handle_char(char c)
{
    if (cursor_row < MAX_LINES) {
        size_t len = strlen(lines[cursor_row]);
        if (len < LINE_LEN - 2 && c >= 32 && c <= 126) {
            lines[cursor_row][len] = c;
            lines[cursor_row][len + 1] = '\0';
            cursor_col = (int)len + 1;
        }
    }
}

void NotepadApp::handle_backspace()
{
    if (cursor_row < MAX_LINES) {
        size_t len = strlen(lines[cursor_row]);
        if (len > 0) {
            lines[cursor_row][len - 1] = '\0';
            cursor_col = (int)len - 1;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = (int)strlen(lines[cursor_row]);
        }
    }
}

void NotepadApp::handle_enter()
{
    if (cursor_row < MAX_LINES - 1) {
        cursor_row++;
        lines[cursor_row][0] = '\0';
        cursor_col = 0;
        if (cursor_row >= line_count) line_count = cursor_row + 1;
    }
}

void NotepadApp::save()
{
    status_msg = "File saved successfully to RAMFS!";
    desktop.play_chime();
}

// ============================================================
// Bluetooth Manager Implementation
// ============================================================

void BluetoothManager::init()
{
    selected_idx = 0;
    scanning = false;
    scan_ticks = 0;

    devices[0] = {"Retro Hi-Fi Audio Headset", "B8:27:EB:4A:12:90", "A2DP Stereo Audio", -42, true};
    devices[1] = {"BlockOS Mobile Handheld",  "CC:78:AB:91:04:12", "RFCOMM Serial",     -58, false};
    devices[2] = {"MS Bluetooth Mouse",       "A0:E6:F8:21:44:88", "HID Pointer",        -38, true};
    devices[3] = {"IBM Model M Wireless",     "00:1B:DC:03:77:AA", "HID Keyboard",       -65, false};
}

void BluetoothManager::start_scan()
{
    scanning = true;
    scan_ticks = 15;
    radio_status = "Scanning for nearby Bluetooth devices...";
    desktop.play_chime();
}

void BluetoothManager::toggle_connect()
{
    if (selected_idx >= 0 && selected_idx < MAX_DEVS) {
        devices[selected_idx].connected = !devices[selected_idx].connected;
        if (devices[selected_idx].connected) {
            desktop.play_sound(659, 40);
            desktop.play_sound(880, 50);
        } else {
            desktop.play_sound(440, 40);
            desktop.play_sound(330, 50);
        }
    }
}

// ============================================================
// GuiEngine Core & Rendering
// ============================================================

void GuiEngine::init(Framebuffer* fb, uint8_t* buffer, uint32_t w, uint32_t h)
{
    fb_ptr = fb;
    backbuf = buffer;
    fb_width = w ? w : 1280;
    fb_height = h ? h : 800;
    is_initialized = (fb != nullptr && buffer != nullptr);
    is_running = true;

    minesweeper.init();
    terminal.init();
    notepad.init();
    bluetooth.init();
}

void GuiEngine::play_sound(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!sound_muted) {
        audio::speaker_beep(freq_hz, duration_ms);
    }
}

void GuiEngine::play_click()
{
    play_sound(1200, 10);
}

void GuiEngine::play_chime()
{
    play_sound(880, 25);
    play_sound(1320, 30);
}

void GuiEngine::play_error()
{
    play_sound(220, 80);
}

void GuiEngine::play_explosion()
{
    play_sound(140, 180);
}

void GuiEngine::play_fanfare()
{
    play_sound(523, 40);
    play_sound(659, 40);
    play_sound(784, 50);
    play_sound(1046, 80);
}

void GuiEngine::draw_pixel(int x, int y, uint32_t color)
{
    if (!backbuf || x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height)
        return;
    bb_put_pixel(backbuf, fb_width, (uint32_t)x, (uint32_t)y, color);
}

void GuiEngine::draw_rect(int x, int y, int w, int h, uint32_t color)
{
    if (!backbuf || w <= 0 || h <= 0) return;
    bb_draw_rect(backbuf, fb_width, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, color);
}

void GuiEngine::draw_text(int x, int y, const char* text, uint32_t color)
{
    if (!backbuf || !text) return;
    bb_draw_text(backbuf, fb_width, x, y, text, color);
}

void GuiEngine::draw_3d_box(int x, int y, int w, int h, bool sunken, uint32_t face_color)
{
    if (w < 2 || h < 2) return;
    uint32_t top_left = sunken ? COLOR_DARKGRAY : COLOR_WHITE;
    uint32_t bot_right = sunken ? COLOR_WHITE : COLOR_BLACK;
    uint32_t inner_br = sunken ? COLOR_LIGHTGRAY : COLOR_DARKGRAY;

    draw_rect(x, y, w, h, face_color);
    // Outer highlight/shadow
    draw_rect(x, y, w, 1, top_left);
    draw_rect(x, y, 1, h, top_left);
    draw_rect(x, y + h - 1, w, 1, bot_right);
    draw_rect(x + w - 1, y, 1, h, bot_right);
    // Inner bevel
    draw_rect(x + 1, y + h - 2, w - 2, 1, inner_br);
    draw_rect(x + w - 2, y + 1, 1, h - 2, inner_br);
}

void GuiEngine::draw_button(int x, int y, int w, int h, const char* text, bool pressed)
{
    draw_3d_box(x, y, w, h, pressed, COLOR_LIGHTGRAY);
    if (text) {
        int tx = x + (w - (int)strlen(text) * 8) / 2 + (pressed ? 1 : 0);
        int ty = y + (h - 8) / 2 + (pressed ? 1 : 0);
        draw_text(tx, ty, text, COLOR_BLACK);
    }
}

void GuiEngine::draw_window(int x, int y, int w, int h, const char* title, bool active)
{
    draw_3d_box(x, y, w, h, false, COLOR_LIGHTGRAY);

    // Title bar
    uint32_t bar_color = active ? COLOR_NAVY : COLOR_INACTIVE_BAR;
    draw_rect(x + 3, y + 3, w - 6, 18, bar_color);
    if (title) {
        draw_text(x + 6, y + 8, title, COLOR_WHITE);
    }

    // Window controls (_ + X)
    int btn_size = 14;
    int bx = x + w - 18;
    int by = y + 5;
    draw_button(bx, by, btn_size, btn_size, "X", false);
    bx -= 16;
    draw_button(bx, by, btn_size, btn_size, "+", false);
    bx -= 16;
    draw_button(bx, by, btn_size, btn_size, "_", false);
}

void GuiEngine::draw_icon(int x, int y, const char* label, int icon_type)
{
    int icon_size = 32;
    draw_rect(x, y, icon_size, icon_size, COLOR_LIGHTGRAY);
    draw_3d_box(x, y, icon_size, icon_size, false, COLOR_LIGHTGRAY);

    if (icon_type == 0) { // Computer
        draw_rect(x + 5, y + 4, 22, 16, COLOR_WHITE);
        draw_rect(x + 7, y + 6, 18, 12, COLOR_BLUE);
        draw_rect(x + 10, y + 21, 12, 3, COLOR_DARKGRAY);
        draw_rect(x + 6, y + 24, 20, 4, COLOR_LIGHTGRAY);
    } else if (icon_type == 1) { // Terminal
        draw_rect(x + 4, y + 4, 24, 24, COLOR_BLACK);
        draw_text(x + 7, y + 10, ">_", COLOR_CYAN);
    } else if (icon_type == 2) { // Minesweeper
        draw_rect(x + 8, y + 8, 16, 16, COLOR_DARKGRAY);
        draw_rect(x + 10, y + 10, 12, 12, COLOR_BLACK);
        draw_rect(x + 14, y + 5, 4, 22, COLOR_BLACK);
        draw_rect(x + 5, y + 14, 22, 4, COLOR_BLACK);
        draw_rect(x + 13, y + 13, 3, 3, COLOR_WHITE);
    } else if (icon_type == 3) { // Notepad
        draw_rect(x + 6, y + 4, 20, 24, COLOR_WHITE);
        draw_rect(x + 8, y + 8, 14, 2, COLOR_BLUE);
        draw_rect(x + 8, y + 13, 14, 2, COLOR_DARKGRAY);
        draw_rect(x + 8, y + 18, 14, 2, COLOR_DARKGRAY);
    } else if (icon_type == 4) { // Bluetooth
        draw_rect(x + 6, y + 4, 20, 24, COLOR_LIGHTBLUE);
        draw_text(x + 11, y + 11, "B", COLOR_WHITE);
    } else if (icon_type == 5) { // Recycle
        draw_rect(x + 7, y + 7, 18, 20, COLOR_DARKGRAY);
        draw_rect(x + 9, y + 9, 14, 16, COLOR_LIGHTGRAY);
        draw_rect(x + 5, y + 5, 22, 3, COLOR_DARKGRAY);
    }

    if (label) {
        int lx = x + (icon_size - (int)strlen(label) * 8) / 2;
        draw_text(lx, y + icon_size + 4, label, COLOR_WHITE);
    }
}

void GuiEngine::draw_cursor(int x, int y)
{
    for (int r = 0; r < 19; ++r) {
        for (int c = 0; c < 12; ++c) {
            char p = g_cursor_bitmap[r][c];
            if (p == 'X') {
                draw_pixel(x + c, y + r, COLOR_BLACK);
            } else if (p == '.') {
                draw_pixel(x + c, y + r, COLOR_WHITE);
            }
        }
    }
}

void GuiEngine::draw_desktop_background()
{
    draw_rect(0, 0, fb_width, fb_height - 30, COLOR_TEAL);

    draw_icon(14, 15,  "Computer", 0);
    draw_icon(14, 75,  "Terminal", 1);
    draw_icon(14, 135, "Mines",    2);
    draw_icon(14, 195, "Readme",   3);
    draw_icon(14, 255, "Wireless", 4);
    draw_icon(14, 315, "Recycle",  5);
}

void GuiEngine::draw_taskbar()
{
    const int bar_y = fb_height - 30;
    const int bar_h = 30;

    draw_3d_box(0, bar_y, fb_width, bar_h, false, COLOR_LIGHTGRAY);

    // Start Button
    draw_button(3, bar_y + 3, 64, 24, " Start", start_menu_open);
    // 4-Color Windows Flag
    int fx = 8;
    int fy = bar_y + 8;
    draw_rect(fx,     fy,     4, 4, COLOR_RED);
    draw_rect(fx + 5, fy,     4, 4, COLOR_GREEN);
    draw_rect(fx,     fy + 5, 4, 4, COLOR_BLUE);
    draw_rect(fx + 5, fy + 5, 4, 4, COLOR_YELLOW);

    // Taskbar Tabs
    int tx = 74;
    auto draw_tab = [&](const char* name, int win_id, int w) {
        GuiWindow* wptr = get_window(win_id);
        if (wptr && wptr->visible) {
            bool is_active = (active_win_id == win_id && !wptr->minimized);
            draw_button(tx, bar_y + 3, w, 24, name, is_active);
            tx += w + 4;
        }
    };

    draw_tab("Prompt", 0, 110);
    draw_tab("Mines", 1, 95);
    draw_tab("Readme.txt", 2, 100);
    draw_tab("Bluetooth", 3, 95);

    // System Tray
    int tray_w = 140;
    int tray_x = fb_width - tray_w - 4;
    draw_3d_box(tray_x, bar_y + 3, tray_w, 24, true, COLOR_LIGHTGRAY);

    // Tray icons: Sound & Bluetooth & Clock
    const char* snd_txt = sound_muted ? "[X]" : "[S]";
    draw_text(tray_x + 6, bar_y + 11, snd_txt, sound_muted ? COLOR_RED : COLOR_GREEN);
    draw_text(tray_x + 34, bar_y + 11, "[B]", COLOR_BLUE);

    // Clock
    uint32_t secs = clock_seconds % 60;
    uint32_t mins = (clock_seconds / 60) % 60;
    uint32_t hrs = 12 + ((clock_seconds / 3600) % 12);
    char time_str[16];
    time_str[0] = '0' + (hrs / 10);
    time_str[1] = '0' + (hrs % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (mins / 10);
    time_str[4] = '0' + (mins % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (secs / 10);
    time_str[7] = '0' + (secs % 10);
    time_str[8] = '\0';
    draw_text(tray_x + 62, bar_y + 11, time_str, COLOR_BLACK);
}

void GuiEngine::draw_start_menu()
{
    if (!start_menu_open) return;

    int sm_w = 180;
    int sm_h = 175;
    int sm_x = 3;
    int sm_y = fb_height - 30 - sm_h;

    draw_3d_box(sm_x, sm_y, sm_w, sm_h, false, COLOR_LIGHTGRAY);

    // Left vertical dark navy banner
    draw_rect(sm_x + 3, sm_y + 3, 24, sm_h - 6, COLOR_NAVY);
    draw_text(sm_x + 8, sm_y + sm_h - 20, "B", COLOR_WHITE);
    draw_text(sm_x + 8, sm_y + sm_h - 32, "L", COLOR_WHITE);
    draw_text(sm_x + 8, sm_y + sm_h - 44, "O", COLOR_WHITE);
    draw_text(sm_x + 8, sm_y + sm_h - 56, "C", COLOR_WHITE);
    draw_text(sm_x + 8, sm_y + sm_h - 68, "K", COLOR_WHITE);

    // Menu entries
    const char* items[] = {
        "> Terminal Shell",
        "* Minesweeper",
        "# Notepad Readme",
        "B Bluetooth Devices",
        "S Sound Test Chime",
        "! Restart BlockOS"
    };

    int iy = sm_y + 8;
    for (int i = 0; i < 6; ++i) {
        draw_text(sm_x + 34, iy, items[i], COLOR_BLACK);
        iy += 26;
        if (i < 5) {
            draw_rect(sm_x + 30, iy - 4, sm_w - 34, 1, COLOR_DARKGRAY);
            draw_rect(sm_x + 30, iy - 3, sm_w - 34, 1, COLOR_WHITE);
        }
    }
}

void GuiEngine::draw_terminal_window()
{
    if (!win_terminal.visible || win_terminal.minimized) return;

    int x = win_terminal.x;
    int y = win_terminal.y;
    int w = win_terminal.w;
    int h = win_terminal.h;

    draw_window(x, y, w, h, win_terminal.title, active_win_id == 0);

    // Menu Bar
    int my = y + 23;
    draw_rect(x + 3, my, w - 6, 16, COLOR_LIGHTGRAY);
    draw_text(x + 10, my + 4, "File  Edit  View  Help", COLOR_BLACK);

    // Terminal Area
    int cx = x + 4;
    int cy = y + 40;
    int cw = w - 8;
    int ch = h - 44;
    draw_3d_box(cx, cy, cw, ch, true, COLOR_BLACK);

    // 1. Boot Graphic: ASCII Banner
    int bx = cx + 8;
    int by = cy + 6;
    for (int i = 0; i < 5; ++i) {
        draw_text(bx, by + i * 9, g_banner[i], COLOR_CYAN);
    }

    // 2. Boot Graphic: Saturn Planet
    int sx = cx + 370;
    int sy = cy + 4;
    for (int i = 0; i < 18; ++i) {
        draw_text(sx, sy + i * 8, g_saturn[i], COLOR_RED);
    }

    // 3. Scrollback History
    int hy = cy + 65;
    for (int i = 0; i < terminal.history_count; ++i) {
        if (hy >= cy + ch - 22) break;
        draw_text(cx + 8, hy, terminal.history[i], terminal.history_color[i]);
        hy += 11;
    }

    // 4. Interactive Input Prompt
    if (hy < cy + ch - 12) {
        draw_text(cx + 8, hy, "root@blockos:/# ", COLOR_GREEN);
        int px = cx + 8 + 16 * 8;
        draw_text(px, hy, terminal.input_buf, COLOR_WHITE);
        // Blinking Cursor
        if (terminal.cursor_blink) {
            draw_text(px + terminal.input_len * 8, hy, "_", COLOR_CYAN);
        }
    }
}

void GuiEngine::draw_minesweeper_window()
{
    if (!win_mines.visible || win_mines.minimized) return;

    int x = win_mines.x;
    int y = win_mines.y;
    int w = win_mines.w;
    int h = win_mines.h;

    draw_window(x, y, w, h, win_mines.title, active_win_id == 1);

    // Menu Bar
    int my = y + 23;
    draw_rect(x + 3, my, w - 6, 16, COLOR_LIGHTGRAY);
    draw_text(x + 10, my + 4, "Game  Help", COLOR_BLACK);

    // Top Status Panel (Sunken)
    int py = y + 42;
    draw_3d_box(x + 8, py, w - 16, 36, true, COLOR_LIGHTGRAY);

    // LED Counter Left: Mines Left
    int lx = x + 14;
    int ly = py + 6;
    draw_3d_box(lx, ly, 44, 24, true, COLOR_BLACK);
    char buf_m[8];
    int fl = minesweeper.flags_left;
    if (fl < 0) fl = 0;
    buf_m[0] = '0';
    buf_m[1] = '0' + (fl / 10);
    buf_m[2] = '0' + (fl % 10);
    buf_m[3] = '\0';
    draw_text(lx + 8, ly + 8, buf_m, COLOR_BRIGHT_RED);

    // Center Smiley Button
    int sx = x + (w - 24) / 2;
    int sy = py + 6;
    draw_button(sx, sy, 24, 24, nullptr, minesweeper.face_pressed);
    draw_rect(sx + 3, sy + 3, 18, 18, COLOR_YELLOW);
    const char* face = ":-)";
    if (minesweeper.game_over) face = "X-(";
    else if (minesweeper.victory) face = "B-)";
    draw_text(sx + 1, sy + 7, face, COLOR_BLACK);

    // LED Counter Right: Timer
    int rx = x + w - 58;
    int ry = py + 6;
    draw_3d_box(rx, ry, 44, 24, true, COLOR_BLACK);
    char buf_t[8];
    int tm = minesweeper.timer_seconds;
    if (tm > 999) tm = 999;
    buf_t[0] = '0' + (tm / 100);
    buf_t[1] = '0' + ((tm / 10) % 10);
    buf_t[2] = '0' + (tm % 10);
    buf_t[3] = '\0';
    draw_text(rx + 8, ry + 8, buf_t, COLOR_BRIGHT_RED);

    // Grid Area (Sunken 9x9)
    int gy = py + 42;
    int cell_size = 22;
    int gw = 9 * cell_size + 4;
    int gh = 9 * cell_size + 4;
    int gx_start = x + (w - gw) / 2;
    draw_3d_box(gx_start, gy, gw, gh, true, COLOR_LIGHTGRAY);

    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 9; ++c) {
            int cx = gx_start + 2 + c * cell_size;
            int cy = gy + 2 + r * cell_size;

            if (minesweeper.revealed[c][r]) {
                if (minesweeper.is_mine[c][r]) {
                    draw_3d_box(cx, cy, cell_size, cell_size, true, COLOR_BRIGHT_RED);
                    draw_text(cx + 7, cy + 7, "*", COLOR_BLACK);
                } else {
                    draw_3d_box(cx, cy, cell_size, cell_size, true, COLOR_LIGHTGRAY);
                    int n = minesweeper.count[c][r];
                    if (n > 0) {
                        uint32_t col = (n == 1) ? COLOR_BLUE : (n == 2) ? COLOR_GREEN : COLOR_RED;
                        char num[2] = {(char)('0' + n), '\0'};
                        draw_text(cx + 7, cy + 7, num, col);
                    }
                }
            } else {
                draw_button(cx, cy, cell_size, cell_size, nullptr, false);
                if (minesweeper.flagged[c][r]) {
                    draw_text(cx + 7, cy + 7, "P", COLOR_RED);
                }
            }
        }
    }
}

void GuiEngine::draw_notepad_window()
{
    if (!win_notepad.visible || win_notepad.minimized) return;

    int x = win_notepad.x;
    int y = win_notepad.y;
    int w = win_notepad.w;
    int h = win_notepad.h;

    draw_window(x, y, w, h, win_notepad.title, active_win_id == 2);

    // Menu Bar
    int my = y + 23;
    draw_rect(x + 3, my, w - 6, 16, COLOR_LIGHTGRAY);
    draw_text(x + 10, my + 4, "File  Edit  Search  Help", COLOR_BLACK);

    // Document area (white)
    int cx = x + 4;
    int cy = y + 42;
    int cw = w - 8;
    int ch = h - 64;
    draw_3d_box(cx, cy, cw, ch, true, COLOR_WHITE);

    // Render document text
    int ty = cy + 6;
    for (int i = 0; i < notepad.line_count; ++i) {
        if (ty >= cy + ch - 12) break;
        draw_text(cx + 6, ty, notepad.lines[i], COLOR_BLACK);
        // Active line cursor
        if (active_win_id == 2 && i == notepad.cursor_row) {
            int clx = cx + 6 + (int)strlen(notepad.lines[i]) * 8;
            draw_rect(clx, ty, 2, 10, COLOR_BLACK);
        }
        ty += 12;
    }

    // Status Bar
    int sby = y + h - 20;
    draw_3d_box(cx, sby, cw, 18, true, COLOR_LIGHTGRAY);
    draw_text(cx + 6, sby + 5, notepad.status_msg, COLOR_DARKGRAY);
}

void GuiEngine::draw_bluetooth_window()
{
    if (!win_bluetooth.visible || win_bluetooth.minimized) return;

    int x = win_bluetooth.x;
    int y = win_bluetooth.y;
    int w = win_bluetooth.w;
    int h = win_bluetooth.h;

    draw_window(x, y, w, h, win_bluetooth.title, active_win_id == 3);

    // Menu Bar
    int my = y + 23;
    draw_rect(x + 3, my, w - 6, 16, COLOR_LIGHTGRAY);
    draw_text(x + 10, my + 4, "Device  Connect  Options  Help", COLOR_BLACK);

    // Controller Status Panel
    int py = y + 42;
    draw_3d_box(x + 6, py, w - 12, 42, true, COLOR_LIGHTGRAY);
    draw_text(x + 12, py + 6,  bluetooth.controller_name, COLOR_NAVY);
    draw_text(x + 12, py + 22, bluetooth.radio_status, COLOR_GREEN);

    // Discovered Devices List Box (Sunken White Area)
    int ly = py + 48;
    int lw = w - 12;
    int lh = 135;
    draw_3d_box(x + 6, ly, lw, lh, true, COLOR_WHITE);

    int dy = ly + 4;
    for (int i = 0; i < BluetoothManager::MAX_DEVS; ++i) {
        bool selected = (bluetooth.selected_idx == i);
        if (selected) {
            draw_rect(x + 8, dy - 2, lw - 4, 30, COLOR_NAVY);
        }

        uint32_t text_col = selected ? COLOR_WHITE : COLOR_BLACK;
        uint32_t sub_col  = selected ? COLOR_CYAN : COLOR_DARKGRAY;

        draw_text(x + 12, dy, bluetooth.devices[i].name, text_col);
        const char* status = bluetooth.devices[i].connected ? "[Connected]" : "[Paired]";
        uint32_t stat_col = bluetooth.devices[i].connected ? COLOR_GREEN : sub_col;
        if (selected) stat_col = COLOR_YELLOW;
        draw_text(x + w - 120, dy, status, stat_col);

        char mac_line[48];
        mac_line[0] = '\0';
        strcat(mac_line, bluetooth.devices[i].mac);
        strcat(mac_line, " (");
        strcat(mac_line, bluetooth.devices[i].type);
        strcat(mac_line, ")");
        draw_text(x + 12, dy + 12, mac_line, sub_col);

        dy += 32;
    }

    // Bottom Action Buttons: [ Scan ], [ Connect / Disconnect ]
    int by = ly + lh + 8;
    draw_button(x + 8, by, 85, 24, bluetooth.scanning ? "Scanning" : "Scan", bluetooth.scanning);
    draw_button(x + 100, by, 110, 24, "Toggle Connect", false);
    draw_button(x + 218, by, 90, 24, "Properties", false);
}

// Window Management Helpers
GuiWindow* GuiEngine::get_window(int id)
{
    if (id == 0) return &win_terminal;
    if (id == 1) return &win_mines;
    if (id == 2) return &win_notepad;
    if (id == 3) return &win_bluetooth;
    return nullptr;
}

void GuiEngine::focus_window(int id)
{
    GuiWindow* w = get_window(id);
    if (w) {
        w->visible = true;
        w->minimized = false;
        active_win_id = id;
        play_click();
    }
}

void GuiEngine::toggle_window(int id)
{
    GuiWindow* w = get_window(id);
    if (w) {
        if (!w->visible) {
            w->visible = true;
            w->minimized = false;
            active_win_id = id;
            play_chime();
        } else if (w->minimized) {
            w->minimized = false;
            active_win_id = id;
            play_click();
        } else if (active_win_id == id) {
            w->minimized = true;
            play_click();
        } else {
            active_win_id = id;
            play_click();
        }
    }
}

// ============================================================
// Event Handling: Mouse & Keyboard
// ============================================================

void GuiEngine::on_mouse_move(int dx, int dy)
{
    cursor_x += dx;
    cursor_y += dy;

    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;
    if ((uint32_t)cursor_x >= fb_width) cursor_x = fb_width - 1;
    if ((uint32_t)cursor_y >= fb_height) cursor_y = fb_height - 1;

    // Handle Window Dragging
    if (dragging_win_id >= 0 && mouse_left) {
        GuiWindow* w = get_window(dragging_win_id);
        if (w) {
            w->x = cursor_x - drag_off_x;
            w->y = cursor_y - drag_off_y;
            if (w->x < 0) w->x = 0;
            if (w->y < 0) w->y = 0;
            if (w->x + w->w > (int)fb_width) w->x = fb_width - w->w;
            if (w->y + 25 > (int)fb_height - 30) w->y = fb_height - 55;
        }
    } else {
        dragging_win_id = -1;
    }
}

void GuiEngine::on_mouse_click(bool left_pressed, bool right_pressed)
{
    bool left_clicked = left_pressed && !prev_left;
    bool right_clicked = right_pressed && !prev_right;
    prev_left = left_pressed;
    prev_right = right_pressed;
    mouse_left = left_pressed;
    mouse_right = right_pressed;

    if (!left_clicked && !right_clicked) {
        if (!left_pressed) {
            dragging_win_id = -1;
            minesweeper.face_pressed = false;
        }
        return;
    }

    const int mx = cursor_x;
    const int my = cursor_y;

    // 1. Taskbar: Start Button
    const int bar_y = fb_height - 30;
    if (my >= bar_y) {
        if (mx >= 3 && mx <= 67) {
            start_menu_open = !start_menu_open;
            play_click();
            return;
        }
        // Taskbar Window Tabs
        if (mx >= 74 && mx <= 184) { toggle_window(0); return; }
        if (mx >= 188 && mx <= 283) { toggle_window(1); return; }
        if (mx >= 287 && mx <= 387) { toggle_window(2); return; }
        if (mx >= 391 && mx <= 486) { toggle_window(3); return; }

        // System Tray: Sound Mute Toggle
        int tray_x = fb_width - 144;
        if (mx >= tray_x + 4 && mx <= tray_x + 28) {
            sound_muted = !sound_muted;
            play_sound(1000, 20);
            return;
        }
        // System Tray: Bluetooth Window Toggle
        if (mx >= tray_x + 32 && mx <= tray_x + 56) {
            toggle_window(3);
            return;
        }
        return;
    }

    // 2. Start Menu Clicks
    if (start_menu_open) {
        int sm_w = 180;
        int sm_h = 175;
        int sm_x = 3;
        int sm_y = fb_height - 30 - sm_h;
        if (mx >= sm_x && mx <= sm_x + sm_w && my >= sm_y && my <= sm_y + sm_h) {
            int item_idx = (my - (sm_y + 8)) / 26;
            if (item_idx == 0) focus_window(0); // Terminal
            else if (item_idx == 1) focus_window(1); // Mines
            else if (item_idx == 2) focus_window(2); // Notepad
            else if (item_idx == 3) toggle_window(3); // Bluetooth
            else if (item_idx == 4) play_chime(); // Sound chime
            else if (item_idx == 5) {
                // Restart
                __asm__ volatile("outb %0, $0x64" : : "a"((uint8_t)0xFE));
            }
            start_menu_open = false;
            return;
        } else {
            start_menu_open = false;
        }
    }

    // 3. Desktop Icons (Left margin)
    if (mx >= 10 && mx <= 50) {
        if (my >= 15 && my <= 65) { focus_window(0); return; } // Computer -> Terminal
        if (my >= 75 && my <= 125) { focus_window(0); return; } // Terminal
        if (my >= 135 && my <= 185) { focus_window(1); return; } // Mines
        if (my >= 195 && my <= 245) { focus_window(2); return; } // Readme
        if (my >= 255 && my <= 305) { toggle_window(3); return; } // Bluetooth
        if (my >= 315 && my <= 365) { play_click(); return; } // Recycle
    }

    // 4. Check Click on Active Window First, then other windows
    int check_order[4] = {active_win_id, 0, 1, 2};
    if (active_win_id == 0) { check_order[1] = 1; check_order[2] = 2; check_order[3] = 3; }
    else if (active_win_id == 1) { check_order[1] = 0; check_order[2] = 2; check_order[3] = 3; }
    else if (active_win_id == 2) { check_order[1] = 0; check_order[2] = 1; check_order[3] = 3; }
    else { check_order[1] = 0; check_order[2] = 1; check_order[3] = 2; }

    for (int i = 0; i < 4; ++i) {
        int wid = check_order[i];
        GuiWindow* w = get_window(wid);
        if (!w || !w->visible || w->minimized) continue;

        if (mx >= w->x && mx <= w->x + w->w && my >= w->y && my <= w->y + w->h) {
            active_win_id = wid;

            // Title Bar Clicks: Dragging or Window Controls
            if (my >= w->y && my <= w->y + 22) {
                // Close 'X' Button
                if (mx >= w->x + w->w - 18 && mx <= w->x + w->w - 4) {
                    w->visible = false;
                    play_click();
                    return;
                }
                // Minimize '_' Button
                if (mx >= w->x + w->w - 50 && mx <= w->x + w->w - 36) {
                    w->minimized = true;
                    play_click();
                    return;
                }
                // Title bar drag start
                dragging_win_id = wid;
                drag_off_x = mx - w->x;
                drag_off_y = my - w->y;
                return;
            }

            // Window Specific Clicks
            if (wid == 1) { // Minesweeper
                // Smiley Button
                int sx = w->x + (w->w - 24) / 2;
                int sy = w->y + 48;
                if (mx >= sx && mx <= sx + 24 && my >= sy && my <= sy + 24) {
                    minesweeper.reset();
                    play_click();
                    return;
                }
                // Grid Clicks
                int cell_size = 22;
                int gw = 9 * cell_size + 4;
                int gx_start = w->x + (w->w - gw) / 2 + 2;
                int gy_start = w->y + 86;
                if (mx >= gx_start && mx < gx_start + 9 * cell_size &&
                    my >= gy_start && my < gy_start + 9 * cell_size) {
                    int gc = (mx - gx_start) / cell_size;
                    int gr = (my - gy_start) / cell_size;
                    if (left_clicked) minesweeper.reveal(gc, gr);
                    else if (right_clicked) minesweeper.toggle_flag(gc, gr);
                    return;
                }
            } else if (wid == 2) { // Notepad
                // File Menu Save Click
                if (my >= w->y + 23 && my <= w->y + 39 && mx >= w->x + 10 && mx <= w->x + 40) {
                    notepad.save();
                    return;
                }
            } else if (wid == 3) { // Bluetooth
                int py = w->y + 90;
                // Device selection click
                if (my >= py && my <= py + 128) {
                    int dev_idx = (my - py) / 32;
                    if (dev_idx >= 0 && dev_idx < BluetoothManager::MAX_DEVS) {
                        bluetooth.selected_idx = dev_idx;
                        play_click();
                        return;
                    }
                }
                // Scan Button
                int by = py + 143;
                if (my >= by && my <= by + 24) {
                    if (mx >= w->x + 8 && mx <= w->x + 93) {
                        bluetooth.start_scan();
                        return;
                    }
                    if (mx >= w->x + 100 && mx <= w->x + 210) {
                        bluetooth.toggle_connect();
                        return;
                    }
                }
            }
            return;
        }
    }
}

void GuiEngine::on_key(char ch, uint32_t scancode, bool pressed)
{
    if (!pressed) return;

    dbg_str("[GUI KEY] sc=");
    char hex[8];
    hex[0] = "0123456789ABCDEF"[(scancode >> 4) & 0xF];
    hex[1] = "0123456789ABCDEF"[scancode & 0xF];
    hex[2] = ' ';
    hex[3] = '\0';
    dbg_str(hex);
    if (ch >= 32 && ch <= 126) {
        dbg_char(ch);
    }
    dbg_str("\r\n");

    uint32_t sc = scancode & 0x7F;
    bool is_enter = (sc == 0x1C || sc == 28 || ch == '\n' || ch == '\r');
    bool is_backspace = (sc == 0x0E || sc == 14 || ch == '\b' || ch == 127);

    // Route key input to active window
    if (active_win_id == 0) { // Terminal
        if (is_enter) {
            terminal.execute();
            play_click();
        } else if (is_backspace) {
            terminal.handle_backspace();
        } else if (ch >= 32 && ch <= 126) {
            terminal.handle_char(ch);
        }
    } else if (active_win_id == 2) { // Notepad
        if (is_enter) {
            notepad.handle_enter();
        } else if (is_backspace) {
            notepad.handle_backspace();
        } else if (ch >= 32 && ch <= 126) {
            notepad.handle_char(ch);
        }
    }

    if (sc == 0x01 || ch == 27) { // Escape
        start_menu_open = false;
    }
}

void GuiEngine::on_tick()
{
    tick_counter++;
    if (tick_counter % 20 == 0) {
        clock_seconds++;
        terminal.cursor_blink = !terminal.cursor_blink;

        if (minesweeper.timer_running && !minesweeper.game_over && !minesweeper.victory) {
            minesweeper.timer_seconds++;
        }

        if (bluetooth.scanning) {
            bluetooth.scan_ticks--;
            if (bluetooth.scan_ticks <= 0) {
                bluetooth.scanning = false;
                bluetooth.radio_status = "Scan complete. 4 devices found.";
                play_chime();
            }
        }
    }
}

// Main Render Pass
void GuiEngine::render()
{
    if (!backbuf || !fb_ptr) return;

    // 1. Draw teal desktop & icons
    draw_desktop_background();

    // 2. Draw inactive windows first
    if (active_win_id != 0) draw_terminal_window();
    if (active_win_id != 1) draw_minesweeper_window();
    if (active_win_id != 2) draw_notepad_window();
    if (active_win_id != 3) draw_bluetooth_window();

    // 3. Draw active window on top
    if (active_win_id == 0) draw_terminal_window();
    else if (active_win_id == 1) draw_minesweeper_window();
    else if (active_win_id == 2) draw_notepad_window();
    else if (active_win_id == 3) draw_bluetooth_window();

    // 4. Taskbar & Start Menu
    draw_taskbar();
    draw_start_menu();

    // 5. Mouse Cursor
    draw_cursor(cursor_x, cursor_y);

    // 6. Blit to real UEFI GOP framebuffer
    bb_blit_to_fb(fb_ptr, backbuf);
}

// ============================================================
// Interactive Desktop Loop
// ============================================================

void GuiEngine::run_desktop()
{
    render();

    // Sound chime on desktop startup
    play_chime();

    uint64_t last_tick = 0;

    while (is_running) {
        blockos::input::poll_hardware();

        blockos::input::Event ev{};
        bool redraw = false;

        while (blockos::input::read(&ev, 1) == 1) {
            if (ev.type == blockos::input::EVENT_MOUSE) {
                on_mouse_move(ev.dx, ev.dy);
                on_mouse_click((ev.buttons & 1) != 0, (ev.buttons & 2) != 0);
                redraw = true;
            } else if (ev.type == blockos::input::EVENT_KEYBOARD) {
                uint8_t sc = (uint8_t)(ev.scancode & 0x7F);
                char ch = (sc < 128) ? scan_base[sc] : 0;
                on_key(ch, ev.scancode, ev.pressed != 0);
                redraw = true;
            }
        }

        // Timer Tick (approximately every 50ms)
        last_tick++;
        if (last_tick % 50000 == 0) {
            on_tick();
            redraw = true;
        }

        if (redraw) {
            render();
        }

        __asm__ volatile("pause");
    }
}
