#include "myGetopt.h"

static int lopt, optIndex, optTmp;

/*--- 服务器参数 ---*/
struct in_addr server_addr;
int server_port, num_threads, server_proxy_port;
char server_proxy_hostname[128];

static struct option opts[] = {
    {"ip", required_argument, NULL, 'i'},
    {"port", required_argument, NULL, 'p'},
    {"threads", required_argument, NULL, 't'},
    {"proxy", required_argument, &lopt, 1},
};

char* lastCh(char* src, char ch_)
{
  int len = strlen(src);
  char* tmp = src + (len - 1);
  while (*tmp != ch_)
    tmp --;
  return tmp;
}

void parseCmdParameter(int argc, char **argv)
{
  while ((optTmp = getopt_long(argc, argv, "i:p:t:", opts, &optIndex)) != -1)
  {
    switch (optTmp)
    {
    case 0:
    {
      if (lopt == 1)
      {
        char *colon_pointer = lastCh(optarg, ':');
        if (colon_pointer != NULL)
        {
          memcpy(server_proxy_hostname, optarg, (colon_pointer - optarg));
          server_proxy_port = atoi(colon_pointer + 1);
        }
        else
        {
          size_t len = strlen(optarg);
          memcpy(server_proxy_hostname, optarg, len);
          server_proxy_hostname[len] = '\0';
          server_proxy_port = 80;
        }
        // printf("proxy{hostname:%s, port:%d}\n", server_proxy_hostname, server_proxy_port);
        break;
      }
    }
    case 'i':
    {
      if (!inet_aton(optarg, &server_addr))
        perror("parse parameter fail - inet_aton\n");
      // printf("ip:%s\n", inet_ntoa(server_addr));
      break;
    }
    case 'p':
    {
      server_port = atoi(optarg);
      // printf("port:%d\n", server_port);
      break;
    }
    case 't':
    {
      num_threads = atoi(optarg);
      // printf("thread:%d\n", num_threads);
      break;
    }
    default:
      printf("check the args[%s]!\n", argv[optind + 1]);
      exit(1);
    }
  }
  return;
}