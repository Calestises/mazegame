#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define ROWS 13
#define COLS 23
#define MAX_TELEPORTS 4

typedef struct {
    int row;
    int col;
} Point;

typedef struct {
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
    char map[ROWS][COLS + 1];
    Point teleports[MAX_TELEPORTS];
    char message[128];
} GameState;

static const char *LEVEL_TEMPLATE[ROWS] = {
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
    "#..#K.......O........##",
    "#.....................#",
    "#######################"
};

void setup_console_encoding(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    setlocale(LC_ALL, ".UTF-8");
}

void clear_screen(void) {
    int result = system("cls");
    if (result != 0) {
        for (int i = 0; i < 25; i++) {
            putchar('\n');
        }
    }
}

void set_message(GameState *game, const char *text) {
    snprintf(game->message, sizeof(game->message), "%s", text);
}

const char *display_symbol(char tile) {
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
        default:
            fallback[0] = tile;
            fallback[1] = '\0';
            return fallback;
    }
}

void add_teleport(GameState *game, int row, int col) {
    if (game->teleportCount >= MAX_TELEPORTS) {
        return;
    }

    game->teleports[game->teleportCount].row = row;
    game->teleports[game->teleportCount].col = col;
    game->teleportCount++;
}

void load_level(GameState *game) {
    memset(game, 0, sizeof(*game));
    game->hp = 3;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char tile = LEVEL_TEMPLATE[r][c];

            if (tile == 'P') {
                game->playerRow = r;
                game->playerCol = c;
                game->startRow = r;
                game->startCol = c;
                game->map[r][c] = '.';
            } else {
                game->map[r][c] = tile;
                if (tile == 'K') {
                    game->requiredKeys++;
                } else if (tile == 'O') {
                    add_teleport(game, r, c);
                }
            }
        }
        game->map[r][COLS] = '\0';
    }

    set_message(game, "第二天版本：先找钥匙，再去开门。");
}

void draw_game(const GameState *game) {
    clear_screen();
    printf("=== 迷宫游戏（第二天版本）===\n");
    printf("生命:%d  金币:%d  钥匙:%d/%d  步数:%d\n",
           game->hp,
           game->gold,
           game->keyCount,
           game->requiredKeys,
           game->steps);
    printf("操作: w/a/s/d 移动, q 退出\n");
    printf("图例: █墙 @玩家 D门 K钥匙 $金币 !陷阱 O传送\n");
    printf("消息: %s\n\n", game->message);

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (r == game->playerRow && c == game->playerCol) {
                fputs(display_symbol('P'), stdout);
            } else {
                fputs(display_symbol(game->map[r][c]), stdout);
            }
        }
        putchar('\n');
    }
}

int get_teleport_destination(const GameState *game, int row, int col, int *targetRow, int *targetCol) {
    if (game->teleportCount < 2) {
        return 0;
    }

    for (int i = 0; i < game->teleportCount; i++) {
        if (game->teleports[i].row == row && game->teleports[i].col == col) {
            int next = (i + 1) % game->teleportCount;
            *targetRow = game->teleports[next].row;
            *targetCol = game->teleports[next].col;
            return 1;
        }
    }

    return 0;
}

int handle_special_tile(GameState *game) {
    char tile = game->map[game->playerRow][game->playerCol];

    if (tile == 'K') {
        game->keyCount++;
        game->map[game->playerRow][game->playerCol] = '.';
        set_message(game, "你拿到了钥匙。");
    } else if (tile == 'C') {
        game->gold++;
        game->map[game->playerRow][game->playerCol] = '.';
        set_message(game, "你捡到了一枚金币。");
    } else if (tile == 'T') {
        game->hp--;
        game->playerRow = game->startRow;
        game->playerCol = game->startCol;
        if (game->hp <= 0) {
            set_message(game, "你踩中了陷阱，生命值归零。");
            return -1;
        }
        set_message(game, "你踩中了陷阱，生命值 -1，已回到起点。");
    } else if (tile == 'O') {
        int targetRow;
        int targetCol;

        if (get_teleport_destination(game, game->playerRow, game->playerCol, &targetRow, &targetCol)) {
            game->playerRow = targetRow;
            game->playerCol = targetCol;
            set_message(game, "你被传送到了另一处。");
        }
    } else if (tile == 'D') {
        if (game->keyCount >= game->requiredKeys) {
            return 1;
        }
        set_message(game, "你还没有拿到全部钥匙，无法开门。");
    } else {
        set_message(game, "移动成功。");
    }

    return 0;
}

int process_move(GameState *game, int dRow, int dCol) {
    int nextRow = game->playerRow + dRow;
    int nextCol = game->playerCol + dCol;
    char tile;

    if (nextRow < 0 || nextRow >= ROWS || nextCol < 0 || nextCol >= COLS) {
        set_message(game, "不能走出地图。");
        return 0;
    }

    tile = game->map[nextRow][nextCol];

    if (tile == '#') {
        set_message(game, "前面是墙。");
        return 0;
    }

    if (tile == 'D' && game->keyCount < game->requiredKeys) {
        set_message(game, "门还打不开，先把钥匙找齐。");
        return 0;
    }

    game->playerRow = nextRow;
    game->playerCol = nextCol;
    game->steps++;

    return handle_special_tile(game);
}

int main(void) {
    GameState game;
    char input[32];
    int result;

    setup_console_encoding();
    load_level(&game);

    while (1) {
        draw_game(&game);

        if (game.hp <= 0) {
            printf("\n游戏结束。\n");
            break;
        }

        printf("\n请输入操作：");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        switch (tolower((unsigned char)input[0])) {
            case 'w':
                result = process_move(&game, -1, 0);
                break;
            case 's':
                result = process_move(&game, 1, 0);
                break;
            case 'a':
                result = process_move(&game, 0, -1);
                break;
            case 'd':
                result = process_move(&game, 0, 1);
                break;
            case 'q':
                printf("\n你已退出游戏。\n");
                return 0;
            default:
                set_message(&game, "无效操作，请输入 w/a/s/d/q。");
                continue;
        }

        if (result == 1) {
            draw_game(&game);
            printf("\n你通关了第二天版本。\n");
            break;
        }
    }

    return 0;
}
