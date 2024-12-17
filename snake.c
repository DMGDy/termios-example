/* snake-termios
* Copyright (C) 2024 Dylan Dy or Dylan-Matthew Garza
* 
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include "config.h"

#ifdef SLOW
  #define DELAY 0.1*5000000000  
#elif defined(MEDIUM)
  #define DELAY 0.1*2500000000  
#elif defined(FAST)
  #define DELAY 0.1*1000000000
#else
  #define DELAY 0.1*2500000000  
#endif




#define BUFF_LEN 128
#define POOL_SIZE X*Y
#define  EMOD(a,b) (a%b + b)%b // euclidian mod to wrap around board

char* sprite[] = {"O","@","o"};
int lose = 0;
int score = 0;

// TODO: make directional keys its own enum
typedef enum {
  NOOP,
  UP,
  DOWN,
  LEFT,
  RIGHT,
} KEY;

typedef struct {
  KEY* keys;
  size_t cap;
  size_t head;
} keys_t;

typedef enum {
  CURSOR,
  POINT,
  TAIL,
}ENTITY_TYPE;


typedef struct {
  size_t x;
  size_t y;
  KEY direction;
}vector;

typedef struct entity_t{
  ENTITY_TYPE type;
  vector vec;
  vector prev_vec;
  struct entity_t* head;
  struct entity_t* tail;
} entity_t;

typedef struct {
  entity_t** pool;
  size_t count;
}entity_pool_t;

vector valid_pos(char*** board);
entity_t* spawn(ENTITY_TYPE,int,int,entity_t*,entity_t*,KEY);
void check_collision(entity_pool_t*,char***);
char*** init_board(void);
void draw_board(char***);
void signal_handler(int);
void reset_terminal();
void configure_terminal(void);
void read_input(keys_t*);
void move_cursor(const keys_t,entity_t*);
void render_board(const KEY,entity_pool_t*,char***);

static struct termios original_term, new_term;

// restore terminal to original state and flush stdout
void
reset_terminal() {
  // reset cursor color
  printf("\033[m");
  // show cursor
  printf("\033[?25h");
  fflush(stdout);

  tcsetattr(STDIN_FILENO,TCSANOW,&original_term);
}
// configure termios struct and set to be used
void
configure_terminal() {

  tcgetattr(STDIN_FILENO,&original_term);
  new_term = original_term;
  // ICANON: canonical input mode (read per line)
  // ECHO: write back typed input
  new_term.c_lflag &= !(ICANON|ECHO); // disable both
  // minimum characters to read non-canonical mode
  new_term.c_cc[VMIN] = 0;
  // minimum time to read characters in non-canonical mode
  new_term.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO,TCSANOW,&new_term);

  // hide cursor
  printf("\033[?25l");

  // on program exit reset
  atexit(reset_terminal);
}

void
draw_board(char*** board) {
  fflush(stdout);
  printf("\033[2J");
  printf("\x1b[H");

  //top border
  for(size_t dx = 0; dx < X+2;++dx) {
    printf("-");
  }
  puts("");

  for(size_t dy = 0; dy < Y;++dy) {
    printf("|");
    for(size_t dx =0; dx < X;++dx) {
      printf("%s",board[dy][dx]); 
    }
    puts("|");
  }
  for(size_t dx = 0; dx < X+2;++dx) {
    printf("-");
  }
  puts("");
 
}

char***
init_board() {
  char*** board = calloc(sizeof(char**),Y);

  for(size_t y = 0;y < Y; y++) {
    board[y] = calloc(sizeof(char*),X);
  }

  for(size_t dy = 0;dy < Y;++dy) {
    for(size_t dx = 0;dx < X;++dx) {
      board[dy][dx] = " ";
    }
  }

  return board;
}

void
check_collision(entity_pool_t* entity_pool,char*** board) {
  for(size_t i = 0; i < entity_pool->count;++i) {
    entity_t* current_entity = entity_pool->pool[i];
    // check if current entity is in same place as another
    if(current_entity->type == CURSOR) {
      for(size_t j = 0;j < entity_pool->count; ++j) {
        entity_t* entity = entity_pool->pool[j];
        if((entity->vec.x == current_entity->vec.x) && (entity->vec.y == current_entity->vec.y)) {
          if(entity->type == POINT) {
            // if head hits point, make new point, add to tail
            score++;
            board[entity->vec.y][entity->vec.x] = sprite[current_entity->type];
            free(entity);
            // make sure new point is somewhere where there isnt a tail
            vector valid = valid_pos(board);
            entity_pool->pool[j] = spawn(POINT,valid.x,valid.y,NULL,NULL,NOOP);

            entity_t* next = current_entity->tail;
            entity_t* prev = current_entity;
            while(next != NULL) {
              prev = next;
              next = next->tail;
            }
            next = spawn(TAIL,prev->prev_vec.x,prev->prev_vec.y,prev,NULL,prev->vec.direction);
            prev->tail = next;
            entity_pool->pool[entity_pool->count] = next;
            ++entity_pool->count;
          }
          else if(entity->type == TAIL) {
            lose = 1;
          }
        }
      }
    }
  }
}

void
render_board(const KEY direction, entity_pool_t* entity_pool,char*** board) {
  for(size_t i = 0; i < entity_pool->count; ++i) {
    entity_t* entity = entity_pool->pool[i];

    switch(entity->type) {
      case(CURSOR):
        entity->prev_vec = entity->vec;
        // move cursor based on direction
        switch(direction) {
          case(UP):
            board[entity->vec.y][entity->vec.x] = " ";
            entity->vec.y = EMOD(entity->vec.y-1,Y);
            break;
          case(DOWN):
            board[entity->vec.y][entity->vec.x] = " ";
            entity->vec.y = EMOD(entity->vec.y+1,Y);
            break;
          case(LEFT):
            board[entity->vec.y][entity->vec.x] = " ";
            entity->vec.x = EMOD(entity->vec.x-1,X);
            break;
          case(RIGHT):
            board[entity->vec.y][entity->vec.x] = " ";
            entity->vec.x = EMOD(entity->vec.x+1,X);
            break;
          case(NOOP):
            break;
          default:
            break;
        }
        break;
      case(TAIL):
        entity->prev_vec = entity->vec;
        board[entity->vec.y][entity->vec.x] = " ";
        entity->vec = entity->head->prev_vec;
        break;
      case(POINT):
        break;
      default:
        break;
    }
    board[entity->vec.y][entity->vec.x] = sprite[entity->type];
  }

}

vector
valid_pos(char*** board) {
  int x = (int)rand()%X;
  int y = (int)rand()%Y;
  while(strcmp(board[y][x]," ") != 0) {
    x = (int)rand()%X;
    y = (int)rand()%Y;  
  }

  vector v = {
    .x = x,
    .y = y,
    .direction = NOOP,
  };
  return v;
}

entity_t*
spawn(ENTITY_TYPE type,int x, int y,entity_t* head, entity_t* tail,KEY direction) {
  entity_t* new = malloc(sizeof(entity_t));

  vector vec = {
    .x = x,
    .y = y,
    .direction = direction 
  };

  new->vec = vec;
  new->prev_vec = vec;

  new->type = type;

  new->head = head;
  new->tail = tail;

  return new;
}

void
move_cursor(const keys_t reg_keys, entity_t* cursor) {
  for(size_t i = 0; i < reg_keys.head;++i) {
    switch(reg_keys.keys[i]) {
      case(UP):
        cursor->vec.y = EMOD(cursor->vec.y-1,Y);
        break;
      case(DOWN):
        cursor->vec.y = EMOD(cursor->vec.y+1,Y);
        break;
      case(LEFT):
        cursor->vec.x = EMOD(cursor->vec.x-1,X);
        break;
      case(RIGHT):
        cursor->vec.x = EMOD(cursor->vec.x+1,X);
        break;
      case(NOOP):
        break;
     default:
        break;
    }
  }
}

// read input from STDIN_FILENO
void
read_input(keys_t* reg_keys) {
  // all key presses during the frame
  char buff[BUFF_LEN] = {0};

  // use read syscall to read key live keypresses
  int n = read(STDIN_FILENO,buff,BUFF_LEN);
  // read through all typed stuff
  for(int i = 0;i < n; ++i) {

    KEY key = NOOP;
    // SIGINT: exit the program
    if(buff[i] == 3) {
      signal(SIGINT,signal_handler);
      free(reg_keys->keys);
      exit(0);
    }
    // obviously 'unsafe', can be at the last point of the buffer
    // escape ascii code + '['
    if(buff[i] == 27 && buff[i+1] == 91) {
      i += 2;
      // last ascii dictates the arrow key direction
      switch(buff[i]) {
        case('A'):
          key = UP;
          break;
        case('B'):
          key = DOWN;
          break;
        case('C'):
          key = RIGHT;
          break;
        case('D'):
          key = LEFT;
          break;
        default:
          key = NOOP;
          break;
      }
    }
    reg_keys->keys[reg_keys->head++] = key;
  }
}

void
signal_handler(int signum) {
  reset_terminal();
  signal(signum,SIG_DFL);
  raise(signum);
}


int
main(void) {
  configure_terminal();
  // set random seed with current time
  srand(time(NULL));
  // read keys that can be pressed in a single frame
  signal(SIGINT,signal_handler);
  char*** board = init_board();
  struct timespec req = {};
  struct timespec rem = {};

  entity_t* cursor = spawn(CURSOR,(int)random()%X,(int)random()%Y,NULL,NULL,RIGHT);
  entity_t* point = spawn(POINT,(int)random()%X,(int)random()%Y,NULL,NULL,NOOP);

  entity_pool_t entity_pool = {
    .pool = realloc(NULL,POOL_SIZE*sizeof(entity_t*)),
    .count = 0
  };

  entity_pool.pool[entity_pool.count++] = cursor;

  entity_pool.pool[entity_pool.count++] = point;

  KEY direction = RIGHT;
  
  while(!lose) {
    keys_t keys_pressed = {
      .keys = malloc(sizeof(KEY) * BUFF_LEN),
      .cap = BUFF_LEN,
      .head = 0,
    };

    // register the keys pressed 
    read_input(&keys_pressed);
    KEY new_direction = keys_pressed.keys[keys_pressed.head - 1];

    // change direction if input provided
    if(new_direction != NOOP) {
      if(!(direction == UP && new_direction == DOWN) && !(direction == DOWN && new_direction == UP) &&
          !(direction == RIGHT && new_direction == LEFT)&&!(direction==LEFT && new_direction == RIGHT)) {
        direction = new_direction;
      }
    }

    //move_cursor(keys_pressed,&cursor);
    render_board(direction,&entity_pool,board);
    check_collision(&entity_pool,board);
    draw_board(board);
    printf("Score: %d\n",score);
    free(keys_pressed.keys);
    req.tv_nsec = DELAY;
    nanosleep(&req,&rem);
  }
  printf("\nYou lose!\n");
  reset_terminal();

  return 0;
}

