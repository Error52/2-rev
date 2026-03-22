#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#define APP_CLASS_NAME "IronDominionCTFWindow"
#define APP_TITLE "Iron Dominion: CTF Edition"

#define FLAG "flag{revers_n0_ez}"
#define ENEMY_MULTIPLIER 2
#define MAX_TURNS 18

#define IDC_STATUS 1001
#define IDC_ACTION_LOG 1002
#define IDC_BTN_DEVELOP 1101
#define IDC_BTN_RECRUIT 1102
#define IDC_BTN_RAID 1103
#define IDC_BTN_ASSAULT 1104
#define IDC_BTN_RESTART 1105

typedef struct Faction {
    int food;
    int army;
    int tech;
} Faction;

typedef struct GameState {
    Faction player;
    Faction enemy;
    int enemy_fort;
    int turn;
    int player_mult;
    int finished;
} GameState;

static const uint8_t encoded_player_mult[4] = {0x34, 0x34, 0x34, 0x34};

static HWND g_status = NULL;
static HWND g_action_log = NULL;
static HWND g_btn_develop = NULL;
static HWND g_btn_recruit = NULL;
static HWND g_btn_raid = NULL;
static HWND g_btn_assault = NULL;
static HWND g_btn_restart = NULL;
static GameState g_game;

static int decode_player_multiplier(void) {
    uint8_t a = encoded_player_mult[0] ^ 0x34;
    uint8_t b = encoded_player_mult[1] ^ 0x34;
    uint8_t c = encoded_player_mult[2] ^ 0x34;
    uint8_t d = encoded_player_mult[3] ^ 0x34;

    int value = (a + b + c + d) - 3;
    if (value < 1) {
        value = 1;
    }
    return value;
}

static void format_status_text(char* out, size_t out_size, const GameState* gs) {
    _snprintf(
        out,
        out_size,
        "Turn: %d/%d\r\n"
        "Player  | food=%d army=%d tech=%d\r\n"
        "Enemy   | food=%d army=%d tech=%d fort=%d\r\n"
        "Enemy bonus: x2 (fixed)\r\n"
        "Goal: capture enemy capital before turn %d",
        gs->turn,
        MAX_TURNS,
        gs->player.food,
        gs->player.army,
        gs->player.tech,
        gs->enemy.food,
        gs->enemy.army,
        gs->enemy.tech,
        gs->enemy_fort,
        MAX_TURNS
    );
}

static void append_action_log(const char* text) {
    char current[2048];
    char combined[3072];

    GetWindowTextA(g_action_log, current, sizeof(current));
    _snprintf(combined, sizeof(combined), "%s\r\n%s", current, text);
    SetWindowTextA(g_action_log, combined);
}

static void set_action_log(const char* text) {
    SetWindowTextA(g_action_log, text);
}

static void update_ui_status(void) {
    char status_buf[1024];
    format_status_text(status_buf, sizeof(status_buf), &g_game);
    SetWindowTextA(g_status, status_buf);
}

static void set_buttons_enabled(BOOL enabled_actions, BOOL enabled_restart) {
    EnableWindow(g_btn_develop, enabled_actions);
    EnableWindow(g_btn_recruit, enabled_actions);
    EnableWindow(g_btn_raid, enabled_actions);
    EnableWindow(g_btn_assault, enabled_actions);
    EnableWindow(g_btn_restart, enabled_restart);
}

static void init_game_state(GameState* gs) {
    gs->player.food = 22;
    gs->player.army = 18;
    gs->player.tech = 1;

    gs->enemy.food = 26;
    gs->enemy.army = 24;
    gs->enemy.tech = 2;

    gs->enemy_fort = 10;
    gs->turn = 1;
    gs->player_mult = decode_player_multiplier();
    gs->finished = 0;
}

static int can_capture_capital(const GameState* gs) {
    int player_power = (gs->player.army + gs->player.tech * 3) * gs->player_mult;
    int enemy_power = (gs->enemy.army + gs->enemy.tech * 2 + gs->enemy_fort * 4) * ENEMY_MULTIPLIER;

    if (gs->player_mult < 3) {
        return 0;
    }

    return player_power > enemy_power;
}

static void enemy_turn(GameState* gs) {
    gs->enemy.food += 6 * ENEMY_MULTIPLIER;
    gs->enemy.tech += 2 * ENEMY_MULTIPLIER;

    if (gs->enemy.food >= 14) {
        gs->enemy.food -= 14;
        gs->enemy.army += (11 + gs->enemy.tech / 2) * ENEMY_MULTIPLIER;
    } else {
        gs->enemy.army += (5 + gs->enemy.tech / 3) * ENEMY_MULTIPLIER;
    }

    if (gs->enemy.army > 30) {
        int raid = 4 * ENEMY_MULTIPLIER;
        gs->player.army -= raid;
        if (gs->player.army < 0) {
            gs->player.army = 0;
        }
    }
}

static void handle_player_develop(GameState* gs) {
    gs->player.food += 8 * gs->player_mult;
    gs->player.tech += 2 * gs->player_mult;
    gs->player.army += 3 * gs->player_mult;
    append_action_log("You invested in development.");
}

static void handle_player_recruit(GameState* gs) {
    if (gs->player.food >= 12) {
        gs->player.food -= 12;
        gs->player.army += (12 + gs->player.tech / 2) * gs->player_mult;
        append_action_log("You recruited new troops.");
        return;
    }

    gs->player.food += 2 * gs->player_mult;
    append_action_log("Not enough food to recruit. You improvised supply lines.");
}

static void handle_player_raid(GameState* gs) {
    if (gs->player.army >= 8) {
        int damage = (6 + gs->player.tech) * gs->player_mult;
        int loss = 3 * ENEMY_MULTIPLIER;

        gs->enemy.army -= damage;
        if (gs->enemy.army < 0) {
            gs->enemy.army = 0;
        }

        gs->player.army -= loss;
        if (gs->player.army < 0) {
            gs->player.army = 0;
        }

        gs->enemy_fort -= gs->player_mult;
        if (gs->enemy_fort < 0) {
            gs->enemy_fort = 0;
        }

        append_action_log("Raid succeeded: enemy army and fortification weakened.");
        return;
    }

    gs->player.food += 2;
    append_action_log("Raid aborted: army too small, supply gains only.");
}

static void finish_with_defeat(HWND hwnd, const char* reason) {
    g_game.finished = 1;
    set_buttons_enabled(FALSE, TRUE);
    append_action_log(reason);
    MessageBoxA(hwnd, "Defeat. Enemy empire crushed your kingdom.", APP_TITLE, MB_OK | MB_ICONERROR);
}

static void finish_with_victory(HWND hwnd) {
    char msg[256];
    g_game.finished = 1;
    set_buttons_enabled(FALSE, TRUE);
    append_action_log("Assault succeeded. You captured the enemy capital.");
    _snprintf(msg, sizeof(msg), "Victory! Flag: %s", FLAG);
    MessageBoxA(hwnd, msg, APP_TITLE, MB_OK | MB_ICONINFORMATION);
}

static void apply_end_of_turn_rules(HWND hwnd) {
    if (g_game.finished) {
        return;
    }

    enemy_turn(&g_game);

    if (g_game.player.army == 0 && g_game.player.food < 10) {
        finish_with_defeat(hwnd, "Your kingdom is exhausted and collapses.");
        return;
    }

    g_game.turn += 1;
    if (g_game.turn > MAX_TURNS) {
        finish_with_defeat(hwnd, "Time is over. You failed to conquer the capital.");
    }
}

static void perform_action(HWND hwnd, int action_id) {
    if (g_game.finished) {
        return;
    }

    switch (action_id) {
        case IDC_BTN_DEVELOP:
            handle_player_develop(&g_game);
            break;
        case IDC_BTN_RECRUIT:
            handle_player_recruit(&g_game);
            break;
        case IDC_BTN_RAID:
            handle_player_raid(&g_game);
            break;
        case IDC_BTN_ASSAULT:
            if (can_capture_capital(&g_game)) {
                finish_with_victory(hwnd);
                update_ui_status();
                return;
            }
            g_game.player.army /= 2;
            append_action_log("Assault failed. Heavy losses on retreat.");
            break;
        default:
            return;
    }

    apply_end_of_turn_rules(hwnd);
    update_ui_status();
}

static void restart_game(void) {
    init_game_state(&g_game);
    set_action_log(
        "Welcome Commander!\r\n"
        "Enemy has fixed x2 strength bonus.\r\n"
        "To get the flag, victory is required.\r\n"
        "Hint for players: this is a reverse challenge."
    );
    set_buttons_enabled(TRUE, FALSE);
    update_ui_status();
}

static void create_child_controls(HWND hwnd) {
    g_status = CreateWindowExA(
        0,
        "STATIC",
        "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20,
        20,
        540,
        120,
        hwnd,
        (HMENU)IDC_STATUS,
        GetModuleHandle(NULL),
        NULL
    );

    g_action_log = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        20,
        150,
        540,
        220,
        hwnd,
        (HMENU)IDC_ACTION_LOG,
        GetModuleHandle(NULL),
        NULL
    );

    g_btn_develop = CreateWindowExA(0, "BUTTON", "Develop", WS_CHILD | WS_VISIBLE,
        20, 390, 120, 36, hwnd, (HMENU)IDC_BTN_DEVELOP, GetModuleHandle(NULL), NULL);
    g_btn_recruit = CreateWindowExA(0, "BUTTON", "Recruit", WS_CHILD | WS_VISIBLE,
        150, 390, 120, 36, hwnd, (HMENU)IDC_BTN_RECRUIT, GetModuleHandle(NULL), NULL);
    g_btn_raid = CreateWindowExA(0, "BUTTON", "Raid", WS_CHILD | WS_VISIBLE,
        280, 390, 120, 36, hwnd, (HMENU)IDC_BTN_RAID, GetModuleHandle(NULL), NULL);
    g_btn_assault = CreateWindowExA(0, "BUTTON", "Assault", WS_CHILD | WS_VISIBLE,
        410, 390, 120, 36, hwnd, (HMENU)IDC_BTN_ASSAULT, GetModuleHandle(NULL), NULL);
    g_btn_restart = CreateWindowExA(0, "BUTTON", "Restart", WS_CHILD | WS_VISIBLE,
        410, 435, 120, 32, hwnd, (HMENU)IDC_BTN_RESTART, GetModuleHandle(NULL), NULL);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            create_child_controls(hwnd);
            restart_game();
            return 0;

        case WM_COMMAND: {
            int id = LOWORD(wParam);

            if (id == IDC_BTN_RESTART) {
                restart_game();
                return 0;
            }

            perform_action(hwnd, id);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static int run_app(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEXA wc;
    HWND hwnd;
    MSG msg;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = APP_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Cannot register window class.", APP_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowExA(
        0,
        APP_CLASS_NAME,
        APP_TITLE,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        600,
        520,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Cannot create app window.", APP_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    return run_app(hInstance, nCmdShow);
}
