/**
 * @file    mygame.c
 * @Author  Gareth Sharpe
 * @date    February 14, 2018
 * @usage   gcc -o mygame mygame.c -lncurses 
 * @brief   A simple flight simulator game.
 * 
 */

#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#define DELAY       35000
#define PLANEWIDTH  16
#define ENEMYWIDTH  5
#define MAGSIZE     10
#define ENEMIES     8   
#define MAXHEALTH   10
#define FRIENDLY    TRUE
#define SHOTGUN     5
#define MINALRM     1
#define MAXALRM     6

/**
* Data Structures
*/
typedef struct bullet {
  int x;
  int y;
  int dir;
  int alive;
  char s;
} bullet;

typedef struct enemy {
  char *top;
  char *cab;
  char *mid;
  char *bot;
  int x;
  int y;
  int alive;
} enemy;

/**
* Function Prototypes
*/
void    display_splash        ();
void    display_game_over     ();
int     select_plane          ();
void    init_mag              (bullet *friendly_mag, int friendly);
void    init_enemies          (enemy *enemies);
int     update_bullets        (bullet *mag, int bullets, int friendly);
void    update_enemies        (bullet *friendly_mag, bullet *enemy_mag, enemy *enemies);
int     update_health         (bullet *enemy_mag, int health, int x, int y);
void    spawn_enemy           (enemy *e, int x, int y);
void    shoot_bullet          (bullet *b, int x, int y);
void    draw_mag              (int bullets);
void    draw_health           (int health);
int     my_random             (int min, int max);
void    start_timer           ();
long    stop_timer            ();
void    alarm_handler         (int signal);
int     calculate_score       (float time_alive, int enemies_destroyed);

/**
* Global Variables
* maintains starting and finishing wall time.
*/
struct timeval start, end;

/**
* Global Variables
* maintains enemy state - required to be global
* to be used with timer handler.
*/
enemy *enemies;
int enemy_bullet_index = 0,
    num_enemies = 0,
    enemy_index = 0,
    enemies_destroyed = 0,
    alarm_delay = 0;

/**
* Main function.
* @param  void
* @return exit_success.
*/
int main(void) {
  /**
  * Local Variables
  * maintains the state of the current window size
  * to determine x, y location of plane.
  */
  int x = 0,
      y = 0;
  int max_x = 0,
      max_y = 0;
  /**
  * Local Variables
  * stores user plane selection and games state.
  */
  int plane;
  char *plane_top,
       *plane_bot;
  int game_over = FALSE;
  /**
  * Local Variables
  * stores the distance the plane can move in
  * the x, y directions.
  */
  int xdirection = 3,
      ydirection = 1;
  /**
  * Local Variables
  * stores the planes health as well as magazine 
  * state.
  */
  int health = MAXHEALTH,
      bullet_index = 0,
      bullets = MAGSIZE;
  /**
  * Local Variables
  * stores timing information which is used to
  * to determine score along with enemies_destroyed.
  */
  long micros;
  float time_alive;
  int score;
  /**
  * Local Variable
  * the main window to use with ncurses.
  */
  WINDOW *mainwin;

  // initialize ncurses
  if ((mainwin = initscr()) == NULL ) {
    fprintf(stderr, "Error initialising ncurses.\n");
    exit(EXIT_FAILURE);
  }

  srand(time(NULL));                                      /* set random time seed */
  noecho();                                               /* turn off keyboard echo */
  curs_set(FALSE);                                        /* trun off cursor display */
  keypad(mainwin, TRUE);                                  /* turn on special characters */

  display_splash();                                       /* display welcome screen */

  plane = select_plane();                                 /* get plane selection from user */

  // set plane depending on user selection
  if (plane == 1) {
    plane_top = "      __!__   ";
    plane_bot = "----*---o---*----";
  } else 
  if (plane == 2) {
    plane_top = "      \\ . /   ";
    plane_bot = "o______(*)______o";
  } else {
    plane_top = "      \\ . /   ";
    plane_bot = "----==( o )==----";
  }

  // allocate memory for game variables
  bullet *friendly_mag = malloc(MAGSIZE * sizeof (bullet));
  bullet *enemy_mag = malloc(MAGSIZE * ENEMIES * sizeof (bullet));
  enemies = malloc (ENEMIES * sizeof (enemy));


  init_mag(friendly_mag, FRIENDLY);                       /* initialize friendly mag */
  init_mag(enemy_mag, !FRIENDLY);                         /* initialize enemy mag */
  init_enemies(enemies);                                  /* initialize enemies */

  getmaxyx(stdscr, max_y, max_x);                         /* get screen dimensions */
  x = max_x / 2 - (PLANEWIDTH / 2);                       /* set plane x to mid screen */
  y = max_y / 2;                                          /* set plane y to mid screen */

  timeout(30);                                            /* set getch() timeout to 30ms */
  signal(SIGALRM, alarm_handler);                         /* initialize SIGALRM with handler */
  alarm_delay = my_random(1, 6);                          /* get random interval for alarm delay */
  alarm(alarm_delay);                                     /* set random interval for alarm delay */
  start_timer();                                          /* start the time to determine score */

  while (!game_over) {

    clear();                                              /* clear screen */

    getmaxyx(stdscr, max_y, max_x);                       /* get screen dimensions */

    mvprintw(y-1, x, plane_top);                          /* draw plane top */
    mvprintw(y, x, plane_bot);                            /* draw plane bottom */

    bullets = update_bullets(friendly_mag, bullets, FRIENDLY); /* update friendly bullet positions */
    update_bullets(enemy_mag, 0, !FRIENDLY);              /* update enemy bullet positions */
    update_enemies(friendly_mag, enemy_mag, enemies);     /* update enemy plane positions */
    health = update_health(enemy_mag, health, x, y);      /* update friendly plane health */

    if (health <= 0)                                      /* if health drops below zero... */
      game_over = TRUE;                                   /* ...then game is over */

    draw_mag(bullets);                                    /* draw the remaining bullets */
    draw_health(health);                                  /* draw the remaining health */

    refresh();                                            /* refresh the screen */

    wborder(mainwin, '|', '|', '-', '-', '+', '+', '+', '+'); /* draw boarder around screen */

    int key = getch();                                    /* get user key press */

    usleep(30000);                                        /* sleep 300ms for fluid motion */

    switch (key) {

      case KEY_UP:                                        /* handle key up button push */
      case 'W':
      case 'w':
        if (y > 2)                                        /* if plane is not at top of screen... */
          y -= ydirection;                                /* ...move plane towards top of screen */
        break;

      case KEY_DOWN:                                      /* handle key down button push */
      case 'S':
      case 's':
        if (y < max_y - 2)                                /* if plane is not at bottom of screen... */
          y += ydirection;                                /* ...move plane towards bottom of screen */
        break;

      case KEY_LEFT:                                      /* handle key left button push */
      case 'A':
      case 'a':
        if (x > xdirection)                               /* if plane is not at left boundry of screen... */
          x -= xdirection;                                /* ...move plane towards left boundry of screen */
        break;

      case KEY_RIGHT:                                     /* handle right button push */
      case 'D':
      case 'd':
        if ((x + PLANEWIDTH + xdirection) < max_x)        /* if tip of right wing is not at right boundry... */
          x += xdirection;                                /* ...move plane towards right boundry of screen */
        break;

      case ' ':                                           /* handle space key push */
        if (bullets != 0) {                               /* if there are still bullets left... */
          bullets--;                                      /* ...decrement bullet count... */
          shoot_bullet(&friendly_mag[bullet_index++], x + (PLANEWIDTH / 2), y + 1); /* ...shoot a bullet... */
          bullet_index %= MAGSIZE;                        /* ...and update the bullet index */
        }
        break;

      case '\n':                                          /* handle enter button push */
        if (bullets >= SHOTGUN) {                         /* if there are enough bullets to use shotgun... */
          bullets -= SHOTGUN;                             /* ...decrement bullet count by shotgun amount */
          for (int i = 0; i < SHOTGUN; i++) {             /* shoot SHOTGUN many bullets from magazine... */
            shoot_bullet(&friendly_mag[bullet_index++], x + (i * PLANEWIDTH / 4), y + 1);
            bullet_index %= MAGSIZE;                      /* ...and update the bullet index after each shot */
          }
        }
        break;

      case 'Q':                                           /* handle the q key press */
      case 'q':
        game_over = TRUE;                                 /* quit the current game */
        break;

      default:                                            /* handle all other key presses */
        break;                                            /* simply break on invalid keys */
    }

    usleep(DELAY);                                        /* delay screen updating for smooth rendering */

  }

  micros = stop_timer();                                  /* stop timer */
  time_alive = micros / (float) 1000000;                  /* convert from microseconds to seconds */

  alarm(0);                                               /* disable alarm from triggering again */
  timeout(-1);                                            /* disable timeout for getch() */

  score = calculate_score(time_alive, enemies_destroyed); /* calculate score */
  display_game_over(mainwin, score);                      /* display game over screen and user score */

  // clean up
  endwin();                                             
  free(friendly_mag);
  free(enemy_mag);
  free(enemies);

  return EXIT_SUCCESS;
}

void display_splash() {
  /**
  * Local Variables
  * stores max screen dimensions.
  */
  int max_x = 0,
      max_y = 0;

  getmaxyx(stdscr, max_y, max_x);                          /* get max screen dimensions */

  // write main screen
  mvprintw(0, max_x / 2 - 30, " _______ __ __         __     __        _______ __           ");
  mvprintw(1, max_x / 2 - 30, "|    ___|  |__|.-----.|  |--.|  |_     |     __|__|.--------.");
  mvprintw(2, max_x / 2 - 30, "|    ___|  |  ||  _  ||     ||   _|    |__     |  ||        |");
  mvprintw(3, max_x / 2 - 30, "|___|   |__|__||___  ||__|__||____|    |_______|__||__|__|__|");
  mvprintw(4, max_x / 2 - 30, "               |_____|                                       ");
  mvprintw(10, (max_x / 2) - 13, "Press any key to continue...");
  mvprintw(15, max_x / 2 - 30, "                 .                             .                 ");
  mvprintw(16, max_x / 2 - 30, "                //                             \\\\                ");
  mvprintw(17, max_x / 2 - 30, "               //                               \\\\               ");
  mvprintw(18, max_x / 2 - 30, "              //                                 \\\\              ");
  mvprintw(19, max_x / 2 - 30, "             //                _._                \\\\             ");
  mvprintw(20, max_x / 2 - 30, "          .---.              .//| \\.             .---.          ");
  mvprintw(21, max_x / 2 - 30, "________ / .-. \\_________..-~ _.-._ ~-..________/ .-. \\_________");
  mvprintw(22, max_x / 2 - 30, "         \\ ~-~ /   /H-     `-=.___.=-'     -H\\  \\ ~-~ /         ");
  mvprintw(23, max_x / 2 - 30, "           ~~~    / H          [H]          H \\   ~~~           ");
  mvprintw(24, max_x / 2 - 30, "                 / _H_         _H_         _H_ \\                 ");                       

  refresh();                                              /* refresh main screen */
  getch();                                                /* wait for user to press any key */

  // clean up
  clear();
  refresh();
}

int select_plane() {
  /**
  * Local Variables
  * stores user selection and selection state.
  */
  int selection = 0,
      selected = FALSE;
  /**
  * Local Variables
  * stores max screen dimensions.
  */
  int max_x = 0,
      max_y = 0;
  /**
  * Local Variables
  * stores user selection marker x,y screen position.
  */
  int marker_x,
      marker_y;
  /**
  * Local Variables
  * stores user selection marker x,y screen position.
  */
  int first_y,
      second_y,
      third_y;;

  getmaxyx(stdscr, max_y, max_x);                         /* get max screen dimensions */

  marker_x = (max_x / 2) - 38;                            /* calculate starting marker x positon */
  marker_y = max_y / 3;                                   /* calculate starting marker y positon */

  first_y = max_y / 3;                                    /* calculate position of first  [ ] */
  second_y = max_y / 2;                                   /* calculate position of second [ ] */
  third_y = max_y / 2 + (max_y / 2 - max_y / 3);          /* calculate position of thrid  [ ] */

  // print title to screen
  mvprintw(0, max_x / 2 - 40, " _______         __              __        _______ __                   ___ __   ");
  mvprintw(1, max_x / 2 - 40, "|     __|.-----.|  |.-----.----.|  |_     |   _   |__|.----.----.---.-.'  _|  |_ ");
  mvprintw(2, max_x / 2 - 40, "|__     ||  -__||  ||  -__|  __||   _|    |       |  ||   _|   _|  _  |   _|   _|");
  mvprintw(3, max_x / 2 - 40, "|_______||_____||__||_____|____||____|    |___|___|__||__| |__| |___._|__| |____|");
  mvprintw(4, max_x / 2 - 40, "                                                                                 ");
  mvprintw(6, max_x / 2 - 13, "Please select an aircraft.");

  // print first plane option to screen
  mvprintw(first_y - 1, max_x / 2 - PLANEWIDTH, "      __!__   ");
  mvprintw(first_y,     max_x / 2 - PLANEWIDTH, "----*---o---*----");

  // print second plane option to screen
  mvprintw(second_y - 1, max_x / 2 - PLANEWIDTH, "      \\ . /   ");
  mvprintw(second_y,     max_x / 2 - PLANEWIDTH, "o______(*)______o");

  // print thrid plane option to screen
  mvprintw(third_y - 1, max_x / 2 - PLANEWIDTH, "      \\ . /   ");
  mvprintw(third_y,     max_x / 2 - PLANEWIDTH, "----==( o )==----");

  // print selection brackets to the screen
  mvprintw(first_y,  max_x / 2 - 40, "[   ]");
  mvprintw(second_y, max_x / 2 - 40, "[   ]");
  mvprintw(third_y,  max_x / 2 - 40, "[   ]");

  mvprintw(marker_y, marker_x, "X");                      /* print user selection maker to screen */

  refresh();                                              /* refresh the screen */

  while (!selected) {                                     /* while the user has not selected a plane... */

    mvprintw(marker_y, marker_x, "X");                    /* print user selection marker */

    int key = getch();                                    /* get user key selection */

    switch (key) {

      case KEY_UP:                                        /* handle key up button press */
      case 'W':
      case 'w':
        mvprintw(marker_y, marker_x, " ");                /* remove current marker  */
        if (marker_y == first_y)                          /* if marker is on first selection... */
          marker_y = third_y;                             /* ...move to final selection */
        else if (marker_y == second_y)                    /* if marker on second selection... */
          marker_y = first_y;                             /* ...move to first selection */
        else                                              /* if maker on third selection... */
          marker_y = second_y;                            /* move to second selection */
        break;

      case KEY_DOWN:                                      /* handle key down button press (see above logic) */
      case 'S':
      case 's':
        mvprintw(marker_y, marker_x, " ");
        if (marker_y == first_y)
          marker_y = second_y;
        else if (marker_y == second_y)
          marker_y = third_y;
        else
          marker_y = first_y;
        break; 

      case 10:                                            /* handle enter key press */
        if (marker_y == first_y)
          selection = 1;
        else if (marker_y == second_y)
          selection = 2;
        else
          selection = 3;
        selected = TRUE;
        break;

    }

    usleep(DELAY);                                        /* delay for smooth marker rendering */

  }

  // clean up
  clear();
  refresh();

  return selection;
}

/**
* initialize a magazine. 
* @param  bullet    mag           pointer to an array of bullets.
* @param  int       friendly      > 0 if friendly, enemy otherwise.
* @return void
*/
void init_mag(bullet *mag, int friendly) {
  if (friendly) {
    for (int i = 0; i < MAGSIZE; i++) {
      bullet *b = (bullet*) malloc(sizeof (bullet));      
      b->x = 0;
      b->y = 0;
      b->dir = 1;
      b->s = '.';
      b->alive = FALSE;
      mag[i] = *b;
    }
  } else {
    for (int i = 0; i < ENEMIES * MAGSIZE; i++) {
      bullet *b = (bullet*) malloc(sizeof (bullet));
      b->x = 0;
      b->y = 0;
      b->dir = -1;
      b->s = '*';
      b->alive = FALSE;
      mag[i] = *b;
    }
  }
}

/**
* initialize an enemey. 
* @param  enemy    enemies     pointer to an array of enemies.
* @return void
*/
void init_enemies(enemy *enemies) {
  for (int i = 0; i < ENEMIES; i++) {
    enemy *e = (enemy*) malloc(sizeof (enemy));
    e->top = " .'.";
    e->cab = " |o|";
    e->mid = ".'o'.";
    e->bot = "|.-.|";
    e->x = 0;
    e->y = 0;
    e->alive = FALSE;
    enemies[i] = *e;
  }
}

/**
* Shoot a new bullet from a starting x,y position. 
* @param  bullet   b    pointer to a bullet in a magizine.
* @param  int      x    new bullet x position.
* @param  int      y    new bullet y position.
* @return void
*/
void shoot_bullet(bullet *b, int x, int y) {
    b->x = x;
    b->y = y;
    b->alive = TRUE;
}

/**
* Spawn a new enemey. 
* @param  bullet   mag          pointer to a magazine.
* @param  int      bullets      number of bullets.
* @param  int      friendly     > 0 if friendly, enemy otherwise.
* @return int      bullets      updated number of bullets in magazine
*/
int update_bullets(bullet *mag, int bullets, int friendly) {
  /**
  * Local Variables
  * stores max screen size and remaining
  * number of bullets in mag depending on
  * whether or not mag is friendly.
  */
  int max_x,
      max_y;
  int total_bullets;

  getmaxyx(stdscr, max_y, max_x);
  if (friendly)
    total_bullets = MAGSIZE;
  else
    total_bullets = MAGSIZE * ENEMIES;
  for (int i = 0; i < total_bullets; i++) {
    // if bullet at position i goes out of screen, end bullet
    if (mag[i].y >= max_y && mag[i].alive & mag[i].y != 0) {
      mag[i].alive = FALSE;
      bullets++;
    } else {
      // remove current bullet at position i, advance and print
      // the bullet in its new position
      mvprintw(mag[i].y, mag[i].x, " ");
      mag[i].y += mag[i].dir;
      mvprintw(mag[i].y, mag[i].x, &mag[i].s);
    }
  }
  return bullets;
}

/**
* Spawn a new enemey. 
* @param  enemy    e    pointer to an enemy to spawn.
* @param  int      x    new enemy x position.
* @param  int      y    new enemy y position.
* @return void
*/
void spawn_enemy(enemy *e, int x, int y) {
  e->x = x;
  e->y = y;
  e->alive = TRUE;
  num_enemies++;
}

/**
* Draw current plane magazine capacity on screen. 
* @param  int      bullets      current number of bullets in magazine.
* @return void
*/
void draw_mag(int bullets) {
  /**
  * Local Variables
  * stores max screen size.
  */
  int max_x,
      max_y;

  getmaxyx(stdscr, max_y, max_x);
  for (int i = 0; i < MAGSIZE; i++) {
    if (i < bullets)
      mvprintw(i + 2, max_x - 3, "o");
    else
      mvprintw(i + 2, max_x - 3, " ");
  }
}

/**
* Draw current plane health on screen. 
* @param  int      health        current plane health.
* @return void
*/
void draw_health(int health) {
  for (int i = 0; i < health; i++)
    mvprintw(1, i + 2, "+");
}

/**
* Updates all enemy positions as well as their alive states. 
* @param  bullet   friendly_mag     pointer to friendly magazine of bullets.
* @param  bullet   enemy_mag        pointer to enemy magazine of bullets.
* @param  enemy    enemeies         pointer to an array of enemies.
* @return void
*/
void update_enemies(bullet *friendly_mag, bullet *enemy_mag, enemy *enemies) {
  /**
  * Local Variables
  * stores random x,y positions of nex
  * enemy movements.
  */
  int xrand,
      yrand;
  /**
  * Local Variables
  * stores max screen size.
  */
  int max_x,
      max_y;

  getmaxyx(stdscr, max_y, max_x);
  for (int i = 0; i < ENEMIES; i++) {
    if (enemies[i].alive) {
      // if enemy is alive, draw it in its x,y position
      mvprintw(enemies[i].y - 3, enemies[i].x, enemies[i].top);
      mvprintw(enemies[i].y - 2, enemies[i].x, enemies[i].cab);
      mvprintw(enemies[i].y - 1, enemies[i].x, enemies[i].mid);
      mvprintw(enemies[i].y - 0, enemies[i].x, enemies[i].bot);

      // randomly decide next x,y movement direction
      // -1 for left or up, +1 for right or down
      xrand = my_random(-1, 1);
      while (enemies[i].x + xrand <= 0 || enemies[i].x + xrand + ENEMYWIDTH >= max_x)
        xrand = my_random(-1, 1);
      yrand = my_random(-1, 1);
      while (enemies[i].y + yrand <= 0 || enemies[i].y + yrand  >= max_y)
        yrand = my_random(-1, 1);

      // add x,y direction to current enemy position
      enemies[i].x += xrand;
      enemies[i].y += yrand;

      // if the randomly selected positions end up not
      // moving the enemy, shoot a bullet
      if (xrand == 0 && yrand == 0) {
        shoot_bullet(&enemy_mag[enemy_bullet_index++], enemies[i].x + 2, enemies[i].y - 4);
        enemy_bullet_index %= ENEMIES * MAGSIZE;
      } 
    } else {
      // if not alive, erase the current enemy
      mvprintw(enemies[i].y - 3, enemies[i].x, " ");
      mvprintw(enemies[i].y - 2, enemies[i].x, " ");
      mvprintw(enemies[i].y - 1, enemies[i].x, " ");
      mvprintw(enemies[i].y - 0, enemies[i].x, " ");
    }
    // for every bullet, if a bullet has reached an 
    // enemy, destroy that enemy
    for (int j = 0; j < MAGSIZE * ENEMIES; j++) {
      if (enemies[i].y == friendly_mag[j].y) {
        if (friendly_mag[j].x >= enemies[i].x && friendly_mag[j].x <= enemies[i].x + ENEMYWIDTH) {
          num_enemies--;
          enemies_destroyed++;
          enemies[i].alive = FALSE;
        }
      }
    }
  }
}

/**
* Returns a random number between min and max. 
* @param  int      min          minimum of range.
* @param  int      max          maximum of range.
* @return int                   a random integer between min and max.
*/
int my_random(int min, int max) {
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

/**
* Updates current health depending on bullet positions. 
* @param  int      signal       the signal id number
* @return void
*/
void alarm_handler(int signal) {
  /**
  * Local Variables
  * stores new x,y position for new enemy as 
  * well as screen dimensions.
  */
  int x,
      y;
  int max_x,
      max_y;

  // if more enemeies are allowed, spawn a new
  // enemy at a random x,y position on screen
  if (num_enemies < ENEMIES) {
    getmaxyx(stdscr, max_y, max_x);
    x = my_random(0, max_x - ENEMYWIDTH);
    y = max_y - 3;
    spawn_enemy(&enemies[enemy_index++], x, y);
    enemy_index %= ENEMIES;
  }

  // reset the alarm for a random duration
  alarm_delay = my_random(MINALRM, MAXALRM);
  alarm(alarm_delay);
}

/**
* Updates current health depending on bullet positions. 
* @param  bullet   enemy_mag     enemy magazine of bullets.
* @param  int      health        current plane health.
* @param  int      x             current plane x position.
* @param  int      y             current plane y position.
* @return int      health        updated plane health.
*/
int update_health(bullet *enemy_mag, int health, int x, int y) {
  // if a bullet has reached the plane, decrement it's health
  for (int i = 0; i < MAGSIZE * ENEMIES; i++) {
    if (enemy_mag[i].y == y) {
      if (enemy_mag[i].x >= x && enemy_mag[i].x <= x + PLANEWIDTH)
        health--;
    }
  }
  return health;
}

/**
* Memorizes the starting time. 
* @return void
*/
void start_timer() { 
    gettimeofday(&start, NULL);
}
 
/**
* Determines end time and computes the elapsed time. 
* @return time the reaction time of the user in msecs.
*/
long stop_timer() { 
  /**
  * Local Variables
  * stores time difference between end and start.
  */
  long time;

    gettimeofday(&end, NULL);
    time = 
      (end.tv_sec - start.tv_sec) 
      * 1000000 + 
      (end.tv_usec - start.tv_usec);
    return time;
}

/**
* Calculates score based on time alive and enemeies destroyed. 
* @return int         final score.
*/
int calculate_score(float time_alive, int enemies_destroyed) {
  return (int) time_alive * enemies_destroyed;
}

void display_game_over(WINDOW *mainwin, int score) {
  /**
  * Local Variables
  * stores x,y position for lettering as 
  * well as screen dimensions.
  */
  int x,
      y;
  int max_x,
      max_y;

  getmaxyx(stdscr, max_y, max_x);
  y = max_y;
  x = max_x / 2 - 33;
  // while the lettering has not reached the top of the screen...
  while (y > 0) {
    // print boarder 
    wborder(mainwin, '|', '|', '-', '-', '+', '+', '+', '+');
    // print game over screen with score
    mvprintw(y + 0, x, " _______                              _______                     ");
    mvprintw(y + 1, x, "|     __|.---.-..--------..-----.    |       |.--.--..-----..----.");
    mvprintw(y + 2, x, "|    |  ||  _  ||        ||  -__|    |   -   ||  |  ||  -__||   _|");
    mvprintw(y + 3, x, "|_______||___._||__|__|__||_____|    |_______| \\___/ |_____||__|  ");
    mvprintw(y + 4, x, "                                                                  ");
    mvprintw(y + 6, x + 25, "Final Score: %d", score);
    mvprintw(y + 7, x + 25, "                      ");
    mvprintw(y + 9, x + 23, "Press any key to quit.");
    mvprintw(y + 10, x + 23, "                      ");
    y--;

    // update screen
    refresh();
    usleep(100000);
  }

  // wait for input
  sleep(4);
  getch();
}