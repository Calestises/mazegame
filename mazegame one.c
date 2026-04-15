#include <stdio.h>
#include <stdlib.h>

#define ROWS 7
#define COLS 12

void clear_screen(void) {
    system("cls");
}

void draw_map(char map[ROWS][COLS + 1], int playerRow, int playerCol) {
    clear_screen();
    printf("Control: w(up) a(left) s(down) d(right) q(quit)\n");
    printf("Goal: Reach D\n\n");

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (i == playerRow && j == playerCol) {
                putchar('P');
            } else {
                putchar(map[i][j]);
            }
        }
        putchar('\n');
    }
}

int main(void) {
    char map[ROWS][COLS + 1] = {
        "############",
        "#P.........D#",
        "#.##.###.#.#",
        "#....#.....#",
        "#.##.#.###.#",
        "#..........#",
        "############"
    };

    int playerRow = 1;
    int playerCol = 1;
    char input;

    while (1) {
        draw_map(map, playerRow, playerCol);
        printf("\nEnter command: ");

        scanf(" %c", &input);

        int nextRow = playerRow;
        int nextCol = playerCol;

        switch (input) {
            case 'w':
                nextRow--;
                break;
            case 's':
                nextRow++;
                break;
            case 'a':
                nextCol--;
                break;
            case 'd':
                nextCol++;
                break;
            case 'q':
                printf("\nQuit game.\n");
                return 0;
            default:
                printf("Invalid input, use w/a/s/d/q\n");
                continue;
        }

        if (map[nextRow][nextCol] == '#') {
            printf("Wall ahead!\n");
            continue;
        }

        playerRow = nextRow;
        playerCol = nextCol;

        if (map[playerRow][playerCol] == 'D') {
            draw_map(map, playerRow, playerCol);
            printf("\nCongratulations! You reached the destination!\n");
            break;
        }
    }

    return 0;
}
