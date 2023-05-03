#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libhttp.h"

#define LIBHTTP_REQUEST_MAX_SIZE 8192

// 打印错误信息
void http_fatal_error(char *message)
{
  fprintf(stderr, "%s\n", message);
  exit(1);
}

struct http_request *http_request_parse(int fd)
{
  // printf("[http_request_parse]: Start to parse the request\n");
  struct http_request *request = (struct http_request *)malloc(sizeof(struct http_request));
  assert(request != NULL);

  char *read_buffer = (char *)malloc(LIBHTTP_REQUEST_MAX_SIZE + 1);
  assert(read_buffer != NULL);

  // printf("[http_request_parse]: Start to read the request\n");
  int bytes_read = read(fd, read_buffer, LIBHTTP_REQUEST_MAX_SIZE);
  read_buffer[bytes_read] = '\0'; /* Always null-terminate. */
  // printf("[http_request_parse]: Read finished.\n");
  // printf("[http_request_parse]: %s\n", read_buffer);

  char *read_start, *read_end;
  size_t read_size;

  do
  {
    /* Read in the HTTP method: "[A-Z]*" */
    read_start = read_end = read_buffer;
    while (*read_end >= 'A' && *read_end <= 'Z')
      read_end++;
    read_size = read_end - read_start;
    if (read_size == 0)
      break;
    request->method = (char *)malloc(read_size + 1);
    memcpy(request->method, read_start, read_size);
    request->method[read_size] = '\0';
    // printf("[http_request-parse]: Method: %s\n", request->method);

    /* Read in a space character. */
    read_start = read_end;
    if (*read_end != ' ')
      break;
    read_end++;

    /* Read in the path: "[^ \n]*" */
    read_start = read_end;
    while (*read_end != '\0' && *read_end != ' ' && *read_end != '\n')
      read_end++;
    read_size = read_end - read_start;
    if (read_size == 0)
      break;
    request->path = (char *)malloc(read_size + 1);
    memcpy(request->path, read_start, read_size);
    request->path[read_size] = '\0';

    if (strcmp(request->method, "POST") == 0)
    {
      // 读取上传的数据
      regex_t regex;
      int reti;
      // NOTE:match需要两个regmatch_t存储比较结果，第一个为整个句子，第二个开始为匹配的组
      regmatch_t pmatch[2];
      char *pattern_ContentLength = "\\{\"id\":([0-9]+),\"name\":\"([a-zA-Z0-9]+)\"\\}|id=([0-9]+)&name=([a-zA-Z0-9]+)|\\{\"name\":\"([a-zA-Z0-9]+)\",\"id\":([0-9]+)\\}|name=([a-zA-Z0-9]+)&id=([0-9]+)";

      reti = regcomp(&regex, pattern_ContentLength, REG_EXTENDED);
      reti = regexec(&regex, read_buffer, 2, pmatch, 0);

      if(reti == 0)
      {
        int len = pmatch[0].rm_eo - pmatch[0].rm_so;
        request->data = (char*)malloc(len + 1);
        memcpy(request->data, &read_buffer[pmatch[0].rm_so], len);
        request->data[len] = '\0';
        printf("request->data = %s\n", request->data);
      }
      else
      {
        request->data = NULL;
      }

      // 使用正则表达式匹配type，并转储数据
      regex_t regex_type;
      char *pattern_ContentType = "Content-Type: ([^\\\n]+)";

      reti = regcomp(&regex_type, pattern_ContentType, REG_EXTENDED);
      reti = regexec(&regex_type, read_buffer, 2, pmatch, 0);

      if(reti == 0)
      {
        int len = pmatch[1].rm_eo - pmatch[1].rm_so;
        request->type = (char *)malloc(len + 1);
        memcpy(request->type, &read_buffer[pmatch[1].rm_so], len);
        request->type[len] = '\0';
        printf("request->type = %s\n", request->type);
      }
      else
      {
        request->type = NULL;
      }
    }

    free(read_buffer);
    return request;
  } while (0);

  /* An error occurred. */
  free(request);
  free(read_buffer);
  return NULL;
}

const char *http_get_response_message(int status_code)
{
  switch (status_code)
  {
  case 100:
    return "Continue";
  case 200:
    return "OK";
  case 301:
    return "Moved Permanently";
  case 302:
    return "Found";
  case 304:
    return "Not Modified";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  default:
    return "Internal Server Error";
  }
}

void http_start_response(int fd, int status_code)
{
  dprintf(fd, "HTTP/1.1 %d %s\r\n", status_code,
          http_get_response_message(status_code));
}

void http_send_header(int fd, char *key, char *value)
{
  dprintf(fd, "%s: %s\r\n", key, value);
}

void http_end_headers(int fd)
{
  dprintf(fd, "\r\n");
}

void http_send_string(int fd, char *data)
{
  http_send_data(fd, data, strlen(data));
}

void http_send_data(int fd, char *data, size_t size)
{
  ssize_t bytes_sent;
  while (size > 0)
  {
    bytes_sent = write(fd, data, size);
    assert(bytes_sent >= 0);
    if (bytes_sent < 0)
      return;
    size -= bytes_sent;
    data += bytes_sent;
  }
}

char *http_get_mime_type(char *file_name)
{
  char *file_extension = strrchr(file_name, '.');
  if (file_extension == NULL)
  {
    return "text/plain";
  }

  if (strcmp(file_extension, ".html") == 0 || strcmp(file_extension, ".htm") == 0)
  {
    return "text/html";
  }
  else if (strcmp(file_extension, ".jpg") == 0 || strcmp(file_extension, ".jpeg") == 0)
  {
    return "image/jpeg";
  }
  else if (strcmp(file_extension, ".png") == 0)
  {
    return "image/png";
  }
  else if (strcmp(file_extension, ".css") == 0)
  {
    return "text/css";
  }
  else if (strcmp(file_extension, ".js") == 0)
  {
    return "application/javascript";
  }
  else if (strcmp(file_extension, ".pdf") == 0)
  {
    return "application/pdf";
  }
  else if (strcmp(file_extension, ".json") == 0)
  {
    return "application/json";
  }
  else
  {
    return "text/plain";
  }
}