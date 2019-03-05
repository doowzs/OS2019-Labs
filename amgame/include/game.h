#ifndef __GAME_H__
#define __GAME_H__

#include <am.h>
#include <amdev.h>
#include <klib.h>

#include <debug.h>

#ifndef bool
#define bool int8_t
#define true 1
#define false 0
#endif

#define GAME_ALL_FPS 24
#define GAME_TETRIS_FPS 4

struct Point {
  int x, y;
};

#include <tetris.h>

struct State {
  int width, height;
  int nextFrame, nextTetrisFrame;
  int score, keyCode;

  struct Tetromino tetromino;
};
extern struct State state;

void drawSquare(struct Point, int, uint32_t);

#endif
