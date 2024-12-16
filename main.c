/* termios-example
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
#include <signal.h>

#include "key.h"

#define BUFF_LEN 4096
#define DELAY 0.1*1250000000  // 0.125 seconds
#define EMOD(a,b) (a%b + b)%b // euclidian mod to wrap around board

#define X 84
#define Y 32

typedef struct {
  size_t x;
  size_t y;
  size_t length;
} entity_t;

int emod(int sum, int max);
int** init_board();
void draw_board(const entity_t,int** board);
void signal_handler(int);
void reset_terminal();
void configure_terminal();
void read_input(keys_t*);
void move_cursor(const keys_t,entity_t*);
void move_direction(const KEY,entity_t*);

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
draw_board(const entity_t cursor,int** board) {
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
      if(dx == cursor.x && dy == cursor.y)
        printf("█");
      else
        printf(" ");
    }
    puts("|");
  }
  for(size_t dx = 0; dx < X+2;++dx) {
    printf("-");
  }
  puts("");
 
}

int**
init_board() {
  int** board = calloc(sizeof(int*),Y);

  for(size_t y = 0;y < Y; y++) {
    board[y] = calloc(sizeof(int),X);
  }

  return board;
}

void
move_direction(const KEY direction, entity_t* cursor) {
  switch(direction) {
    case(UP):
      cursor->y = EMOD(cursor->y-1,Y);
      break;
    case(DOWN):
      cursor->y = EMOD(cursor->y+1,Y);
      break;
    case(LEFT):
      cursor->x = EMOD(cursor->x-1,X);
      break;
    case(RIGHT):
      cursor->x = EMOD(cursor->x+1,X);
      break;
    case(NOOP):
      break;
    default:
      break;
  }
}

void
move_cursor(const keys_t reg_keys, entity_t* cursor) {
  for(size_t i = 0; i < reg_keys.head;++i) {
    switch(reg_keys.keys[i]) {
      case(UP):
        cursor->y = EMOD(cursor->y-1,Y);
        break;
      case(DOWN):
        cursor->y = EMOD(cursor->y+1,Y);
        break;
      case(LEFT):
        cursor->x = EMOD(cursor->x-1,X);
        break;
      case(RIGHT):
        cursor->x = EMOD(cursor->x+1,X);
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
    reg_keys->keys[reg_keys->head] = key;
    ++reg_keys->head;
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

  // read keys that can be pressed in a single frame

  signal(SIGINT,signal_handler);

  int** board = init_board();

  struct timespec req = {};
  struct timespec rem = {};

  entity_t cursor = {
    .x = 0,
    .y = 0,
    .length = 1
  };

  KEY direction = RIGHT;
  
  while(1) {

    keys_t keys_pressed = {
      .keys = malloc(sizeof(KEY) * BUFF_LEN),
      .cap = BUFF_LEN,
      .head = 0,
    };

    // register the keys pressed 
    read_input(&keys_pressed);
    KEY new_direction = keys_pressed.keys[keys_pressed.head - 1];
    if(new_direction != NOOP) {
      direction = new_direction;
    }
    //move_cursor(keys_pressed,&cursor);
    move_direction(direction,&cursor);
    draw_board(cursor,board);
    free(keys_pressed.keys);
    req.tv_nsec = DELAY;
    nanosleep(&req,&rem);
  }

  return 0;
}

