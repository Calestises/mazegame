#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

#define LEVEL_COUNT 3
#define MAX_ROWS 24
#define MAX_COLS 44
#define MAX_TELEPORTS 8
#define SAVE_FILE "save.txt"

typedef struct {
    int row;
    int col;
} Point;

typedef struct {
    int rows;
    int cols;
    int keyTotal;
    int goldTotal;
    int teleportPairs;
} LevelConfig;

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
    int teleportCount;
    char map[MAX_ROWS][MAX_COLS + 1];
    Point teleports[MAX_TELEPORTS];
    char message[128];
} GameState;

static const LevelConfig LEVELS[LEVEL_COUNT] = {
    {13, 23, 1, 1, 1},
    {19, 35, 2, 2, 1},
    {21, 39, 2, 2, 1}
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
        CONSOLE_SCREEN_BUFFER_INFO info;
        COORD topLeft = {0, 0};
        DWORD written = 0;

        if (GetConsoleScreenBufferInfo(g_console, &info)) {
            DWORD cellCount = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
            FillConsoleOutputCharacterA(g_console, ' ', cellCount, topLeft, &written);
            FillConsoleOutputAttribute(
                g_console,
                info.wAttributes,
                cellCount,
                topLeft,
                &written
            );
            SetConsoleCursorPosition(g_console, topLeft);
            return;
        }
    }
#endif
    int result = system("cls");
    if (result != 0) {
        for (int i = 0; i < 30; ++i) {
            putchar('\n');
        }
    }
}

static void move_cursor_home(void) {
#ifdef _WIN32
    if (g_console != NULL) {
        COORD topLeft = {0, 0};
        SetConsoleCursorPosition(g_console, topLeft);
        return;
    }
#endif
    fputs("\033[H", stdout);
}

static void clear_to_line_end(void) {
#ifdef _WIN32
    if (g_console != NULL) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        DWORD written = 0;

        if (GetConsoleScreenBufferInfo(g_console, &info)) {
            DWORD cellCount = (DWORD)(info.dwSize.X - info.dwCursorPosition.X);
            FillConsoleOutputCharacterA(
                g_console,
                ' ',
                cellCount,
                info.dwCursorPosition,
                &written
            );
            FillConsoleOutputAttribute(
                g_console,
                g_default_attributes,
                cellCount,
                info.dwCursorPosition,
                &written
            );
        }
        return;
    }
#endif
    fputs("\033[K", stdout);
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

static int tile_blocked_for_player(char tile) {
    return tile == '#';
}

static void add_teleport(GameState *game, int row, int col) {
    if (game->teleportCount >= MAX_TELEPORTS) {
        return;
    }
    game->teleports[game->teleportCount].row = row;
    game->teleports[game->teleportCount].col = col;
    game->teleportCount++;
}

static void seed_random(void) {
    static int seeded = 0;

    if (seeded) {
        return;
    }

    seeded = 1;
#ifdef _WIN32
    srand((unsigned int)(time(NULL) ^ GetTickCount()));
#else
    srand((unsigned int)time(NULL));
#endif
}

static void initialize_map(GameState *game, int rows, int cols) {
    game->rows = rows;
    game->cols = cols;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            game->map[r][c] = '#';
        }
        game->map[r][cols] = '\0';
    }
}

static void shuffle_ints(int *values, int count) {
    for (int i = count - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int temp = values[i];
        values[i] = values[j];
        values[j] = temp;
    }
}

static void generate_maze_layout(GameState *game) {
    int stackRows[MAX_ROWS * MAX_COLS];
    int stackCols[MAX_ROWS * MAX_COLS];
    int top = 0;
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    stackRows[0] = 1;
    stackCols[0] = 1;
    game->map[1][1] = '.';

    while (top >= 0) {
        int row = stackRows[top];
        int col = stackCols[top];
        int dirs[4] = {0, 1, 2, 3};
        int carved = 0;

        shuffle_ints(dirs, 4);

        for (int i = 0; i < 4; ++i) {
            int dir = dirs[i];
            int nextRow = row + dr[dir] * 2;
            int nextCol = col + dc[dir] * 2;

            if (nextRow <= 0 || nextRow >= game->rows - 1 ||
                nextCol <= 0 || nextCol >= game->cols - 1) {
                continue;
            }

            if (game->map[nextRow][nextCol] != '#') {
                continue;
            }

            game->map[row + dr[dir]][col + dc[dir]] = '.';
            game->map[nextRow][nextCol] = '.';
            ++top;
            stackRows[top] = nextRow;
            stackCols[top] = nextCol;
            carved = 1;
            break;
        }

        if (!carved) {
            --top;
        }
    }
}

static void compute_distances(const GameState *game, int startRow, int startCol, int dist[MAX_ROWS][MAX_COLS]) {
    Point queue[MAX_ROWS * MAX_COLS];
    int head = 0;
    int tail = 0;
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    for (int r = 0; r < game->rows; ++r) {
        for (int c = 0; c < game->cols; ++c) {
            dist[r][c] = -1;
        }
    }

    dist[startRow][startCol] = 0;
    queue[tail++] = (Point){startRow, startCol};

    while (head < tail) {
        Point current = queue[head++];

        for (int i = 0; i < 4; ++i) {
            int nextRow = current.row + dr[i];
            int nextCol = current.col + dc[i];

            if (nextRow < 0 || nextRow >= game->rows || nextCol < 0 || nextCol >= game->cols) {
                continue;
            }
            if (game->map[nextRow][nextCol] == '#') {
                continue;
            }
            if (dist[nextRow][nextCol] != -1) {
                continue;
            }

            dist[nextRow][nextCol] = dist[current.row][current.col] + 1;
            queue[tail++] = (Point){nextRow, nextCol};
        }
    }
}

static int count_open_neighbors(const GameState *game, int row, int col) {
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    int count = 0;

    for (int i = 0; i < 4; ++i) {
        int nextRow = row + dr[i];
        int nextCol = col + dc[i];

        if (nextRow < 0 || nextRow >= game->rows || nextCol < 0 || nextCol >= game->cols) {
            continue;
        }
        if (game->map[nextRow][nextCol] != '#') {
            ++count;
        }
    }

    return count;
}

static Point find_farthest_point(const GameState *game, int dist[MAX_ROWS][MAX_COLS]) {
    Point farthest = {1, 1};
    int bestDistance = -1;

    for (int r = 1; r < game->rows - 1; ++r) {
        for (int c = 1; c < game->cols - 1; ++c) {
            if (game->map[r][c] == '#' || dist[r][c] <= bestDistance) {
                continue;
            }
            bestDistance = dist[r][c];
            farthest.row = r;
            farthest.col = c;
        }
    }

    return farthest;
}

static int choose_random_empty_cell(
    const GameState *game,
    int dist[MAX_ROWS][MAX_COLS],
    int minDistance,
    int requireDeadEnd,
    Point *chosen
) {
    Point candidates[MAX_ROWS * MAX_COLS];
    int candidateCount = 0;

    for (int threshold = minDistance; threshold >= 1; --threshold) {
        candidateCount = 0;

        for (int r = 1; r < game->rows - 1; ++r) {
            for (int c = 1; c < game->cols - 1; ++c) {
                if (game->map[r][c] != '.') {
                    continue;
                }
                if (r == game->startRow && c == game->startCol) {
                    continue;
                }
                if (dist[r][c] < threshold) {
                    continue;
                }
                if (requireDeadEnd && count_open_neighbors(game, r, c) != 1) {
                    continue;
                }
                candidates[candidateCount++] = (Point){r, c};
            }
        }

        if (candidateCount > 0) {
            *chosen = candidates[rand() % candidateCount];
            return 1;
        }
    }

    return 0;
}

static void place_tile_items(
    GameState *game,
    char tile,
    int count,
    int dist[MAX_ROWS][MAX_COLS],
    int minDistance,
    int preferDeadEnd
) {
    for (int i = 0; i < count; ++i) {
        Point target;

        if (!choose_random_empty_cell(game, dist, minDistance, preferDeadEnd, &target)) {
            if (!choose_random_empty_cell(game, dist, minDistance, 0, &target)) {
                return;
            }
        }

        game->map[target.row][target.col] = tile;
        if (tile == 'O') {
            add_teleport(game, target.row, target.col);
        }
    }
}

static void generate_level(GameState *game, const LevelConfig *level) {
    int dist[MAX_ROWS][MAX_COLS];
    Point door;

    initialize_map(game, level->rows, level->cols);
    generate_maze_layout(game);

    game->startRow = 1;
    game->startCol = 1;
    game->playerRow = 1;
    game->playerCol = 1;
    game->map[1][1] = '.';

    compute_distances(game, game->startRow, game->startCol, dist);
    door = find_farthest_point(game, dist);
    game->map[door.row][door.col] = 'D';

    game->requiredKeys = level->keyTotal;
    place_tile_items(game, 'K', level->keyTotal, dist, 8, 1);
    place_tile_items(game, 'C', level->goldTotal, dist, 4, 0);
    place_tile_items(game, 'O', level->teleportPairs * 2, dist, 4, 1);
}

static void load_level(GameState *game, int levelIndex, int keepProgress) {
    const LevelConfig *level = &LEVELS[levelIndex];

    if (!keepProgress) {
        game->hp = 3;
        game->gold = 0;
        game->steps = 0;
    }

    game->currentLevel = levelIndex;
    game->keyCount = 0;
    game->teleportCount = 0;
    generate_level(game, level);

    snprintf(
        game->message,
        sizeof(game->message),
        "已进入第 %d 关，先找钥匙，再去开门",
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
        case '.':
            return " ";
        case 'P':
            return "@";
        case 'C':
            return "$";
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
    static int previousFrameLines = 0;
    int currentFrameLines = 0;

    move_cursor_home();

    printf("=== 迷宫游戏(神秘版) ===");
    clear_to_line_end();
    putchar('\n');
    currentFrameLines++;
    printf(
        "关卡:%d/%d  生命:%d  金币:%d  钥匙:%d/%d  步数:%d",
        game->currentLevel + 1,
        LEVEL_COUNT,
        game->hp,
        game->gold,
        game->keyCount,
        game->requiredKeys,
        game->steps
    );
    clear_to_line_end();
    putchar('\n');
    currentFrameLines++;
    printf("操作: 直接按 w/a/s/d 移动, p 保存, o 读档, q 退出");
    clear_to_line_end();
    putchar('\n');
    currentFrameLines++;
    printf("图例: █墙 @玩家 D门 K钥匙 $金币 O传送");
    clear_to_line_end();
    putchar('\n');
    currentFrameLines++;
    printf("消息: %s", game->message);
    clear_to_line_end();
    putchar('\n');
    currentFrameLines++;
    clear_to_line_end();
    putchar('\n');
    currentFrameLines++;

    for (int r = 0; r < game->rows; ++r) {
        for (int c = 0; c < game->cols; ++c) {
            if (game->playerRow == r && game->playerCol == c) {
                draw_tile('P');
            } else {
                draw_tile(game->map[r][c]);
            }
        }
        clear_to_line_end();
        putchar('\n');
        currentFrameLines++;
    }

    for (int r = currentFrameLines; r < previousFrameLines; ++r) {
        clear_to_line_end();
        putchar('\n');
    }

    previousFrameLines = currentFrameLines;
    fflush(stdout);
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
        "%d %d %d %d %d %d %d %d %d %d %d %d %d\n",
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
        game->teleportCount
    );

    for (int r = 0; r < game->rows; ++r) {
        fprintf(fp, "%s\n", game->map[r]);
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
            "%d %d %d %d %d %d %d %d %d %d %d %d %d",
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
            &game->teleportCount
        ) != 12) {
        fclose(fp);
        return 0;
    }

    if (game->rows > MAX_ROWS || game->cols > MAX_COLS ||
        game->teleportCount > MAX_TELEPORTS) {
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

    for (int i = 0; i < game->teleportCount; ++i) {
        if (fscanf(fp, "%d %d", &game->teleports[i].row, &game->teleports[i].col) != 2) {
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    set_message(game, "读档成功");
    return 1;
}

static int try_advance_level(GameState *game) {
    if (game->currentLevel + 1 >= LEVEL_COUNT) {
        set_message(game, "你已经通关全部关卡");
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
            "你拿到了钥匙，当前进度 %d/%d",
            game->keyCount,
            game->requiredKeys
        );
    } else if (tile == 'C') {
        game->gold++;
        game->map[game->playerRow][game->playerCol] = '.';
        set_message(game, "你捡到了一枚金币");
    } else if (tile == 'O') {
        int targetRow;
        int targetCol;
        if (get_teleport_destination(game, game->playerRow, game->playerCol, &targetRow, &targetCol)) {
            game->playerRow = targetRow;
            game->playerCol = targetCol;
            set_message(game, "你被传送到了另一处");
        }
    } else if (tile == 'D') {
        if (game->keyCount >= game->requiredKeys) {
            return 1;
        }
        snprintf(
            game->message,
            sizeof(game->message),
            "钥匙还没找全，当前 %d/%d，无法开门",
            game->keyCount,
            game->requiredKeys
        );
    }

    return 0;
}

static int process_move(GameState *game, int dRow, int dCol) {
    int nextRow = game->playerRow + dRow;
    int nextCol = game->playerCol + dCol;
    char tile;

    if (!is_inside_map(game, nextRow, nextCol)) {
        set_message(game, "不能走出地图");
        return 0;
    }

    tile = game->map[nextRow][nextCol];
    if (tile_blocked_for_player(tile)) {
        set_message(game, "前面是墙");
        return 0;
    }

    if (tile == 'D' && game->keyCount < game->requiredKeys) {
        snprintf(
            game->message,
            sizeof(game->message),
            "钥匙还没找全，当前 %d/%d，无法开门",
            game->keyCount,
            game->requiredKeys
        );
        return 0;
    }

    game->playerRow = nextRow;
    game->playerCol = nextCol;
    game->steps++;

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
    int running = 1;
    int victory = 0;

    setup_console_encoding();
    seed_random();
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
            set_message(&game, "没有找到有效存档，已开始新游戏");
        }
    } else {
        start_new_game(&game);
    }

    while (running) {
        int stateAfterMove = 0;
        draw_game(&game);

        if (game.hp <= 0) {
            break;
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
                    set_message(&game, "保存成功");
                } else {
                    set_message(&game, "保存失败");
                }
                continue;
            case 'o':
                if (load_game(&game)) {
                    set_message(&game, "读档成功");
                } else {
                    set_message(&game, "读档失败，没有找到有效存档");
                }
                continue;
            case 'q':
                running = 0;
                continue;
            default:
                set_message(&game, "无效操作，请输入 w/a/s/d/p/o/q");
                continue;
        }

        if (stateAfterMove == -1) {
            continue;
        }

        if (stateAfterMove == 2) {
            if (try_advance_level(&game)) {
                victory = 1;
                break;
            }
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
