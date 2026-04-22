#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

#define LEVEL_COUNT 3
#define MAX_ROWS 24
#define MAX_COLS 44
#define MAX_ENEMIES 8
#define MAX_TELEPORTS 8
#define SAVE_FILE "save.txt"

typedef struct {
    int row;
    int col;
} Point;

typedef struct {
    int row;
    int col;
    int startRow;
    int startCol;
    int dirRow;
    int dirCol;
    char type;
} Enemy;

typedef struct {
    int rows;
    int cols;
    const char *data[MAX_ROWS];
} LevelTemplate;

typedef struct {
    int currentLevel;
    int rows;
    int cols;
    int hp;
    int gold;
    int keyCount;
    int requiredKeys;
    int steps;
    int playerRow;
    int playerCol;
    int startRow;
    int startCol;
    int enemyCount;
    int teleportCount;
    char map[MAX_ROWS][MAX_COLS + 1];
    Enemy enemies[MAX_ENEMIES];
    Point teleports[MAX_TELEPORTS];
    char message[128];
} GameState;

static const LevelTemplate LEVELS[LEVEL_COUNT] = {
    {
        13,
        23,
        {
            "#######################",
            "#P....#......#......D.#",
            "#.....#..##..#........#",
            "#.....#...............#",
            "#..######..#####......#",
            "#..........T..........#",
            "#..####....####....#..#",
            "#......O..............#",
            "#.............C.......#",
            "#..####....####....#..#",
            "#..#K.......O....H.#..#",
            "#.....................#",
            "#######################"
        }
    },
    {
        19,
        35,
        {
            "###################################",
            "#P......#.............#.........D.#",
            "#.......#..######.....#...........#",
            "#.......#.............#...........#",
            "#..########..#..##########........#",
            "#...K...T.........................#",
            "#..#####..#####..#####..#####.....#",
            "#..#...#....V...........#...#.....#",
            "#..#...#................#...#.....#",
            "#..#####..#####..#####..#####.....#",
            "#..........O.....C..............H.#",
            "#..#####..#####..#####..#####.....#",
            "#..#...#....O...........#...#.....#",
            "#..#...#................#...#.....#",
            "#..#####..#####..#####..#####.....#",
            "#..............#####......K.......#",
            "#...########..............###.....#",
            "#.................................#",
            "###################################"
        }
    },
    {
        21,
        39,
        {
            "#######################################",
            "#P......#.................#........D..#",
            "#.......#..##########.....#...........#",
            "#.......#.................#...........#",
            "#..#########..#####..###..#####.......#",
            "#..........T..........................#",
            "#..#####..#########..#................#",
            "#..#...#....O........#..........H.....#",
            "#..#...#.............#................#",
            "#..#####..#####..#####....######......#",
            "#......C.........................S....#",
            "#..#####..#####..#####................#",
            "#..#...#....O....#...#................#",
            "#..#...#.........#...#.....#####......#",
            "#..#####..#####..#####................#",
            "#....K.........#####..................#",
            "#...########..............###.........#",
            "#...............T..............V......#",
            "#..##########..............#####......#",
            "#.....................................#",
            "#######################################"
        }
    }
};

#ifdef _WIN32
static HANDLE g_console = NULL;
static WORD g_default_attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
#endif

static void set_cursor_visible(int visible) {
#ifdef _WIN32
    CONSOLE_CURSOR_INFO cursorInfo;

    if (g_console == NULL) {
        return;
    }
    if (GetConsoleCursorInfo(g_console, &cursorInfo)) {
        cursorInfo.bVisible = visible ? TRUE : FALSE;
        SetConsoleCursorInfo(g_console, &cursorInfo);
    }
#else
    (void)visible;
#endif
}

static void setup_console_encoding(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;

    g_console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_console != NULL && GetConsoleScreenBufferInfo(g_console, &consoleInfo)) {
        g_default_attributes = consoleInfo.wAttributes;
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    setlocale(LC_ALL, ".UTF-8");
    set_cursor_visible(0);
}

static void clear_screen(void) {
#ifdef _WIN32
    if (g_console != NULL) {
        COORD topLeft = {0, 0};
        SetConsoleCursorPosition(g_console, topLeft);
        return;
    }
#endif
    int result = system("cls");
    if (result != 0) {
        for (int i = 0; i < 30; ++i) {
            putchar('\n');
        }
    }
}

static int read_game_key(void) {
#ifdef _WIN32
    int ch = _getch();
    if (ch == 0 || ch == 224) {
        (void)_getch();
        return 0;
    }
    return ch;
#else
    return getchar();
#endif
}

static int key_available(void) {
#ifdef _WIN32
    return _kbhit();
#else
    return 1;
#endif
}

static void sleep_ms(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    (void)ms;
#endif
}

static unsigned long long current_time_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    return 0;
#endif
}

static void set_tile_color(char tile) {
#ifdef _WIN32
    if (g_console == NULL) {
        return;
    }

    switch (tile) {
        case 'C':
            SetConsoleTextAttribute(
                g_console,
                FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
            );
            break;
        case 'K':
            SetConsoleTextAttribute(
                g_console,
                FOREGROUND_RED | FOREGROUND_GREEN
            );
            break;
        case 'T':
            SetConsoleTextAttribute(
                g_console,
                FOREGROUND_RED | FOREGROUND_INTENSITY
            );
            break;
        case 'O':
            SetConsoleTextAttribute(
                g_console,
                FOREGROUND_BLUE | FOREGROUND_INTENSITY
            );
            break;
        default:
            SetConsoleTextAttribute(g_console, g_default_attributes);
            break;
    }
#else
    (void)tile;
#endif
}

static void reset_tile_color(void) {
#ifdef _WIN32
    if (g_console != NULL) {
        SetConsoleTextAttribute(g_console, g_default_attributes);
    }
#endif
}

static void set_message(GameState *game, const char *text) {
    snprintf(game->message, sizeof(game->message), "%s", text);
}

static int is_enemy_symbol(char ch) {
    return ch == 'H' || ch == 'V' || ch == 'S';
}

static int enemy_at(const GameState *game, int row, int col) {
    for (int i = 0; i < game->enemyCount; ++i) {
        if (game->enemies[i].row == row && game->enemies[i].col == col) {
            return i;
        }
    }
    return -1;
}

static int tile_blocked_for_player(char tile) {
    return tile == '#';
}

static int tile_blocked_for_enemy(char tile) {
    return tile == '#';
}

static void apply_player_damage(GameState *game, const char *reason) {
    game->hp--;
    if (game->hp > 0) {
        game->playerRow = game->startRow;
        game->playerCol = game->startCol;
        snprintf(game->message, sizeof(game->message), "%s 生命值 -1，已回到起点。", reason);
    } else {
        snprintf(game->message, sizeof(game->message), "%s 生命值归零，游戏失败。", reason);
    }
}

static void add_enemy(GameState *game, int row, int col, char type) {
    Enemy *enemy;

    if (game->enemyCount >= MAX_ENEMIES) {
        return;
    }

    enemy = &game->enemies[game->enemyCount++];
    enemy->row = row;
    enemy->col = col;
    enemy->startRow = row;
    enemy->startCol = col;
    enemy->type = type;
    enemy->dirRow = 0;
    enemy->dirCol = 0;

    if (type == 'H') {
        enemy->dirCol = 1;
    } else if (type == 'V') {
        enemy->dirRow = 1;
    }
}

static void add_teleport(GameState *game, int row, int col) {
    if (game->teleportCount >= MAX_TELEPORTS) {
        return;
    }
    game->teleports[game->teleportCount].row = row;
    game->teleports[game->teleportCount].col = col;
    game->teleportCount++;
}

static void load_level(GameState *game, int levelIndex, int keepProgress) {
    const LevelTemplate *level = &LEVELS[levelIndex];

    if (!keepProgress) {
        game->hp = 3;
        game->gold = 0;
        game->steps = 0;
    }

    game->currentLevel = levelIndex;
    game->rows = level->rows;
    game->cols = level->cols;
    game->keyCount = 0;
    game->requiredKeys = 0;
    game->enemyCount = 0;
    game->teleportCount = 0;

    for (int r = 0; r < game->rows; ++r) {
        for (int c = 0; c < game->cols; ++c) {
            char tile = level->data[r][c];

            if (tile == 'P') {
                game->playerRow = r;
                game->playerCol = c;
                game->startRow = r;
                game->startCol = c;
                game->map[r][c] = '.';
            } else if (is_enemy_symbol(tile)) {
                add_enemy(game, r, c, tile);
                game->map[r][c] = '.';
            } else {
                game->map[r][c] = tile;
                if (tile == 'O') {
                    add_teleport(game, r, c);
                } else if (tile == 'K') {
                    game->requiredKeys++;
                }
            }
        }
        game->map[r][game->cols] = '\0';
    }

    snprintf(
        game->message,
        sizeof(game->message),
        "已进入第 %d 关。先找钥匙，再去开门。",
        game->currentLevel + 1
    );
}

static void start_new_game(GameState *game) {
    memset(game, 0, sizeof(*game));
    load_level(game, 0, 0);
}

static const char *display_symbol(char tile) {
    static char fallback[2];

    switch (tile) {
        case '#':
            return "█";
        case 'P':
            return "@";
        case 'C':
            return "$";
        case 'T':
            return "!";
        case 'H':
        case 'V':
        case 'S':
            return "X";
        default:
            fallback[0] = tile;
            fallback[1] = '\0';
            return fallback;
    }
}

static void draw_tile(char tile) {
    set_tile_color(tile);
    fputs(display_symbol(tile), stdout);
    reset_tile_color();
}

static void draw_game(const GameState *game) {
    clear_screen();
    printf("=== 迷宫游戏（字符版）===\n");
    printf(
        "关卡:%d/%d  生命:%d  金币:%d  钥匙:%d/%d  步数:%d\n",
        game->currentLevel + 1,
        LEVEL_COUNT,
        game->hp,
        game->gold,
        game->keyCount,
        game->requiredKeys,
        game->steps
    );
    printf("操作: 直接按 w/a/s/d 移动, p 保存, o 读档, q 退出\n");
    printf("图例: █墙 @玩家 D门 K钥匙 $金币 !陷阱 O传送 X敌人\n");
    printf("消息: %s\n\n", game->message);

    for (int r = 0; r < game->rows; ++r) {
        for (int c = 0; c < game->cols; ++c) {
            if (game->playerRow == r && game->playerCol == c) {
                draw_tile('P');
            } else {
                int enemyIndex = enemy_at(game, r, c);
                if (enemyIndex >= 0) {
                    draw_tile(game->enemies[enemyIndex].type);
                } else {
                    draw_tile(game->map[r][c]);
                }
            }
        }
        putchar('\n');
    }
}

static int get_teleport_destination(
    const GameState *game,
    int row,
    int col,
    int *targetRow,
    int *targetCol
) {
    if (game->teleportCount < 2) {
        return 0;
    }

    for (int i = 0; i < game->teleportCount; ++i) {
        if (game->teleports[i].row == row && game->teleports[i].col == col) {
            int next = (i + 1) % game->teleportCount;
            *targetRow = game->teleports[next].row;
            *targetCol = game->teleports[next].col;
            return 1;
        }
    }

    return 0;
}

static int is_inside_map(const GameState *game, int row, int col) {
    return row >= 0 && row < game->rows && col >= 0 && col < game->cols;
}

static int save_game(const GameState *game) {
    FILE *fp = fopen(SAVE_FILE, "w");

    if (fp == NULL) {
        return 0;
    }

    fprintf(
        fp,
        "%d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
        game->currentLevel,
        game->rows,
        game->cols,
        game->hp,
        game->gold,
        game->keyCount,
        game->requiredKeys,
        game->steps,
        game->playerRow,
        game->playerCol,
        game->startRow,
        game->startCol,
        game->enemyCount,
        game->teleportCount
    );

    for (int r = 0; r < game->rows; ++r) {
        fprintf(fp, "%s\n", game->map[r]);
    }

    for (int i = 0; i < game->enemyCount; ++i) {
        fprintf(
            fp,
            "%d %d %d %d %d %d %c\n",
            game->enemies[i].row,
            game->enemies[i].col,
            game->enemies[i].startRow,
            game->enemies[i].startCol,
            game->enemies[i].dirRow,
            game->enemies[i].dirCol,
            game->enemies[i].type
        );
    }

    for (int i = 0; i < game->teleportCount; ++i) {
        fprintf(fp, "%d %d\n", game->teleports[i].row, game->teleports[i].col);
    }

    fclose(fp);
    return 1;
}

static int load_game(GameState *game) {
    FILE *fp = fopen(SAVE_FILE, "r");

    if (fp == NULL) {
        return 0;
    }

    if (fscanf(
            fp,
            "%d %d %d %d %d %d %d %d %d %d %d %d %d %d",
            &game->currentLevel,
            &game->rows,
            &game->cols,
            &game->hp,
            &game->gold,
            &game->keyCount,
            &game->requiredKeys,
            &game->steps,
            &game->playerRow,
            &game->playerCol,
            &game->startRow,
            &game->startCol,
            &game->enemyCount,
            &game->teleportCount
        ) != 14) {
        fclose(fp);
        return 0;
    }

    if (game->rows > MAX_ROWS || game->cols > MAX_COLS ||
        game->enemyCount > MAX_ENEMIES || game->teleportCount > MAX_TELEPORTS) {
        fclose(fp);
        return 0;
    }

    for (int r = 0; r < game->rows; ++r) {
        if (fscanf(fp, "%s", game->map[r]) != 1) {
            fclose(fp);
            return 0;
        }
        game->map[r][game->cols] = '\0';
    }

    for (int i = 0; i < game->enemyCount; ++i) {
        if (fscanf(
                fp,
                "%d %d %d %d %d %d %c",
                &game->enemies[i].row,
                &game->enemies[i].col,
                &game->enemies[i].startRow,
                &game->enemies[i].startCol,
                &game->enemies[i].dirRow,
                &game->enemies[i].dirCol,
                &game->enemies[i].type
            ) != 7) {
            fclose(fp);
            return 0;
        }
    }

    for (int i = 0; i < game->teleportCount; ++i) {
        if (fscanf(fp, "%d %d", &game->teleports[i].row, &game->teleports[i].col) != 2) {
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    set_message(game, "读档成功。");
    return 1;
}

static int try_advance_level(GameState *game) {
    if (game->currentLevel + 1 >= LEVEL_COUNT) {
        set_message(game, "你已经通关全部关卡。");
        return 1;
    }

    load_level(game, game->currentLevel + 1, 1);
    return 0;
}

static int handle_special_tile(GameState *game) {
    char tile = game->map[game->playerRow][game->playerCol];

    if (tile == 'K') {
        game->keyCount++;
        game->map[game->playerRow][game->playerCol] = '.';
        snprintf(
            game->message,
            sizeof(game->message),
            "你拿到了钥匙。当前进度 %d/%d。",
            game->keyCount,
            game->requiredKeys
        );
    } else if (tile == 'C') {
        game->gold++;
        game->map[game->playerRow][game->playerCol] = '.';
        set_message(game, "你捡到了一枚金币。");
    } else if (tile == 'T') {
        apply_player_damage(game, "你踩中了陷阱。");
        if (game->hp <= 0) {
            return -1;
        }
    } else if (tile == 'O') {
        int targetRow;
        int targetCol;
        if (get_teleport_destination(game, game->playerRow, game->playerCol, &targetRow, &targetCol)) {
            game->playerRow = targetRow;
            game->playerCol = targetCol;
            set_message(game, "你被传送到了另一处。");
            if (enemy_at(game, game->playerRow, game->playerCol) >= 0) {
                apply_player_damage(game, "你刚传送出来就撞上了敌人。");
                if (game->hp <= 0) {
                    return -1;
                }
            }
        }
    } else if (tile == 'D') {
        if (game->keyCount >= game->requiredKeys) {
            return 1;
        }
        snprintf(
            game->message,
            sizeof(game->message),
            "钥匙还没找全，当前 %d/%d，无法开门。",
            game->keyCount,
            game->requiredKeys
        );
    }

    return 0;
}

static void move_horizontal_enemy(GameState *game, Enemy *enemy) {
    int nextCol = enemy->col + enemy->dirCol;
    if (tile_blocked_for_enemy(game->map[enemy->row][nextCol])) {
        enemy->dirCol = -enemy->dirCol;
        nextCol = enemy->col + enemy->dirCol;
    }
    if (!tile_blocked_for_enemy(game->map[enemy->row][nextCol])) {
        enemy->col = nextCol;
    }
}

static void move_vertical_enemy(GameState *game, Enemy *enemy) {
    int nextRow = enemy->row + enemy->dirRow;
    if (tile_blocked_for_enemy(game->map[nextRow][enemy->col])) {
        enemy->dirRow = -enemy->dirRow;
        nextRow = enemy->row + enemy->dirRow;
    }
    if (!tile_blocked_for_enemy(game->map[nextRow][enemy->col])) {
        enemy->row = nextRow;
    }
}

static void move_chasing_enemy(const GameState *game, Enemy *enemy) {
    int rowStep = 0;
    int colStep = 0;

    if (game->playerRow > enemy->row) {
        rowStep = 1;
    } else if (game->playerRow < enemy->row) {
        rowStep = -1;
    }

    if (game->playerCol > enemy->col) {
        colStep = 1;
    } else if (game->playerCol < enemy->col) {
        colStep = -1;
    }

    if (abs(game->playerRow - enemy->row) >= abs(game->playerCol - enemy->col)) {
        if (rowStep != 0 && !tile_blocked_for_enemy(game->map[enemy->row + rowStep][enemy->col])) {
            enemy->row += rowStep;
            return;
        }
        if (colStep != 0 && !tile_blocked_for_enemy(game->map[enemy->row][enemy->col + colStep])) {
            enemy->col += colStep;
        }
    } else {
        if (colStep != 0 && !tile_blocked_for_enemy(game->map[enemy->row][enemy->col + colStep])) {
            enemy->col += colStep;
            return;
        }
        if (rowStep != 0 && !tile_blocked_for_enemy(game->map[enemy->row + rowStep][enemy->col])) {
            enemy->row += rowStep;
        }
    }
}

static int move_enemies(GameState *game) {
    for (int i = 0; i < game->enemyCount; ++i) {
        Enemy *enemy = &game->enemies[i];

        if (enemy->type == 'H') {
            move_horizontal_enemy(game, enemy);
        } else if (enemy->type == 'V') {
            move_vertical_enemy(game, enemy);
        } else if (enemy->type == 'S') {
            move_chasing_enemy(game, enemy);
        }

        if (enemy->row == game->playerRow && enemy->col == game->playerCol) {
            apply_player_damage(game, "你被敌人击中了。");
            return game->hp <= 0 ? -1 : 1;
        }
    }

    return 0;
}

static int process_move(GameState *game, int dRow, int dCol) {
    int nextRow = game->playerRow + dRow;
    int nextCol = game->playerCol + dCol;
    char tile;

    if (!is_inside_map(game, nextRow, nextCol)) {
        set_message(game, "不能走出地图。");
        return 0;
    }

    tile = game->map[nextRow][nextCol];
    if (tile_blocked_for_player(tile)) {
        set_message(game, "前面是墙。");
        return 0;
    }

    if (tile == 'D' && game->keyCount < game->requiredKeys) {
        snprintf(
            game->message,
            sizeof(game->message),
            "钥匙还没找全，当前 %d/%d，无法开门。",
            game->keyCount,
            game->requiredKeys
        );
        return 0;
    }

    game->playerRow = nextRow;
    game->playerCol = nextCol;
    game->steps++;

    if (enemy_at(game, game->playerRow, game->playerCol) >= 0) {
        apply_player_damage(game, "你主动撞上了敌人。");
        return game->hp <= 0 ? -1 : 0;
    }

    {
        int special = handle_special_tile(game);
        if (special == -1) {
            return -1;
        }
        if (special == 1) {
            return 2;
        }
    }

    return 1;
}

int main(void) {
    GameState game;
    char input[32];
    int inputChar;
    int needsRedraw = 1;
    int running = 1;
    int victory = 0;
    unsigned long long lastEnemyMove = 0;

    setup_console_encoding();
    clear_screen();
    printf("=== 迷宫游戏（字符版）===\n");
    printf("1. 新游戏\n");
    printf("2. 读取存档\n");
    printf("请选择：");

    if (fgets(input, sizeof(input), stdin) == NULL) {
        return 0;
    }

    if (input[0] == '2') {
        memset(&game, 0, sizeof(game));
        if (!load_game(&game)) {
            start_new_game(&game);
            set_message(&game, "没有找到有效存档，已开始新游戏。");
        }
    } else {
        start_new_game(&game);
    }

    lastEnemyMove = current_time_ms();

    while (running) {
        int stateAfterMove = 0;

        if (needsRedraw) {
            draw_game(&game);
            needsRedraw = 0;
        }

        if (game.hp <= 0) {
            break;
        }

        if (current_time_ms() - lastEnemyMove >= 800) {
            int enemyResult = move_enemies(&game);
            lastEnemyMove = current_time_ms();
            needsRedraw = 1;
            if (enemyResult == -1) {
                continue;
            }
        }

        if (!key_available()) {
            sleep_ms(20);
            continue;
        }

        inputChar = read_game_key();
        if (inputChar == EOF) {
            break;
        }
        if (inputChar == 0) {
            continue;
        }

        switch (tolower((unsigned char)inputChar)) {
            case 'w':
                stateAfterMove = process_move(&game, -1, 0);
                break;
            case 's':
                stateAfterMove = process_move(&game, 1, 0);
                break;
            case 'a':
                stateAfterMove = process_move(&game, 0, -1);
                break;
            case 'd':
                stateAfterMove = process_move(&game, 0, 1);
                break;
            case 'p':
                if (save_game(&game)) {
                    set_message(&game, "保存成功。");
                } else {
                    set_message(&game, "保存失败。");
                }
                needsRedraw = 1;
                continue;
            case 'o':
                if (load_game(&game)) {
                    set_message(&game, "读档成功。");
                } else {
                    set_message(&game, "读档失败，没有找到有效存档。");
                }
                lastEnemyMove = current_time_ms();
                needsRedraw = 1;
                continue;
            case 'q':
                running = 0;
                continue;
            default:
                set_message(&game, "无效操作，请输入 w/a/s/d/p/o/q。");
                needsRedraw = 1;
                continue;
        }

        needsRedraw = 1;

        if (stateAfterMove == -1) {
            continue;
        }

        if (stateAfterMove == 2) {
            if (try_advance_level(&game)) {
                victory = 1;
                break;
            }
            lastEnemyMove = current_time_ms();
            continue;
        }
    }

    draw_game(&game);
    if (victory) {
        printf("\nYou Win!\n");
        printf("你成功完成了全部 3 个关卡，最终金币数：%d，步数：%d\n", game.gold, game.steps);
        printf("按任意键退出...");
        fflush(stdout);
        (void)read_game_key();
    } else if (game.hp <= 0) {
        printf("\n游戏结束。最终金币数：%d，步数：%d\n", game.gold, game.steps);
    } else {
        printf("\n你已退出游戏。\n");
    }

    set_cursor_visible(1);
    return 0;
}
