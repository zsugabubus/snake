#define _GNU_SOURCE

#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static char const USAGE[] =
"Usage: snake [OPTION]\n"
"\n"
"  -s SPEED      set snake speed (default: 6)\n"
"  -t THEME      set display theme\n"
"  -m NAME       start playing on map\n"
"  -M            mouse mode\n"
"  -h            display this help and exit\n"
"\n"
"Pass 'help' as value to get full list of available\n"
"choices.\n"
"\n"
;

#define G(str) "\033[32m" str "\033[m"
#define B(str) "\033[1;32m" str "\033[m"

#define ARRAY_SIZE(x) (int)(sizeof x / sizeof *x)

enum {
	W = 21,
	H = 23,
};

enum direction {
	UP,
	RIGHT,
	DOWN,
	LEFT,
};

enum type {
	T_GROUND,
	T_HEAD,
	T_SNAKE = T_HEAD + 4,
	T_FAT_SNAKE = T_SNAKE + 4 * 4,
	T_WALL = T_FAT_SNAKE + 4 * 4,
	T_SNAKE_END = T_WALL,
	T_HOLE,
	T_MEAT,
	T_SNAIL,
	T_EGG,
	T_STAR,
	T_ANT,
	T_BEETLE,
	T_PRESENT,
	T_HIT,
	T_ALPHABET,
};

struct map {
	char name[W];
	void (*enter)(void);
};

static char const ASCII_ARTS[][20] = {
	[T_GROUND] = "  ",
	[T_HEAD] =
	B("V "), B("< "), B("^ "), B(" >"),
	[T_SNAKE] =
	G("  "), G("o-"), G("| "), G("o "),
	G("o-"), G("  "), G("o-"), G("--"),
	G("| "), G("o-"), G("  "), G("o "),
	G("o "), G("--"), G("o "), G("  "),
	[T_FAT_SNAKE] =
	B("  "), B("O="), B("| "), B("O "),
	B("O="), B("  "), B("O="), B("=="),
	B("| "), B("O="), B("  "), B("O "),
	B("O "), B("=="), B("O "), B("  "),
	[T_HOLE] = "\033[1;34m[]\033[m",
	[T_MEAT] = "++",
	[T_EGG] = "O ",
	[T_SNAIL] = "@/",
	[T_BEETLE] = "MM",
	[T_ANT] = "mm",
	[T_STAR] = "**",
	[T_PRESENT] = "??",
	[T_WALL] = "##",
	[T_HIT] = ":(",
	[T_ALPHABET] =
	"A ", "B ", "C ", "D ", "E ",
	"F ", "G ", "H ", "I ", "J ",
	"K ", "L ", "M ", "N ", "O ",
	"P ", "Q ", "R ", "S ", "T ",
	"U ", "V ", "W ", "X ", "Y ",
	"Z ",
};

static char const ASCII_BLOCK_ARTS[][20] = {
	[T_GROUND] = "  ",
	[T_HEAD] =
#define X "\033[1;7moo\033[m"
	X, X, X, X,
#undef X
#define X "\033[7m  \033[m"
	[T_SNAKE] =
	X, X, X, X,
	X, X, X, X,
	X, X, X, X,
	X, X, X, X,
#undef X
#define X "\033[7m++\033[m"
	[T_FAT_SNAKE] =
	X, X, X, X,
	X, X, X, X,
	X, X, X, X,
	X, X, X, X,
#undef X
	[T_HOLE] = "\033[1;7;34m[]\033[m",
	[T_MEAT] = "\033[1;7;31m++\033[m",
	[T_EGG] = "\033[7;31mO \033[m",
	[T_SNAIL] = "\033[7;31m@/\033[m",
	[T_BEETLE] = "\033[7;31mMM\033[m",
	[T_ANT] = "\033[7;31mmm\033[m",
	[T_STAR] = "\033[1;7;33m**\033[m",
	[T_PRESENT] = "\033[1;7;33m??\033[m",
	[T_WALL] = "\033[1;7m  \033[m",
	[T_HIT] = "\033[1;31m:(\033[m",
	[T_ALPHABET] =
	"A ", "B ", "C ", "D ", "E ",
	"F ", "G ", "H ", "I ", "J ",
	"K ", "L ", "M ", "N ", "O ",
	"P ", "Q ", "R ", "S ", "T ",
	"U ", "V ", "W ", "X ", "Y ",
	"Z ",
};

static char const UNICODE_ARTS[][20] = {
	[T_GROUND] = "  ",
	[T_HEAD] =
	B("ꙭ "), B("ꙭ "), B("ꙭ "), B("ꙭ "),
	[T_SNAKE] =
	G("  "), G("┗━"), G("┃ "), G("┛ "),
	G("┗━"), G("  "), G("┏━"), G("━━"),
	G("┃ "), G("┏━"), G("  "), G("┓ "),
	G("┛ "), G("━━"), G("┓ "), G("  "),
	[T_FAT_SNAKE] =
	B("  "), B("╚═"), B("║ "), B("╝ "),
	B("╚═"), B("  "), B("╔═"), B("══"),
	B("║ "), B("╔═"), B("  "), B("╗ "),
	B("╝ "), B("══"), B("╗ "), B("  "),
	[T_HOLE] = "\033[1;34m🞑 \033[m",
	[T_MEAT] = "🍗",
	[T_EGG] = "🥚",
	[T_SNAIL] = "🐌",
	[T_BEETLE] = "🐞",
	[T_ANT] = "🐜",
	[T_STAR] = "🌟",
	[T_PRESENT] = "🎁",
	[T_WALL] = "🧱",
	[T_HIT] = "💥",
	[T_ALPHABET] =
	"Ａ", "Ｂ", "Ｃ", "Ｄ", "Ｅ",
	"Ｆ", "Ｇ", "Ｈ", "Ｉ", "Ｊ",
	"Ｋ", "Ｌ", "Ｍ", "Ｎ", "Ｏ",
	"Ｐ", "Ｑ", "Ｒ", "Ｓ", "Ｔ",
	"Ｕ", "Ｖ", "Ｗ", "Ｘ", "Ｙ",
	"Ｚ",
};

static char const (*ARTS)[20] = UNICODE_ARTS;

static int const SPEED_DELAYS[] = {
	800,
	500,
	300,
	200,
	145,
	125,
	100,
	80,
	50,
};

static char jungle[H * W];
static enum direction snake_dir, next_snake_dir;
static int snake_growth;
static int score;
static int speed = 7;
static int ytail, xtail;
static int yhead, xhead;
static int yfood, xfood;
static int food_timeout;
static enum direction food_dir;
static int mushroom_bonus;
static int star_bonus;
static int paused;
static int mouse;
static struct termios saved_termios;

static int partially_damaged;
static int jungle_damage[20];
static int num_damages;
static int old_score, old_timeout;

static enum direction
opposite(enum direction d)
{
	return d ^ 2;
}

static enum direction
turn_left(enum direction d)
{
	return (d - 1) % 4;
}

static enum direction
turn_right(enum direction d)
{
	return (d + 1) % 4;
}

static void
draw_cell(int pos)
{
	fputs(ARTS[(unsigned)jungle[pos]], stdout);
}

static void
vacuum_jungle(void)
{
	memset(jungle, T_GROUND, sizeof jungle);
	snake_growth = 1;
	mushroom_bonus = 0;
	star_bonus = 0;
	yfood = -1;
	food_timeout = 0;
	partially_damaged = 0;
}

static void
draw_jungle(void)
{
	if (partially_damaged) {
		for (int i = 0; i < num_damages; ++i) {
			int at = jungle_damage[i];
			fprintf(stdout, "\033[%d;%dH", 1 + at / W, 1 + (at % W) * 2);
			draw_cell(at);
		}
	} else {
		fputs("\033[H\033[2J", stdout);
		for (int y = 0; y < H; ++y) {
			for (int x = 0; x < W; ++x) {
				draw_cell(y * W + x);
			}
			fputs(UNICODE_ARTS == ARTS ? "\033[m│\n" : "\033[m|\n", stdout);
		}
		fputs("\033[m", stdout);
		for (int x = 0; x < W; ++x)
			fputs(UNICODE_ARTS == ARTS ? "──" : "--", stdout);
		fputs(UNICODE_ARTS == ARTS ? "┘\n\033[m" : "\n\033[m", stdout);
	}
	num_damages = 0;
}

static void
draw_number(int n, int m)
{
	for (; m; m /= 10) {
		int digit = n / m;
		n %= m;

		if (UNICODE_ARTS == ARTS)
			/* Unicode SEGMENTED DIGIT ZERO */
			printf("\xf0\x9f\xaf%c ", 0xb0 + digit);
		else
			printf("%d ", digit);
	}
}

static void
draw_status(void)
{
	if (old_score != score || !partially_damaged) {
		old_score = score;
		printf("\033[%d;%dH", 1 + H + 1, 1);
		draw_number(old_score, 100000);
	}

	if (old_timeout != food_timeout || !partially_damaged) {
		old_timeout = food_timeout;
		printf("\033[%d;%dH", 1 + H + 1, 1 + (W - 2) * 2);
		if (old_timeout)
			draw_number(old_timeout, 10);
		else
			printf("    ");
	}
}

static void
draw(void)
{
	draw_jungle();
	draw_status();
	partially_damaged = 1;
	fflush(stdout);
}

static void
move(int *y, int *x, enum direction d)
{
	*y = (*y + (char)((((H - 1) <<   UP * 8) | (1 <<  DOWN * 8)) >> (d * 8))) % H;
	*x = (*x + (char)((((W - 1) << LEFT * 8) | (1 << RIGHT * 8)) >> (d * 8))) % W;
}

static void
plant(int pos, enum type t)
{
	partially_damaged &= num_damages < 20;
	if (partially_damaged)
		jungle_damage[num_damages++] = pos;
	jungle[pos] = t;
}

static void
plant_yx(int y, int x, enum type t)
{
	plant(y * W + x, t);
}

static void
plant_yxh(int y, int x, int n, enum type t)
{
	for (int i = 0; i < n; ++i)
		plant_yx(y, x + i, t);
}

static void
plant_yxv(int y, int x, int n, enum type t)
{
	for (int i = 0; i < n; ++i)
		plant_yx(y + i, x, t);
}

static int
plant_random(enum type t)
{
	int n = 0;
	for (int pos = 0; pos < H * W; ++pos)
		n += T_GROUND == jungle[pos];
	if (!n)
		return -1;
	n = rand() % n;

	for (int pos = 0;; ++pos) {
		if (T_GROUND == jungle[pos] && !n--) {
			plant(pos, t);
			return pos;
		}
	}

}

static void
plant_text(int y, int x, char const *str)
{
	for (; *str; ++str) {
		enum type t = ' ' == *str ? T_GROUND : T_ALPHABET + (*str - 'A');
		plant_yx(y, x, t);
		move(&y, &x, RIGHT);
	}
}

static void
plant_ctext(int y, char const *str)
{
	plant_text(y, (W - strlen(str)) / 2, str);
}

static int
have(enum type t)
{
	for (int i = 0; i < H * W; ++i)
		if (t == (enum type)jungle[i])
			return 1;
	return 0;
}

static void
plant_food(void)
{
	int p = rand() % 1024;
	if (p < 50 && !have(T_HOLE)) {
		plant_random(T_HOLE);
	} else if (p < 300 && yfood < 0) {
		int p = rand() % 32;
		enum type t;
		if (p < 10)
			t = T_SNAIL;
		else if (p < 20)
			t = T_BEETLE;
		else if (p < 30)
			t = T_ANT;
		else
			t = T_PRESENT;
		int pos = plant_random(t);
		if (pos < 0)
			return;
		yfood = pos / W;
		xfood = pos % W;
		food_dir = T_SNAIL == t
			? (rand() % 2 ? LEFT : RIGHT)
			: rand() % 4;
		if (rand() % 16 < 15)
			food_timeout = 30;
	} else {
		int p = rand() % 32;
		enum type t;
		if (p < 17)
			t = T_EGG;
		else if (p < 30)
			t = T_SNAIL;
		else
			t = T_BEETLE;
		plant_random(t);
	}
}

static void
plant_foodn(int i)
{
	for (; 0 < i; --i)
		plant_food();
}

static void
move_food(void)
{
	if (0 < food_timeout && !--food_timeout) {
		plant_yx(yfood, xfood, T_GROUND);
		return;
	}

	if (yfood < 0) {
		food_timeout = 0;
		return;
	}

	int y = yfood, x = xfood;
	move(&y, &x, food_dir);
	enum type t = jungle[y * W + x];
	if (T_GROUND != t && !(T_HEAD <= t && t < T_HEAD + 4))
		food_dir = opposite(food_dir);

	y = yfood, x = xfood;
	move(&y, &x, food_dir);
	if (T_GROUND != jungle[y * W + x])
		return;
	t = jungle[yfood * W + xfood];
	plant_yx(yfood, xfood, T_GROUND);
	plant_yx((yfood = y), (xfood = x), t);
}

static int
have_special_food(void)
{
	for (int i = 0; i < H * W; ++i)
		switch (jungle[i]) {
		case T_SNAIL:
		case T_BEETLE:
		case T_EGG:
		case T_ANT:
		case T_PRESENT:
			return 1;
		}

	return 0;
}

static int
move_snake(void)
{
	if (next_snake_dir != opposite(snake_dir))
		snake_dir = next_snake_dir;
	next_snake_dir = snake_dir;

	if (snake_growth <= 0) {
		if (snake_growth < 0)
			++snake_growth;

		enum type t = jungle[ytail * W + xtail];
		enum direction tail_dir = t < T_SNAKE ? snake_dir : (t - T_SNAKE) % 4;
		/* if (!(ytail == yhead && xtail == xhead)) */
			plant_yx(ytail, xtail, T_GROUND);
		move(&ytail, &xtail, tail_dir);

		/* if (ytail == yhead && xtail == xhead)
			snake_growth = 0; */
	}


	if (0 <= snake_growth) {
		if (0 < snake_growth)
			--snake_growth;

		int prev_pos = yhead * W + xhead;
		move(&yhead, &xhead, snake_dir);
		int new_pos = yhead * W + xhead;
		int bug = new_pos == yfood * W + xfood;
		if (bug)
			yfood = -1;
		enum type new = jungle[new_pos];
		if (new == T_WALL || (T_HEAD <= new && new < T_SNAKE_END)) {
			plant(new_pos, T_HIT);
			return 0;
		} else switch (new) {
		case T_HEAD:
		case T_SNAKE:
		case T_FAT_SNAKE:
		case T_WALL:
			/* Handled above. */
			break;

		case T_GROUND:
		case T_ALPHABET:
			/* Ok. Move along. */
			break;

		case T_HOLE:
			snake_growth = -9999;
			break;

		case T_MEAT:
			snake_growth += 1;
			score += speed;
			plant_random(T_MEAT);
			/* Otherwise player would not be motivated to
			 * pick up foods immediately. */
			if (!have_special_food())
				plant_foodn(rand() % 4);
			break;

		case T_SNAIL:
		case T_BEETLE:
		case T_EGG:
		case T_ANT:
			score += (speed + rand() % (speed * speed)) << bug;
			snake_growth += 1;
			break;

		case T_PRESENT:
			plant_foodn(2 + rand() % 4);
			break;

		case T_STAR:
			star_bonus += 10;
			break;

		case T_HIT:
			/* Unreachable. */
			break;
		}
		if (T_HOLE != new)
			plant(new_pos, T_HEAD + snake_dir);
		enum type old_into = T_GROUND;
		if (new_pos != ytail * W + xtail) {
			enum type base = snake_growth <= 0 ? T_SNAKE : T_FAT_SNAKE;
			old_into = base + opposite(jungle[prev_pos] - T_HEAD) * 4 + snake_dir;
		}
		plant(prev_pos, old_into);
	}
	return 1;
}

static int
move_world(void)
{
	return move_snake() && (move_food(), 1);
}

static int
run(void)
{
	static long const NSEC_PER_MSEC = 1000000;
	static long const NSEC_PER_SEC = NSEC_PER_MSEC * 1000;

	sigset_t sigmask;
	sigemptyset(&sigmask);

	struct pollfd fd;
	fd.fd = STDIN_FILENO;
	fd.events = POLLIN;

	struct timespec last_frame;

	goto first_draw;
	for (;;) {
		enum type head = jungle[yhead * W + xhead];
		if (T_GROUND == head)
			break;
		int in_hole = T_HOLE == head;
		int frame_duration = SPEED_DELAYS[speed - 1] >> in_hole;

		struct timespec next_frame;
		next_frame.tv_sec = last_frame.tv_sec;
		next_frame.tv_nsec = last_frame.tv_nsec + frame_duration * NSEC_PER_MSEC;
		next_frame.tv_sec += NSEC_PER_SEC <= next_frame.tv_nsec;
		next_frame.tv_nsec -= NSEC_PER_SEC <= next_frame.tv_nsec ? NSEC_PER_SEC : 0;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		struct timespec timeout;
		timeout.tv_sec = next_frame.tv_sec - now.tv_sec;
		timeout.tv_nsec = next_frame.tv_nsec - now.tv_nsec;
		timeout.tv_sec -= timeout.tv_nsec < 0;
		timeout.tv_nsec += timeout.tv_nsec < 0 ? NSEC_PER_SEC : 0;

		/* Missed frame. */
		if (timeout.tv_sec < 0) {
			timeout.tv_sec = 0;
			timeout.tv_nsec = 0;
		}

		int rc = ppoll(&fd, 1, paused ? NULL : &timeout, &sigmask);
		if (rc < 0)
			continue;

		if (!rc) {
			if (!move_world())
				return -1;
		first_draw:
			draw();
			clock_gettime(CLOCK_MONOTONIC, &last_frame);
			continue;
		}

		if (~POLLIN & fd.revents)
			exit(EXIT_FAILURE);

		char key;
		if (1 != read(fd.fd, &key, sizeof key))
			continue;

		if (mouse) switch (key) {
		case 'A':
			next_snake_dir = turn_right(snake_dir);
			paused = 0;
			break;

		case 'B':
			next_snake_dir = turn_left(snake_dir);
			paused = 0;
			break;

		case ' ':
		case 'p':
			paused ^= 1;
			break;
		} else switch (key) {
		case '\033':
		case '[':
			/* Explicitly ignore. */
			/* A-D is for \033[X arrow keys. */
			break;

		case 'h':
		case 'a':
		case 'D':
		case '4':
			next_snake_dir = LEFT;
			paused = 0;
			break;

		case 'j':
		case 's':
		case 'B':
		case '5':
		case '2':
			next_snake_dir = DOWN;
			paused = 0;
			break;

		case 'k':
		case 'w':
		case 'A':
		case '8':
			next_snake_dir = UP;
			paused = 0;
			break;

		case 'l':
		case 'd':
		case 'C':
		case '6':
			next_snake_dir = RIGHT;
			paused = 0;
			break;

		case ' ':
		case 'p':
			paused ^= 1;
			break;
		}
	}

	return yhead * W + xhead;
}

static void
plant_snake(int y, int x, enum direction d)
{
	ytail = yhead = y;
	xtail = xhead = x;
	next_snake_dir = snake_dir = d;
	plant_yx(yhead, xhead, T_HEAD + snake_dir);
}

static void enter_random_map(void);

static void
marathon(void)
{
	if (0 <= run())
		enter_random_map();
}

static void
enter_map_classic(void)
{
	vacuum_jungle();
	plant_snake(12, 18, LEFT);
	plant_random(T_MEAT);
	marathon();
}

static void
enter_map_around(void)
{
	vacuum_jungle();
	plant_yxh(0, 0, W, T_WALL);
	plant_yxv(0, 0, H, T_WALL);
	plant_yxv(0, W - 1, H, T_WALL);
	plant_yxh(H - 1, 0, W, T_WALL);
	plant_snake(H / 2, W / 2, rand() % 4);
	plant_random(T_MEAT);
	marathon();
}

static void
enter_map_corners(void)
{
	int Py = 7;

	vacuum_jungle();
	/* Going clockwise starting from top left corner. */
	plant_yxv(0, 0, 3, T_WALL);
	plant_yx(0, 1, T_WALL);
	plant_yx(0, W - 2, T_WALL);
	plant_yxv(0, W - 1, 3, T_WALL);
	plant_yxv(H - 3, W - 1, 3, T_WALL);
	plant_yx(H - 1, W - 2, T_WALL);
	plant_yx(H - 1, 1, T_WALL);
	plant_yxv(H - 3, 0, 3, T_WALL);
	/* Bars. */
	int y = (H - Py) / 2 - 1, x = W / 4, xn = W - 2 * x;
	plant_yxh(y, x, xn, T_WALL);
	plant_yxh(H - 1 - y, x, xn, T_WALL);
	plant_snake(H / 2 + rand() % 4 - 2, W / 2, rand() % 2 ? LEFT : RIGHT);
	plant_random(T_MEAT);
	marathon();
}

static void
enter_map_whirpool(void)
{
	int Pc = 1;
	int Px = 3;

	vacuum_jungle();
	int yn = (H - Pc) / 2, xn = (W - Pc) / 2;
	int yoff = yn - Px - 1, xoff = xn + Px + 1;
	plant_yxh(yoff, 0, xn, T_WALL);
	plant_yxh(H - 1 - yoff, W - xn, xn, T_WALL);
	plant_yxv(0, xoff, yn, T_WALL);
	plant_yxv(H - yn, W - 1 - xoff, yn, T_WALL);
	plant_snake(H / 2, W / 2, rand() % 4);
	plant_random(T_MEAT);
	marathon();
}

static void
enter_map_cross(void)
{
	vacuum_jungle();
	plant_yxh(H / 2, W / 2 - W / 4, W / 2 | 1, T_WALL);
	plant_yxv(H / 2 - H / 4, W / 2, H / 2 | 1, T_WALL);
	int y = rand() % 2 ? H - 1 - H / 8 : H / 8;
	int x = rand() % 2 ? W - 1 - W / 8 : W / 8;
	plant_snake(y, x, rand() % 4);
	plant_random(T_MEAT);
	marathon();
}

static void
enter_map_four(void)
{
	vacuum_jungle();
	plant_yxh(H / 2, 0, W, T_WALL);
	plant_yxv(0, W / 2, H, T_WALL);
	int y = H / 4 + (rand() % 2 ? H / 2 : 0);
	int x = W / 4 + (rand() % 2 ? W / 2 : 0);
	enum direction d = rand() % 2
		? (y < H / 2 ? UP : DOWN)
		: (x < W / 2 ? LEFT : RIGHT);
	plant_snake(y, x, d);
	plant_random(T_MEAT);
	marathon();
}

static struct map const MAPS[] = {
	{ "CLASSIC", enter_map_classic },
	{ "AROUND", enter_map_around },
	{ "CORNERS", enter_map_corners },
	{ "CROSS", enter_map_cross },
	{ "WHIRPOOL", enter_map_whirpool },
	{ "FOUR", enter_map_four },
};

static void
enter_random_map(void)
{
	MAPS[rand() % ARRAY_SIZE(MAPS)].enter();
}

static void
plant_button(int y, int x, char const *text)
{
	plant_yx(y, x, T_HOLE);
	plant_text(y, x + 2, text);
}

static void
enter_welcome_menu(void);

static void
enter_speed_menu(void)
{
	vacuum_jungle();
	plant_ctext(1, "SPEED");
	plant_text(3, 0, "SET");
	plant_text(3, W - 6, "SLOWER");
	for (int i = 1; i <= 9; ++i) {
		plant_yxh(3 + i, (W - 9) / 2, 9 - i + 1, T_STAR);
		plant_yx(3 + i, 0, T_HOLE);
		plant_yx(3 + i, W - i, i == 9 ? T_WALL : T_HOLE);
	}
	plant_snake(4 + (9 - speed), 1, RIGHT);
	int x = W - 9;
	plant_yx(15, x - 1, T_EGG);
	plant_yxv(15, x, 4, T_WALL);
	plant_yx(16, x - 1, T_WALL);
	plant_yx(16, x + 1, T_SNAIL);
	plant_yx(17, x - 1, T_BEETLE);
	plant_yxh(17, x, 3, T_WALL);
	plant_yx(17, x + 3, T_EGG);
	plant_text(15, 1, "BACK");
	plant_yxh(16, 1, 4, T_HOLE);
	plant_yx(16, 0, T_WALL);
	plant_random(T_MEAT);

	int rc = run();
	if (18 == rc / W) {
		return;
	} else if (4 * W <= rc && rc < (4 + 10) * W) {
		speed = 9 - (rc / W - 4);
		speed -= 1 < (rc % W);
	}
	enter_speed_menu();
}

static void
wait_user(void)
{
	sigset_t sigmask;
	sigemptyset(&sigmask);

	struct pollfd fd;
	fd.fd = STDIN_FILENO;
	fd.events = POLLIN;

	draw();

	for (;;) {
		int rc = ppoll(&fd, 1, NULL, &sigmask);
		if (rc < 0)
			continue;

		if (~POLLIN & fd.revents)
			exit(EXIT_FAILURE);

		char key;
		if (1 != read(fd.fd, &key, sizeof key))
			continue;
		if (' ' == key || 'p' == key)
			break;
	}
}

static void
enter_maps_menu(int sel, int autoplay)
{
	vacuum_jungle();
	plant_ctext(1, "MAPS");
	plant_snake(4 + sel * 2, 2, RIGHT);
	for (int i = 0; i < ARRAY_SIZE(MAPS); ++i)
		plant_button(4 + i * 2, 4, MAPS[i].name);
	plant_button(H - 2, 2, "BACK");
	paused = !autoplay;

	int rc = run();
	if (4 * W <= rc && rc < 17 * W) {
		sel = (rc / W - 4) / 2;
		score = 0;
		MAPS[sel].enter();
		wait_user();
	} else if (H - 2 == rc / W) {
		enter_welcome_menu();
		return;
	}

	enter_maps_menu(sel, 0);
}

static void
enter_about_menu(void)
{
	vacuum_jungle();
	plant_ctext(1, "ABOUT");
	plant_ctext(4, "WRITTEN BY");
	plant_button(5, 4, "ZSUGABUBUS");
	plant_snake(5, 17, LEFT);
	plant_ctext(7, "LICENSE");
	plant_ctext(8, "UNLICENSE");
	plant_ctext(10, "BUGS");
	plant_text(11, 4, "GITHUB");
	plant_text(12, 6, "ZSUGABUBUS");
	plant_text(13, 13, "SNAKE");
	paused = 1;
	run();
}

static void
enter_welcome_menu(void)
{
	vacuum_jungle();
	plant_ctext(1, "SNAKE");
	if (mouse) {
		plant_ctext(5, "SCRL UP    TURN RIGHT");
		plant_ctext(6, "SCRL DOWN  TURN LEFT ");
		plant_ctext(7, "SPACE      PAUSE     ");
	} else {
		plant_ctext(4, "H A    LEFT ");
		plant_ctext(5, "J S    DOWN ");
		plant_ctext(6, "K W    UP   ");
		plant_ctext(7, "L D    RIGHT");
		plant_ctext(8, "SPACE  PAUSE");
	}

	int xn = 14;
	int x = (W - xn) / 2;
	plant_button(11, x, "PLAY");
	plant_snake(11, W - 1 - x, LEFT);
	plant_button(13, x + 2, "MAPS");
	plant_button(15, x + 4, "SPEED");
	plant_button(17, x + 6, "ABOUT");

	time_t now = time(NULL);
	struct tm const *tm = localtime(&now);
	int stars = 0;
	if (20 <= tm->tm_hour)
		stars = 3;
	else if (tm->tm_hour <= 3)
		stars = 5;
	else if (tm->tm_hour <= 6)
		stars = 9;
	for (int i = 0; i < stars; ++i)
		plant_random(T_STAR);

	plant_random(T_MEAT);

	switch (run() / W) {
	case 11:
		score = 0;
		enter_random_map();
		wait_user();
		break;

	case 13:
		enter_maps_menu(0, 0);
		break;

	case 15:
		enter_speed_menu();
		break;

	case 19:
		enter_about_menu();
		break;

	default:
		exit(EXIT_SUCCESS);
	}

	enter_welcome_menu();
}

static void
handle_interrupt(int sig)
{
	(void)sig;
	exit(EXIT_FAILURE);
}

static void
restore_term(void)
{
	tcsetattr(STDOUT_FILENO, TCSANOW, &saved_termios);
	fputs("\033[?25h", stdout);
	fputs("\033[?1049l", stdout);
	fflush(stdout);
}

static void
save_term(void)
{
	tcgetattr(STDOUT_FILENO, &saved_termios);
	atexit(restore_term);
}

static void
prepare_term(void)
{
	struct termios new_termios = saved_termios;
	new_termios.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDOUT_FILENO, TCSANOW, &new_termios);

	/* Hide cursor (DECTCEM). */
	fputs("\033[?25l", stdout);
	/* Use alt screen. */
	fputs("\033[?1049h", stdout);
	fflush(stdout);
}

static void
handle_continue(int sig)
{
	(void)sig;
	prepare_term();
	partially_damaged = 0;
	draw();
}

static void
print_m_help(FILE *stream)
{
	fprintf(stream, "Available maps:\n");
	for (int i = 0; i < ARRAY_SIZE(MAPS); ++i)
		fprintf(stream, "  %s\n", MAPS[i].name);
}

static void
print_s_help(FILE *stream)
{
	fprintf(stream, "Available speed levels:\n");
	for (int i = 1; i <= 9; ++i) {
		char const *s = "";
		if (1 == i)
			s = " (slowest)";
		else if (9 == i)
			s = " (fastest)";
		fprintf(stream, "  %-6d%3d ms%s\n", i, SPEED_DELAYS[i - 1], s);
	}
}

static void
print_t_help(FILE *stream)
{
	fprintf(stream, "Available themes:\n");
	fprintf(stream, "  ascii\n");
	fprintf(stream, "  ascii-block\n");
	fprintf(stream, "  unicode\n");
}

int
main(int argc, char *argv[])
{
	srand(time(NULL));
	setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
	sigset_t sigmask;
	sigfillset(&sigmask);
	pthread_sigmask(SIG_SETMASK, &sigmask, NULL);
	signal(SIGINT, handle_interrupt);
	signal(SIGTERM, handle_interrupt);
	signal(SIGQUIT, handle_interrupt);
	signal(SIGCONT, handle_continue);
	signal(SIGWINCH, handle_continue);

	int map = -1;

	for (int opt; 0 < (opt = getopt(argc, argv, "hm:s:t:M"));) switch (opt) {
	case 'h':
		printf(USAGE);
		return EXIT_SUCCESS;

	case 'm':
		if (!strcmp(optarg, "help")) {
			print_m_help(stdout);
			return EXIT_SUCCESS;
		}
		for (int i = 0; i < ARRAY_SIZE(MAPS); ++i)
			if (!strcmp(optarg, MAPS[i].name))
				map = i;
		if (map < 0) {
			fprintf(stderr, USAGE);
			print_m_help(stderr);
			return EXIT_FAILURE;
		}
		break;

	case 'M':
		mouse = 1;
		break;

	case 's':
		if (!strcmp(optarg, "help")) {
			print_s_help(stdout);
			return EXIT_SUCCESS;
		}
		speed = atoi(optarg);
		if (!(1 <= speed && speed <= 9)) {
			fprintf(stderr, USAGE);
			print_s_help(stderr);
			return EXIT_FAILURE;
		}
		break;

	case 't':
		if (!strcmp(optarg, "help")) {
			print_t_help(stdout);
			return EXIT_SUCCESS;
		}
		if (!strcmp(optarg, "ascii")) {
			ARTS = ASCII_ARTS;
		} else if (!strcmp(optarg, "ascii-block")) {
			ARTS = ASCII_BLOCK_ARTS;
		} else if (!strcmp(optarg, "unicode")) {
			ARTS = UNICODE_ARTS;
		} else {
			fprintf(stderr, USAGE);
			print_t_help(stderr);
			return EXIT_FAILURE;
		}
		break;

	case '?':
		fprintf(stderr, USAGE);
		return EXIT_FAILURE;

	default:
		abort();
	}

	save_term();
	prepare_term();
	if (0 <= map)
		enter_maps_menu(map, 1);
	else
		enter_welcome_menu();
}
