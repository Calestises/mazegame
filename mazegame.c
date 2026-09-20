#include <ctype.h>//字符处理
#include <stdio.h>
#include <stdlib.h>//随机数函数，生成迷宫用
#include <string.h>//提供字符串和内存处理函数
#include <time.h>//time()随机生成种子
#include <conio.h>
#include <windows.h>

#define ROWS 21  
#define COLS 31
#define maxvolumn (ROWS * COLS)
#define savefile "yoursave.txt"

typedef struct { //坐标结构体
    int row;
    int col;
} Point;

typedef struct { //状态结构体
    int rows;
    int cols;
    int playerRow;
    int playerCol;
    int startRow;
    int startCol;
    int doorRow;
    int doorCol;
    int keyCount;
    int totalkeys;
    int win;
    int teleportCount;
    char map[ROWS][COLS + 1]; //字符串多一行
    Point teleports[4];
} Game;

static HANDLE handle = NULL; //句柄初始未获得

static void hidecursor(void) { //光标隐藏函数
    CONSOLE_CURSOR_INFO cursorInfo;
    if (handle == NULL) {
        return;
    }
    if (GetConsoleCursorInfo(handle, &cursorInfo)) {
        cursorInfo.bVisible = FALSE; //将光标改为不可见然后提交
        SetConsoleCursorInfo(handle, &cursorInfo);
    }
}

static void showcursor(void) { //光标显示函数
    CONSOLE_CURSOR_INFO cursorInfo;
    if (handle == NULL) {
        return;
    }
    if (GetConsoleCursorInfo(handle, &cursorInfo)) {
        cursorInfo.bVisible = TRUE; //将光标改为可见然后提交
        SetConsoleCursorInfo(handle, &cursorInfo);
    }
}

static void setupconsole(void) { //初始化控制台环境
    handle = GetStdHandle(STD_OUTPUT_HANDLE); //设置句柄
    SetConsoleOutputCP(CP_UTF8); //输出编码为UTF-8
    SetConsoleCP(CP_UTF8);
    hidecursor();
}

static void clearscreen(void) { //清屏
    system("cls");
}

static void pauseandexit(void) { //提示用户按键并退出
    printf("\n按任意键退出...");
    (void)_getch();
}

static void getsavefilepath(char *buffer, size_t size) {  //寻找存档保存路径 需要获得一个字符数组地址和和大小
    DWORD length = GetModuleFileNameA(NULL, buffer, (DWORD)size); //获取路径

    if (length > 0 && length < size) { //找到路径且不超过缓冲
        char *lastSlash = strrchr(buffer, '\\');
        char *lastForwardSlash = strrchr(buffer, '/');
        char *separator = lastSlash;

        if (lastForwardSlash != NULL && (separator == NULL || lastForwardSlash > separator)) { 
            separator = lastForwardSlash;
        }

        if (separator != NULL) { //找到分隔符
            separator[1] = '\0';
            snprintf(buffer + strlen(buffer), size - strlen(buffer), "%s", savefile);
            return;
        }
    }
    snprintf(buffer, size, "%s", savefile);
}

static void seedrandom(void) { //初始化一次随机种子
    static int seeded = 0;
    if (seeded) {
        return;
    }
    seeded = 1;
    srand((unsigned int)time(NULL));
}

static void fill_walls(Game *game) { //填充墙，需要获得游戏状态和游戏对象的地址
    int r; //行
    int c; //列
    game->rows = ROWS; //设置宏
    game->cols = COLS;
    for (r = 0; r < game->rows; r++) { //遍历每行每列，填墙
        for (c = 0; c < game->cols; c++) {
            game->map[r][c] = '#';
        }
        game->map[r][game->cols] = '\0';
    }
}

static void changedirect(int dirs[4]) { //随机选择迷宫挖路方向，需要获得4个整型数组，即方向
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1); //随机挑选一个，然后交换位置
        int temp = dirs[i];
        dirs[i] = dirs[j];
        dirs[j] = temp;
    }
}

static void makemaze(Game *game) { //开始挖掘迷宫路径，需要获得游戏状态和游戏对象的地址
    int stackRow[maxvolumn];
    int stackCol[maxvolumn];
    int top = 0;
    int dr[4] = {-1, 1, 0, 0};
    int dc[4] = {0, 0, -1, 1};

    stackRow[0] = 1;
    stackCol[0] = 1;
    game->map[1][1] = '.';

    while (top >= 0) { //栈里还有点就可以继续挖
        int nowRow = stackRow[top];
        int nowCol = stackCol[top];
        int dirs[4] = {0, 1, 2, 3};
        int moved = 0;
        int i;

        changedirect(dirs);

        for (i = 0; i < 4; i++) {
            int dir = dirs[i];
            int nextRow = nowRow + dr[dir] * 2;
            int nextCol = nowCol + dc[dir] * 2;

            if (nextRow <= 0 || nextRow >= game->rows - 1) { //挖到边界了
                continue;
            }
            if (nextCol <= 0 || nextCol >= game->cols - 1) {
                continue;
            }
            if (game->map[nextRow][nextCol] != '#') {
                continue;
            }

            game->map[nowRow + dr[dir]][nowCol + dc[dir]] = '.'; //打通当前点，改为.符号，表示路
            game->map[nextRow][nextCol] = '.';
            top++;
            stackRow[top] = nextRow; //新点行的坐标放进栈顶
            stackCol[top] = nextCol; //新点列的坐标放进栈顶
            moved = 1;
            break;
        }

        if (!moved) { //当前点走到头了
            top-=1;
        }
    }
}

static void bfsdisrance(Game *game, int startRow, int startCol, int dist[ROWS][COLS]) { //负责算从起点到全图各处的最短距离表（二维数组），需要获取当前游戏数据，bfs的起点坐标和保存结果的距离数组
    Point queue[maxvolumn];
    int head = 0;
    int tail = 0;
    int dr[4] = {-1, 1, 0, 0};
    int dc[4] = {0, 0, -1, 1};
    int r;
    int c;

    for (r = 0; r < game->rows; r++) {
        for (c = 0; c < game->cols; c++) {
            dist[r][c] = -1;
        }
    }

    dist[startRow][startCol] = 0;
    queue[tail].row = startRow;
    queue[tail].col = startCol;
    tail++;

    while (head < tail) {
        Point now = queue[head];
        int i;
        head++;

        for (i = 0; i < 4; i++) { //循环寻找哪些格子值得搜索
            int nextRow = now.row + dr[i];
            int nextCol = now.col + dc[i];

            if (nextRow < 0 || nextRow >= game->rows) {
                continue;
            }
            if (nextCol < 0 || nextCol >= game->cols) {
                continue;
            }
            if (game->map[nextRow][nextCol] == '#') {
                continue;
            }
            if (dist[nextRow][nextCol] != -1) {
                continue;
            }

            dist[nextRow][nextCol] = dist[now.row][now.col] + 1;
            queue[tail].row = nextRow;
            queue[tail].col = nextCol;
            tail++;
        }
    }
}

static void choosedoor(Game *game) { //把门放到离起点最远的位置，获取游戏当前的状态数据
    int dist[ROWS][COLS];
    int best = -1;
    int r;
    int c;

    bfsdisrance(game, game->startRow, game->startCol, dist);

    for (r = 1; r < game->rows - 1; r++) { //逐行列搜索最远距离
        for (c = 1; c < game->cols - 1; c++) {
            if (game->map[r][c] == '#') {
                continue;
            }
            if (dist[r][c] > best) {
                best = dist[r][c];
                game->doorRow = r;
                game->doorCol = c;
            }
        }
    }

    game->map[game->doorRow][game->doorCol] = 'D';
}

static int countopenaround(Game *game, int row, int col) { //计算格子旁边有几条路能通，用于后面放置钥匙位置，需要获取游戏状态和格子的行列位置
    int dr[4] = {-1, 1, 0, 0};
    int dc[4] = {0, 0, -1, 1};
    int count = 0;
    int i;

    for (i = 0; i < 4; i++) { //统计四周多少地方不是墙
        int nextRow = row + dr[i];
        int nextCol = col + dc[i];
        if (nextRow < 0 || nextRow >= game->rows) {
            continue;
        }
        if (nextCol < 0 || nextCol >= game->cols) {
            continue;
        }
        if (game->map[nextRow][nextCol] != '#') {
            count++;
        }
    }
    return count;
}

static int findplace(Game *game, int dist[ROWS][COLS], int minDist, int needDeadEnd, Point *result) { //找一个满足要求的空地坐标，可能是钥匙或者传送点之类的，需要获取游戏状态，距离表，最小起点要求，条件开关，然后把找到的位置给到最后一个变量
    Point list[maxvolumn];
    int count = 0;
    int r;
    int c;
    int limit;

    for (limit = minDist; limit >= 1; limit--) {
        count = 0;
        for (r = 1; r < game->rows - 1; r++) {
            for (c = 1; c < game->cols - 1; c++) {
                if (game->map[r][c] != '.') {
                    continue;
                }
                if (r == game->startRow && c == game->startCol) {
                    continue;
                }
                if (dist[r][c] < limit) {
                    continue;
                }
                if (needDeadEnd && countopenaround(game, r, c) != 1) {
                    continue;
                }
                list[count].row = r;
                list[count].col = c;
                count++;
            }
        }
        if (count > 0) {
            int pick = rand() % count;
            *result = list[pick];
            return 1;
        }
    }
    return 0;
}

static void additems(Game *game) { //根据迷宫结构来摆放钥匙和传送点，获取当前游戏状态
    int dist[ROWS][COLS];
    Point p;
    int i;

    bfsdisrance(game, game->startRow, game->startCol, dist);

    game->totalkeys = 2;
    game->teleportCount = 0;

    for (i = 0; i < game->totalkeys; i++) { //放钥匙
        if (findplace(game, dist, 6, 1, &p)) {
            game->map[p.row][p.col] = 'K';
        }
    }

    for (i = 0; i < 2; i++) {
        if (findplace(game, dist, 4, 0, &p)) {  //放传送点
            game->map[p.row][p.col] = 'O';
            game->teleports[game->teleportCount] = p;
            game->teleportCount++;
        }
    }
}

static void setnewgame(Game *game) { //在空地图新建一个游戏，获取当前游戏状态
    memset(game, 0, sizeof(*game));
    fill_walls(game);
    makemaze(game);

    game->startRow = 1;
    game->startCol = 1;
    game->playerRow = 1;
    game->playerCol = 1;
    game->map[1][1] = '.';

    choosedoor(game);
    additems(game);
}

static void printmap(char tile) { //将该地图打印出来，转换字符为图案
    switch (tile) {
        case '#':
            printf("█");
            break;
        case '.':
            printf(" ");
            break;
        default:
            printf("%c", tile);
            break;
    }
}

static void drawgame(Game *game) { //游戏画面输出函数，需要获取当前游戏状态
    int r;
    int c;
    clearscreen();
    printf("钥匙:%d/%d\n", game->keyCount, game->totalkeys);
    printf("操作: 移动:w/a/s/d, 保存:p,读档:o,退出:q\n");
    printf("图例: █墙 @玩家 D门 K钥匙 O传送\n");
    printf("\n");

    for (r = 0; r < game->rows; r++) {
        for (c = 0; c < game->cols; c++) {
            if (r == game->playerRow && c == game->playerCol) {
                printf("@");
            } else {
                printmap(game->map[r][c]);
            }
        }
        printf("\n");
    }
}

static int getteleporttarget(Game *game, int row, int col, int *targetRow, int *targetCol) { //查找传送点位置，获取当前游戏状态，玩家坐标，传送点坐标
    int i;
    if (game->teleportCount < 2) {
        return 0;
    }
    for (i = 0; i < game->teleportCount; i++) {
        if (game->teleports[i].row == row && game->teleports[i].col == col) {
            int next = (i + 1) % game->teleportCount;
            *targetRow = game->teleports[next].row;
            *targetCol = game->teleports[next].col;
            return 1;
        }
    }
    return 0;
}

static int savegame(Game *game) { //写存档文件，把当前存档的所有游戏状态全写进去，所以需要获取当前游戏状态
    char savePath[MAX_PATH];
    FILE *fp;
    int i;
    int r;

    getsavefilepath(savePath, sizeof(savePath));
    fp = fopen(savePath, "w");

    if (fp == NULL) {
        return 0;
    }

    fprintf(fp, "%d %d %d %d %d %d %d %d\n",
        game->rows,
        game->cols,
        game->playerRow,
        game->playerCol,
        game->doorRow,
        game->doorCol,
        game->keyCount,
        game->totalkeys);

    fprintf(fp, "%d %d %d\n", game->startRow, game->startCol, game->teleportCount);

    for (r = 0; r < game->rows; r++) {
        fprintf(fp, "%s\n", game->map[r]);
    }

    for (i = 0; i < game->teleportCount; i++) {
        fprintf(fp, "%d %d\n", game->teleports[i].row, game->teleports[i].col);
    }

    fclose(fp);
    return 1;
}

static int loadgame(Game *game) { //将存档文件读出来并同步到当前游戏状态，需要获取当前游戏状态
    char savePath[MAX_PATH];
    FILE *fp;
    int r;
    int i;

    getsavefilepath(savePath, sizeof(savePath));
    fp = fopen(savePath, "r");

    if (fp == NULL) {
        return 0;
    }

    memset(game, 0, sizeof(*game));

    if (fscanf(fp, "%d %d %d %d %d %d %d %d",
        &game->rows,
        &game->cols,
        &game->playerRow,
        &game->playerCol,
        &game->doorRow,
        &game->doorCol,
        &game->keyCount,
        &game->totalkeys) != 8) {
        fclose(fp);
        return 0;
    }

    if (game->rows <= 0 || game->rows > ROWS || //读档数据合法检查
        game->cols <= 0 || game->cols > COLS ||
        game->teleportCount < 0 || game->teleportCount > 4) {
        fclose(fp);
        return 0;
    }

    if (fscanf(fp, "%d %d %d", //读取起点位置和传送点数量
        &game->startRow,
        &game->startCol,
        &game->teleportCount) != 3) {
        fclose(fp);
        return 0;
    }

    for (r = 0; r < game->rows; r++) {
        if (fscanf(fp, "%s", game->map[r]) != 1) {
            fclose(fp);
            return 0;
        }
        game->map[r][game->cols] = '\0';
    }

    for (i = 0; i < game->teleportCount; i++) {
        if (fscanf(fp, "%d %d", &game->teleports[i].row, &game->teleports[i].col) != 2) {
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    game->win = 0;
    return 1;
}

static int moveplayer(Game *game, int dr, int dc) { //检测玩家的移动情况，比如遇到门，墙壁，传送点，然后判断逻辑，需要获取当前游戏数据，和行列变化的情况
    int nextRow = game->playerRow + dr;
    int nextCol = game->playerCol + dc;
    char tile;

    if (nextRow < 0 || nextRow >= game->rows || nextCol < 0 || nextCol >= game->cols) {
        return 0;
    }

    tile = game->map[nextRow][nextCol];

    if (tile == '#') {
        return 0;
    }

    if (tile == 'D' && game->keyCount < game->totalkeys) {
        return 0;
    }

    game->playerRow = nextRow;
    game->playerCol = nextCol;

    tile = game->map[game->playerRow][game->playerCol];

    if (tile == 'K') {
        game->keyCount++;
        game->map[game->playerRow][game->playerCol] = '.';
    } else if (tile == 'O') {
        int targetRow;
        int targetCol;
        if (getteleporttarget(game, game->playerRow, game->playerCol, &targetRow, &targetCol)) {
            game->playerRow = targetRow;
            game->playerCol = targetCol;
        }
    } else if (tile == 'D') {
        game->win = 1;
        return 2;
    }

    return 1;
}

static int readkey(void) { //读取用户的输入
    return _getch();
}

int main(void) {
    Game game;
    char input[32];
    int running = 1;
    int key;

    setupconsole();
    seedrandom();

    clearscreen();
    printf("神秘迷宫游戏\n");
    printf("1. 新游戏\n");
    printf("2. 读取存档\n");
    printf("请选择: ");

    if (fgets(input, sizeof(input), stdin) == NULL) { //读取失败就退出
        showcursor();
        return 0;
    }

    if (input[0] == '2') { //优先按用户选择读档，读档失败就开新档
        if (!loadgame(&game)) {
            setnewgame(&game);
        }
    } else {
        setnewgame(&game);
    }

    while (running) {
        drawgame(&game);

        if (game.win) { //判断是否胜利
            break;
        }

        key = readkey();
        switch (tolower((unsigned char)key)) { //按键反馈对应
            case 'w':
                moveplayer(&game, -1, 0);
                break;
            case 's':
                moveplayer(&game, 1, 0);
                break;
            case 'a':
                moveplayer(&game, 0, -1);
                break;
            case 'd':
                moveplayer(&game, 0, 1);
                break;
            case 'p':
                (void)savegame(&game);
                break;
            case 'o':
                (void)loadgame(&game);
                break;
            case 'q':
                running = 0;
                break;
            default:
                break;
        }
    }

    drawgame(&game);
    if (game.win) {
        printf("\nYou Win!\n");
        printf("你成功逃出了迷宫\n");
        pauseandexit();
    }

    showcursor();
    return 0;
}
