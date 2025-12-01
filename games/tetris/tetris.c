#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define DROP_INTERVAL_MS 500

static struct termios original_termios;
static bool terminal_configured = false;

static void restore_terminal(void)
{
    if (terminal_configured)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        terminal_configured = false;
    }
}

static void handle_exit(int signum)
{
    (void) signum;
    restore_terminal();
    printf("\nThanks for playing!\n");
    exit(EXIT_SUCCESS);
}

static void set_terminal_raw(void)
{
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
    {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    struct termios raw = original_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
    terminal_configured = true;
    atexit(restore_terminal);
    signal(SIGINT, handle_exit);
    signal(SIGTERM, handle_exit);
}

typedef struct
{
    int rotation_count;
    const int (*rotations)[4][4];
} Shape;

typedef struct
{
    int x;
    int y;
    int rotation;
    int shape_index;
} Piece;

static const int SHAPE_I[4][4][4] = {
        {{0, 0, 0, 0},
         {1, 1, 1, 1},
         {0, 0, 0, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 1, 0, 0}},

        {{0, 0, 0, 0},
         {1, 1, 1, 1},
         {0, 0, 0, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 1, 0, 0}}};

static const int SHAPE_O[1][4][4] = {
        {{0, 0, 0, 0},
         {0, 1, 1, 0},
         {0, 1, 1, 0},
         {0, 0, 0, 0}}};

static const int SHAPE_T[4][4][4] = {
        {{0, 0, 0, 0},
         {1, 1, 1, 0},
         {0, 1, 0, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {1, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {1, 1, 1, 0},
         {0, 0, 0, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {0, 1, 1, 0},
         {0, 1, 0, 0},
         {0, 0, 0, 0}}};

static const int SHAPE_S[2][4][4] = {
        {{0, 0, 0, 0},
         {0, 1, 1, 0},
         {1, 1, 0, 0},
         {0, 0, 0, 0}},

        {{1, 0, 0, 0},
         {1, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 0, 0, 0}}};

static const int SHAPE_Z[2][4][4] = {
        {{0, 0, 0, 0},
         {1, 1, 0, 0},
         {0, 1, 1, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {1, 1, 0, 0},
         {1, 0, 0, 0},
         {0, 0, 0, 0}}};

static const int SHAPE_L[4][4][4] = {
        {{0, 0, 0, 0},
         {1, 1, 1, 0},
         {1, 0, 0, 0},
         {0, 0, 0, 0}},

        {{1, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 0, 0, 0}},

        {{0, 0, 1, 0},
         {1, 1, 1, 0},
         {0, 0, 0, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 1, 1, 0},
         {0, 0, 0, 0}}};

static const int SHAPE_J[4][4][4] = {
        {{0, 0, 0, 0},
         {1, 1, 1, 0},
         {0, 0, 1, 0},
         {0, 0, 0, 0}},

        {{0, 1, 0, 0},
         {0, 1, 0, 0},
         {1, 1, 0, 0},
         {0, 0, 0, 0}},

        {{1, 0, 0, 0},
         {1, 1, 1, 0},
         {0, 0, 0, 0},
         {0, 0, 0, 0}},

        {{0, 1, 1, 0},
         {0, 1, 0, 0},
         {0, 1, 0, 0},
         {0, 0, 0, 0}}};

static const Shape SHAPES[] = {
        {4, SHAPE_I},
        {1, SHAPE_O},
        {4, SHAPE_T},
        {2, SHAPE_S},
        {2, SHAPE_Z},
        {4, SHAPE_L},
        {4, SHAPE_J},
};

static int board[BOARD_HEIGHT][BOARD_WIDTH];

static void clear_board(void)
{
    for (int y = 0; y < BOARD_HEIGHT; ++y)
    {
        for (int x = 0; x < BOARD_WIDTH; ++x)
        {
            board[y][x] = 0;
        }
    }
}

static Piece spawn_piece(void)
{
    int shape_index = rand() % (int) (sizeof(SHAPES) / sizeof(SHAPES[0]));
    Piece piece = {
            .x = (BOARD_WIDTH - 4) / 2,
            .y = 0,
            .rotation = 0,
            .shape_index = shape_index,
    };
    return piece;
}

static bool check_collision(const Piece *piece, int offset_x, int offset_y, int rotation_offset)
{
    const Shape *shape = &SHAPES[piece->shape_index];
    int rotation = (piece->rotation + rotation_offset + shape->rotation_count) % shape->rotation_count;
    const int (*cells)[4] = shape->rotations[rotation];

    for (int dy = 0; dy < 4; ++dy)
    {
        for (int dx = 0; dx < 4; ++dx)
        {
            if (!cells[dy][dx])
            {
                continue;
            }

            int new_x = piece->x + dx + offset_x;
            int new_y = piece->y + dy + offset_y;

            if (new_x < 0 || new_x >= BOARD_WIDTH || new_y >= BOARD_HEIGHT)
            {
                return true;
            }
            if (new_y >= 0 && board[new_y][new_x])
            {
                return true;
            }
        }
    }
    return false;
}

static void merge_piece(const Piece *piece)
{
    const Shape *shape = &SHAPES[piece->shape_index];
    const int (*cells)[4] = shape->rotations[piece->rotation];

    for (int dy = 0; dy < 4; ++dy)
    {
        for (int dx = 0; dx < 4; ++dx)
        {
            if (cells[dy][dx])
            {
                int x = piece->x + dx;
                int y = piece->y + dy;
                if (y >= 0 && y < BOARD_HEIGHT && x >= 0 && x < BOARD_WIDTH)
                {
                    board[y][x] = piece->shape_index + 1;
                }
            }
        }
    }
}

static int clear_lines(void)
{
    int cleared = 0;
    for (int y = BOARD_HEIGHT - 1; y >= 0; --y)
    {
        bool full = true;
        for (int x = 0; x < BOARD_WIDTH; ++x)
        {
            if (!board[y][x])
            {
                full = false;
                break;
            }
        }

        if (full)
        {
            ++cleared;
            for (int row = y; row > 0; --row)
            {
                for (int x = 0; x < BOARD_WIDTH; ++x)
                {
                    board[row][x] = board[row - 1][x];
                }
            }
            for (int x = 0; x < BOARD_WIDTH; ++x)
            {
                board[0][x] = 0;
            }
            ++y; // re-check current row after collapsing
        }
    }
    return cleared;
}

static void draw_board(const Piece *current, int score)
{
    printf("\033[H\033[J");
    printf("==== Terminal Tetris ====\n");
    printf("Controls: a=left d=right s=down w=rotate space=drop q=quit\n");
    printf("Score: %d\n\n", score);

    for (int y = 0; y < BOARD_HEIGHT; ++y)
    {
        printf("|");
        for (int x = 0; x < BOARD_WIDTH; ++x)
        {
            bool filled = board[y][x] != 0;

            if (current)
            {
                const Shape *shape = &SHAPES[current->shape_index];
                const int (*cells)[4] = shape->rotations[current->rotation];
                int local_y = y - current->y;
                int local_x = x - current->x;
                if (local_x >= 0 && local_x < 4 && local_y >= 0 && local_y < 4)
                {
                    filled = filled || cells[local_y][local_x] != 0;
                }
            }

            printf(filled ? "#" : " ");
        }
        printf("|\n");
    }

    for (int i = 0; i < BOARD_WIDTH + 2; ++i)
    {
        printf("-");
    }
    printf("\n");
    fflush(stdout);
}

static bool read_input(char *out_char)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout = {0, 0};
    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0)
    {
        ssize_t bytes = read(STDIN_FILENO, out_char, 1);
        return bytes == 1;
    }
    return false;
}

static void sleep_millis(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void hard_drop(Piece *piece)
{
    while (!check_collision(piece, 0, 1, 0))
    {
        piece->y += 1;
    }
}

static int game_loop(void)
{
    int score = 0;
    Piece current = spawn_piece();

    struct timespec last_drop;
    clock_gettime(CLOCK_MONOTONIC, &last_drop);

    while (true)
    {
        draw_board(&current, score);

        char input;
        if (read_input(&input))
        {
            if (input == 'q')
            {
                break;
            }
            else if (input == 'a' && !check_collision(&current, -1, 0, 0))
            {
                current.x -= 1;
            }
            else if (input == 'd' && !check_collision(&current, 1, 0, 0))
            {
                current.x += 1;
            }
            else if (input == 's' && !check_collision(&current, 0, 1, 0))
            {
                current.y += 1;
            }
            else if (input == 'w' && !check_collision(&current, 0, 0, 1))
            {
                current.rotation = (current.rotation + 1) % SHAPES[current.shape_index].rotation_count;
            }
            else if (input == ' ')
            {
                hard_drop(&current);
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - last_drop.tv_sec) * 1000L + (now.tv_nsec - last_drop.tv_nsec) / 1000000L;

        if (elapsed_ms >= DROP_INTERVAL_MS)
        {
            last_drop = now;
            if (!check_collision(&current, 0, 1, 0))
            {
                current.y += 1;
            }
            else
            {
                merge_piece(&current);
                int cleared = clear_lines();
                score += cleared * 100;
                current = spawn_piece();

                if (check_collision(&current, 0, 0, 0))
                {
                    draw_board(NULL, score);
                    printf("Game over! Final score: %d\n", score);
                    return score;
                }
            }
        }

        sleep_millis(16); // ~60 FPS refresh
    }

    return score;
}

int main(void)
{
    srand((unsigned int) time(NULL));
    clear_board();
    set_terminal_raw();
    game_loop();
    restore_terminal();
    return 0;
}

