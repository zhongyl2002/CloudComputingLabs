#ifndef MYGET_H
#define MYGET_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<dirent.h>
#include<assert.h>
#include<regex.h>
#include"libhttp.h"


void handle_static_file(int fd);

#endif