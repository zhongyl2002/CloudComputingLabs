#include "myGet.h"

char server_static_directory[10] = "./static";
char server_data_directory[10] = "./data";

char* firstChar(char* src, char target)
{
  int len_ = strlen(src);
  int i = 0;
  while (i < len_)
  {
    if(src[i] == target)
      return (src + i);
    i ++;
  }
  return NULL;
}

int countSubstring(char* src, char* target) {
    int count = 0;
    int len = strlen(target);
    char* p = src;

    while ((p = strstr(p, target))) {
        count++;
        p += len;
    }

    return count;
}

// 该函数一定记得free返回的指针
char* read_file(FILE *fp)
{
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  rewind(fp);

  char *read_buffer = (char *)malloc(size + 1);
  if (!read_buffer)
  {
    fprintf(stderr, "%s\n", "Malloc Failed");
  }
  int bytes_read = fread(read_buffer, 1, size, fp);
  read_buffer[bytes_read] = '\0';
  
  return read_buffer;
}

void write_file(char *file_path, FILE *fp, int fd, int sCode)
{
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  rewind(fp);

  char *read_buffer = (char *)malloc(size + 1);
  if (!read_buffer)
  {
    fprintf(stderr, "%s\n", "Malloc Failed");
  }
  int bytes_read = fread(read_buffer, 1, size, fp);
  printf("[write_file]: bytes_read: %d\n", bytes_read);
  read_buffer[bytes_read] = '\0';
  char sz_string[20];
  snprintf(sz_string, sizeof(sz_string), "%d", bytes_read);

  http_start_response(fd, sCode);
  http_send_header(fd, "Content-Type", http_get_mime_type(file_path));
  http_send_header(fd, "Content-Length", sz_string);
  http_end_headers(fd);
  http_send_data(fd, read_buffer, bytes_read);

  fclose(fp);
  free(read_buffer);
}

void sendSpecialFile(char* server_files_directory, char* specialFile, int fd, int sCode)
{
  char* file_path = (char*)malloc(strlen(server_files_directory) + 16);
  strncpy(file_path, server_files_directory, strlen(server_files_directory));
  file_path[strlen(server_files_directory)] = '\0';
  strcat(file_path, specialFile);
  // printf("file_path = %s\n", file_path);
  FILE* file = fopen(file_path, "rb+");
  assert(file != NULL);
  if(file)
    write_file(file_path, file, fd, sCode);
  free(file_path);
}

void handle_web_query(int fd, char* cmd, struct http_request* req)
{
  if(strcmp(cmd, "/check") == 0)
  {
    char path[] = "./data/data.txt";
    FILE* fp = fopen(path, "rb+");
    assert(fp != NULL);
    write_file(path, fp, fd, 200);
  }
  else if(strcmp(cmd, "/list") == 0)
  {
    char path[] = "./data/data.json";
    FILE* fp = fopen(path, "rb+");
    assert(fp != NULL);
    write_file(path, fp, fd, 200);
  }
  else if(strcmp(cmd, "/echo") == 0)
  {
    printf("echo - get data : %s\n", req->data);
    if(req->data != NULL)
    {
      http_start_response(fd, 200);
      http_send_header(fd, "Content-Type", "application/x-www-form-urlencoded");
      char tmp[10];
      sprintf(tmp, "%d", (int)strlen(req->data));
      http_send_header(fd, "Content-Length", tmp);
      http_end_headers(fd);
      http_send_data(fd, req->data, strlen(req->data));
    }
    else
    {
      FILE* fp = fopen("./data/error.txt", "rb+");
      write_file("./data/error.txt", fp, fd, 404);
    }
  }
  else if(strcmp(cmd, "/upload") == 0)
  {
    printf("upload - get data : %s\n", req->data);
    if(req->data != NULL)
    {
      http_start_response(fd, 200);
      http_send_header(fd, "Content-Type", "application/json");
      char tmp[10];
      sprintf(tmp, "%d", (int)strlen(req->data));
      http_send_header(fd, "Content-Length", tmp);
      http_end_headers(fd);
      http_send_data(fd, req->data, strlen(req->data));
      return;
    }
    else
    {
      FILE* fp = fopen("./data/error.txt", "rb+");
      write_file("./data/error.txt", fp, fd, 404);
    }
  }
  else
  {
    regex_t regex;
    int reti;
    regmatch_t pmatch[3];
    // NOTE:表示一个问号需要两个\\，因为\在c语言中有转义功能，需要另外一个\将\转义为\.
    char* pattern = "/search\\?id=([0-9]+)&name=([a-zA-Z0-9]+)";

    reti = regcomp(&regex, pattern, REG_EXTENDED);
    reti = regexec(&regex, cmd, 3, pmatch, 0);

    if (reti == 0) {
      char *id, *name;
      int id_len, name_len;
      // extract matched strings
      id_len = pmatch[1].rm_eo - pmatch[1].rm_so;
      id = malloc(id_len + 1);
      memcpy(id, &cmd[pmatch[1].rm_so], id_len);
      id[id_len] = '\0';

      name_len = pmatch[2].rm_eo - pmatch[2].rm_so;
      name = malloc(name_len + 1);
      memcpy(name, &cmd[pmatch[2].rm_so], name_len);
      name[name_len] = '\0';

      FILE* fp = fopen("./data/data.json", "rb+");
      char* srcCh = read_file(fp);
      char* target = (char*)malloc(64);
      sprintf(target, "{\"id\":%s,\"name\":\"%s\"}", id, name);
      target[id_len + name_len + 17] = '\0';
      int cnt = countSubstring(srcCh, target);
      free(srcCh);

      // cnt == 0 —— 输出/data/not_found.json文件内容
      if(cnt == 0)
      {
        FILE* fp = fopen("./data/not_found.json", "rb+");
        write_file("./data/not_found.json", fp, fd, 404);
      }
      else
      {
        http_start_response(fd, 200);
        http_send_header(fd, "Content-Type", http_get_mime_type("./data/not_found.json"));
        char tmpch[5];
        sprintf(tmpch, "%d", (int)(2 + strlen(target) * cnt + (cnt - 1)));
        http_send_header(fd, "Content-Length", tmpch);
        http_end_headers(fd);
        http_send_data(fd, "[", 1);
        while (cnt --)
        {
          if(cnt == 0)
          {
            http_send_data(fd, target, strlen(target));
            http_send_data(fd, "]", 1);
          }
          else
          {
            http_send_data(fd, target, strlen(target));
            http_send_data(fd, ",", 1);
          }
        }
      }
      
      // free memory
      free(id);
      free(name);
      free(target);
    }
    else
    {
      sendSpecialFile(server_static_directory, "/404.html", fd, 404);
    }
    regfree(&regex);
  }
}

// 传入参数fd为连接套接字
void handle_static_file(int fd)
{
  printf("in handle_files_request\n");
  struct http_request *request = http_request_parse(fd);
  printf("parse request succeed->path:%s, method:%s\n", request->path, request->method);
  if (!request)
  {
    fprintf(stderr, "%s\n", "Request parse Failed");
    return;
  }
  if(strcmp(request->method, "GET") != 0 && strcmp(request->method, "POST") != 0)
  {
    sendSpecialFile(server_static_directory, "/501.html", fd, 501);
  }
  char* first_ = firstChar(request->path, '/');
  char* second_ = firstChar(first_ + 1, '/');
  if(first_ != NULL && second_ != NULL)
  {
    char* api = (char*) malloc(6);
    memcpy(api, request->path, (second_ - first_));
    api[(second_ - first_)] = '\0';
    printf("first:%s second:%s api:%s res:%d\n", first_, second_, api, strcmp(api, "/api"));
    if(strcmp(api, "/api") == 0)
    {
      handle_web_query(fd, second_, request);
      free(api);
      return;
    }
    free(api);
  }
  char *file_path = (char *)malloc(strlen(server_static_directory) + strlen(request->path) + 1);
  strncpy(file_path, server_static_directory, strlen(server_static_directory));
  file_path[strlen(server_static_directory)] = '\0';
  strcat(file_path, request->path);
  printf("Request path: %s\n", request->path);

  FILE *fp = fopen(file_path, "rb+");
  if(strcmp(request->path, "/404.html") == 0)   // 特殊文件
  {
    sendSpecialFile(server_static_directory, "/404.html", fd, 404);
  }
  else if(strcmp(request->path, "/501.html") == 0)  // 特殊文件
  {
    sendSpecialFile(server_static_directory, "/501.html", fd, 501);
  }
  else if (fp)                  // 文件
  {
    write_file(file_path, fp, fd, 200);
  }
  else if (errno == EISDIR)     // 目录
  {
    char *index_path = (char *)malloc(50);
    strcpy(index_path, file_path);
    strcat(index_path, "/index.html");
    FILE *file = fopen(index_path, "rb+");
    if (file)
    {
      write_file(index_path, file, fd, 200);
    }
    else
    {
      sendSpecialFile(server_static_directory, "/404.html", fd, 404);
    }
    free(index_path);
  }
  else                          // 不存在
  {
    sendSpecialFile(server_static_directory, "/404.html", fd, 404);
  }
  free(file_path);
}