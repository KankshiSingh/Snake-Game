// gcc snake.c -o snake -lncurses
// ./snake -d 30x20

#include <stdlib.h>
#include <unistd.h>
#include <curses.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define MAX_SCORE 256
#define BASE_FRAME_TIME 280000  // base microseconds per frame

typedef struct {
    int x;
    int y;
} vec2;

int score = 0;
int high_score = 0;
char score_message[64];

bool is_running = true;   // false only when game over screen is active
bool paused = false;

int screen_width = 25;
int screen_height = 20;

// current frame time (gets faster as score increases)
int frame_time = BASE_FRAME_TIME;

// timer for berry
int berry_time_limit = 10;      // seconds allowed to reach each berry
time_t berry_start_time;        // when current berry spawned
bool time_up = false;           // whether last game over was due to timer

// ncurses window
WINDOW *win;

// snake
vec2 head = { 0, 0 };
vec2 segments[MAX_SCORE + 1];
vec2 dir = { 1, 0 }; // starts moving right

// berry
vec2 berry;

// ---------- Utility functions ----------

bool collide(vec2 a, vec2 b) {
    return (a.x == b.x && a.y == b.y);
}

bool collide_snake_body(vec2 point) {
    for (int i = 0; i < score; i++) {
        if (collide(point, segments[i])) {
            return true;
        }
    }
    return false;
}

vec2 spawn_berry() {
    // spawn a new berry with 1 cell padding from edges and not inside the snake
    vec2 b;
    do {
        b.x = 1 + rand() % (screen_width - 2);
        b.y = 1 + rand() % (screen_height - 2);
    } while (collide(head, b) || collide_snake_body(b));
    return b;
}

void draw_border(int y, int x, int width, int height) {
    // top row
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + width * 2 + 1, ACS_URCORNER);
    for (int i = 1; i < width * 2 + 1; i++) {
        mvaddch(y, x + i, ACS_HLINE);
    }
    // vertical lines
    for (int i = 1; i < height + 1; i++) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + width * 2 + 1, ACS_VLINE);
    }
    // bottom row
    mvaddch(y + height + 1, x, ACS_LLCORNER);
    mvaddch(y + height + 1, x + width * 2 + 1, ACS_LRCORNER);
    for (int i = 1; i < width * 2 + 1; i++) {
        mvaddch(y + height + 1, x + i, ACS_HLINE);
    }
}

void quit_game() {
    // exit cleanly from application
    endwin();
    // clear screen, place cursor on top, and un-hide cursor
    printf("\e[1;1H\e[2J");
    printf("\e[?25h");
    exit(0);
}

void update_speed() {
    // Increase speed as score increases (min cap)
    frame_time = BASE_FRAME_TIME - score * 8000; // faster per point
    if (frame_time < 60000) {
        frame_time = 60000; // don't go too fast
    }
}

void restart_game() {
    head.x = screen_width / 2;
    head.y = screen_height / 2;
    dir.x = 1;
    dir.y = 0;
    score = 0;
    paused = false;
    time_up = false;

    berry = spawn_berry();
    berry_start_time = time(NULL);    // reset timer for new berry

    update_speed();
    sprintf(score_message, "[ Score: %d | Best: %d ]", score, high_score);
    is_running = true;
}

void init() {
    srand(time(NULL));

    // initialize window
    win = initscr();
    keypad(win, true);  b
    noecho();
    nodelay(win, true);
    curs_set(0);

    // initialize color
    if (has_colors() == FALSE) {
        endwin();
        fprintf(stderr, "Your terminal does not support color\n");
        exit(1);
    }
    start_color();
    use_default_colors();
    init_pair(1, COLOR_RED, -1);    // berry
    init_pair(2, COLOR_GREEN, -1);  // snake
    init_pair(3, COLOR_YELLOW, -1); // border/text

    // center the snake at start
    head.x = screen_width / 2;
    head.y = screen_height / 2;

    berry = spawn_berry();
    berry_start_time = time(NULL);   // start timer for first berry
    time_up = false;

    update_speed();
    sprintf(score_message, "[ Score: %d | Best: %d ]", score, high_score);
}

// ---------- Input ----------

void process_input() {
    int pressed = wgetch(win);

    if (pressed == KEY_LEFT) {
        if (dir.x == 1) return; // prevent reverse direction
        dir.x = -1;
        dir.y = 0;
    }
    else if (pressed == KEY_RIGHT) {
        if (dir.x == -1) return;
        dir.x = 1;
        dir.y = 0;
    }
    else if (pressed == KEY_UP) {
        if (dir.y == 1) return;
        dir.x = 0;
        dir.y = -1;
    }
    else if (pressed == KEY_DOWN) {
        if (dir.y == -1) return;
        dir.x = 0;
        dir.y = 1;
    }
    else if (pressed == ' ' ) {
        // during game over this will restart
        if (!is_running) {
            restart_game();
        }
    }
    else if (pressed == 'p' || pressed == 'P') {
        // pause/resume only if not in game-over state
        if (is_running) {
            paused = !paused;
        }
    }
    else if (pressed == '\e') { // ESC
        quit_game();
    }
}

// ---------- Game Over Screen ----------

void game_over_screen() {
    while (is_running == false) {
        process_input();

        attron(COLOR_PAIR(3));
        int msg_y = screen_height / 2;
        int msg_x = screen_width - 18;

        if (time_up) {
            mvaddstr(msg_y,     msg_x, "     TIME UP! GAME OVER   ");
        } else {
            mvaddstr(msg_y,     msg_x, "        GAME OVER         ");
        }
        mvaddstr(msg_y + 1, msg_x, "   [SPACE] Restart        ");
        mvaddstr(msg_y + 2, msg_x, "    [ESC]   Quit          ");

        draw_border(msg_y - 1, msg_x - 1, 19, 3);
        attroff(COLOR_PAIR(3));

        usleep(frame_time);
        refresh();
    }
}

// ---------- Update & Draw ----------

void update() {
    // TIMER CHECK (before moving/eating)
    int elapsed = (int)(time(NULL) - berry_start_time);
    if (elapsed >= berry_time_limit) {
        // time is up → game over
        time_up = true;

        if (score > high_score) {
            high_score = score;
        }
        sprintf(score_message, "[ Score: %d | Best: %d ]", score, high_score);

        is_running = false;
        game_over_screen();
        return;
    }

    // update snake segments
    for (int i = score; i > 0; i--) {
        segments[i] = segments[i - 1];
    }
    segments[0] = head;

    // move snake head
    head.x += dir.x;
    head.y += dir.y;

    // collide with body or walls
    if (collide_snake_body(head) ||
        head.x < 0 || head.y < 0 ||
        head.x >= screen_width || head.y >= screen_height) {

        if (score > high_score) {
            high_score = score;
        }
        time_up = false; // this game over is due to collision

        sprintf(score_message, "[ Score: %d | Best: %d ]", score, high_score);

        is_running = false;
        game_over_screen();
        return;
    }

    // eating a berry
    if (collide(head, berry)) {
        if (score < MAX_SCORE) {
            score += 1;
            update_speed();
            sprintf(score_message, "[ Score: %d | Best: %d ]", score, high_score);
        }
        // spawn new berry and reset timer
        berry = spawn_berry();
        berry_start_time = time(NULL);
        time_up = false;
    }

    usleep(frame_time);
}

void draw() {
    erase();

    // draw berry
    attron(COLOR_PAIR(1));
    mvaddch(berry.y + 1, berry.x * 2 + 1, '@');
    attroff(COLOR_PAIR(1));

    // draw snake
    attron(COLOR_PAIR(2));
    for (int i = 0; i < score; i++) {
        mvaddch(segments[i].y + 1, segments[i].x * 2 + 1, ACS_DIAMOND);
    }
    mvaddch(head.y + 1, head.x * 2 + 1, 'O');
    attroff(COLOR_PAIR(2));

    // draw border
    attron(COLOR_PAIR(3));
    draw_border(0, 0, screen_width, screen_height);
    attroff(COLOR_PAIR(3));

    // score & status
    attron(COLOR_PAIR(3));
    mvaddstr(0, 2, score_message);

    // timer display
    int time_left = berry_time_limit - (int)(time(NULL) - berry_start_time);
    if (time_left < 0) time_left = 0;
    mvaddstr(1, 2, "Time Left: ");
    mvprintw(1, 14, "%d sec", time_left);

    mvaddstr(screen_height + 2, 2,
             "Arrows: Move  P: Pause/Resume  SPACE: Restart (on Game Over)  ESC: Quit");
    if (paused) {
        mvaddstr(2, 2, "[ PAUSED ]");
    }
    attroff(COLOR_PAIR(3));

    refresh();
}

// ---------- main ----------

int main(int argc, char *argv[]) {
    // process user args
    if (argc == 1) {
        // use defaults
    }
    else if (argc == 3) {
        if (!strcmp(argv[1], "-d")) {
            if (sscanf(argv[2], "%dx%d", &screen_width, &screen_height) != 2) {
                printf("Usage: snake [options]\nOptions:\n -d [width]x[height]"
                       "\tdefine dimensions of the screen\n\nDefault dimensions are 25x20\n");
                exit(1);
            }
        }
    }
    else {
        printf("Usage: snake [options]\nOptions:\n -d [width]x[height]"
               "\tdefine dimensions of the screen\n\nDefault dimensions are 25x20\n");
        exit(1);
    }

    init();

    while (1) {
        process_input();

        if (!paused && is_running) {
            update();
        }

        draw();
    }

    quit_game();
    return 0;
}

