#include <stdio.h>
#include <errno.h>

#include "myGetopt.h"
#include "myGet.h"

struct sockaddr_in server_address;
size_t len_address = sizeof(server_address);
int socket_number;

extern int server_port;

void socketListenStart()
{
  socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (socket_number == -1)
  {
    perror("Failed to create a new socket");
    exit(errno);
  }

  printf("ok\n");

  int socket_option = 1;
  if (setsockopt(socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                 sizeof(socket_option)) == -1)
  {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(socket_number, (struct sockaddr *)&server_address,
           sizeof(server_address)) == -1)
  {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(socket_number, 1024) == -1)
  {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);
}

void *serve()
{
  socketListenStart();
  struct sockaddr_in client_address;
  while (1)
  {
    int client_socket_number = accept(socket_number,
                                      (struct sockaddr *)&client_address,
                                      (socklen_t *)&len_address);
    if (client_socket_number < 0)
    {
      perror("Error accepting socket");
      exit(1);
    }
    handle_static_file(client_socket_number);
  }
  return NULL;
}

int main(int argc, char **argv)
{
  parseCmdParameter(argc, argv);
  printf("args:\n\tserver_addr:%s\n\tserver_port:%d\n\tserver_proxy_hostname:%s\n\tserver_proxy_port:%d\n\tnum_threads:%d\n\n",
          inet_ntoa(server_addr), server_port, server_proxy_hostname, server_proxy_port, num_threads);
  serve();
  return 0;
}