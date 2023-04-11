#ifndef MYSUDOKU_HPP
#define MYSUDOKU_HPP
#include <assert.h>
#include <memory.h>
#include <map>
#include <vector>
#include <stdio.h>
const int NUM = 9;
enum { ROW=9, COL=9, N = 81, NEIGHBOR = 20 };
void solve_sudoku_dancing_links(char* ch_, int* myBoard, int tmp);
#endif