#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define SBUF_SIZE 50
#define NTHREADS  10
/*
  thread pool 방식으로 구현
*/
void doit(int fd);
void parse_uri(char *uri,char *hostname,char *path, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void blank_response(int fd, char *buf);
void* thread(void* vargp);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
sbuf_t sbuf;

int main(int argc, char ** argv) {
  int listenfd, connfd;
  pthread_t tid;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 쓰레드 NTHREADS개 생성
  for (int i = 0; i < NTHREADS; i++) {
    Pthread_create(&tid, NULL, thread, NULL);
  }

  listenfd = Open_listenfd(argv[1]);
  sbuf_init(&sbuf, SBUF_SIZE);

  while (1) {
    clientlen = sizeof(clientaddr);

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    sbuf_insert(&sbuf, connfd); // 메인쓰레드의 역할은 단지 connfd를 sbuf에 넣어주는 것
  }
  return 0;
}

void* thread(void* vargp)
{
  int connfd = sbuf_remove(&sbuf); // sbuf에서 connfd 하나 빼옴. sbuf에 connfd 없으면 블락
  doit(connfd); // 요청 처리
  Close(connfd); // 요청 처리햇으면 이제 fd 닫아주기
  // 
}
void doit(int fd)
{
  int is_static;
  int clientfd;
  struct stat sbuf;

  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], host[MAXLINE];
  char header[MAXLINE];
  char port[MAXLINE];
  rio_t rio, rio2;

  Rio_readinitb(&rio, fd);
  // 첫줄 request line 받기
  // GET http://google.com/favicon.ico HTTP/1.1
  Rio_readlineb(&rio, buf, MAXLINE);
  
  // method, uri, version으로 분리하기
  sscanf(buf, "%s %s %s", method, uri, version);
  // method: GET, uri: http://google.com/favicon.ico, version: HTTP/1.1
  // uri에서 filename 추출하기

  // printf("%s", buf);
  if (strcasecmp(method, "GET")) {
    // 다르면 아직 구현 안된 method라서 오류메시지 출력하고 리턴.
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  parse_uri(uri, host, filename, port);

  /*---브라우저 요청라인 + 요청헤더 저장---*/
  // 요청라인을 헤더에 저장
  sprintf(header, "GET %s HTTP/1.0\r\n", filename);
  sprintf(header, "%sUser-Agent: %s", header, user_agent_hdr);
  sprintf(header, "%sConnection: close\r\n", header);
  sprintf(header, "%sProxy-Connection: close\r\n", header);

  // 브라우저측 요청헤더 정보 저장하기(일단 다 받기)
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(&rio, buf, MAXLINE);

    if (strstr(buf, "User-Agent:")) {
      continue;
    }

    else if (strstr(buf, "Connection:")) {
      continue;
    }

    else if (strstr(buf, "Proxy-Connection:")) {
      continue;
    }

    else {
      sprintf(header, "%s%s", header, buf);
    }
  }


  printf("Request-header:\n");
  printf("%s", header);

  /*---프록시서버와 웹서버 연결---*/

  // 서버의 호스트와 포트번호(80) 넣어서 연결
  printf("host: %s\n", host);
  printf("port: %s\n", port);
  clientfd = Open_clientfd(host, port);
  Rio_readinitb(&rio2, clientfd);

  // 서버에 요청헤더 전송
  Rio_writen(clientfd, header, strlen(header));

  // blank_response(fd, buf);
  // 응답 받기. 언제까지? -> 응답헤더의 Content-length에 적힌 바이트만큼만
  
  /*---응답헤더 받아서 저장---*/

  Rio_readlineb(&rio2, buf, MAXLINE);
  sprintf(header, buf);

  // 응답헤더 계속 받는 부분
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(&rio2, buf, MAXLINE);
    sprintf(header, "%s%s", header, buf);
  }

  // 일단 응답헤더 출력해보기
  printf("response-header:\n");
  printf("%s", header);

  // 응답 헤더 브라우저에 보내기
  Rio_writen(fd, header, strlen(header));

  // 응답 바디 읽기
  size_t n;
  while ((n = Rio_readlineb(&rio2, buf, MAXLINE)) != 0) {
    printf("%d바이트 받음\n", n);
    printf("%s\n", buf);
    Rio_writen(fd, buf, n);
  }
  // 다했으면 프록시-웹서버간 연결 끊기
  Close(clientfd);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // HTTP response body 만들기
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // HTTP response 출력
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  // 우선 request line 보내주고
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  // content-type 보내주고
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  // content-length 보내주고
  Rio_writen(fd, buf, strlen(buf));
  // response body 보내준다.
  Rio_writen(fd, body, strlen(body));
}

void blank_response(int fd, char *buf)
{
  strcpy(buf, "HTTP/1.0 200 OK\r\n\r\n");
  Rio_writen(fd, buf, strlen(buf));
  return;
}

void parse_uri(char *uri,char *hostname,char *filename,char *port)
{
  // uri: "http://google.com:12345/favicon.ico"
  // uri(port 없을 때): "http://google.com/favicon.ico"
  
  // // 찾기
  char *p1 = strstr(uri, "//");
  p1 += 2;

  // / 찾기
  char *p2 = strstr(p1, "/");

  // 포트번호 써있나 보기, : d없으면 NULL 반환
  char *p3 = strstr(p1, ":");  
  
  // 포트번호 있을 때,
  if (p3 != NULL) {
    // 포트번호 복사
    strncpy(port, p3 + 1, p2 - p3 - 1);
    // 호스트 설정
    strncpy(hostname, p1, p3 - p1);
  }

  // 없을 때,
  else {
    strcpy(port, "80");
    strncpy(hostname, p1, p2 - p1);
  }
  // printf("%s", port);
  // filename 복사
  strcpy(filename, p2);
}