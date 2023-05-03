#ifndef MYGETOPT_H
#define MYGETOPT_H

/*
    该头文件只要用于解析参数
*/

#include<getopt.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>


/*--- 服务器参数 ---*/
extern struct in_addr server_addr;
extern int server_port, num_threads, server_proxy_port;
extern char server_proxy_hostname[128];

// 解析命令行输入参数
void parseCmdParameter(int argc, char** argv);
char* lastCh(char* src, char ch_);

#endif