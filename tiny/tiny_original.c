/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // request line과 request header 읽기
  Rio_readinitb(&rio, fd);

  // request line 한 줄 읽기
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  // 요청헤더 다 출력
  printf("%s", buf);

  // buf는 "%s %s %s" 형태이고, 각각의 문자열은 method, uri, version에 넣는다.
  sscanf(buf, "%s %s %s", method, uri, version);

  // method와 "GET"를 대소문자 구분없이 비교해서 0이면 같은거, 0 아니면 다른거
  if (strcasecmp(method, "GET")) {
    // 다르면 아직 구현 안된 method라서 오류메시지 출력하고 리턴.
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // 나머지 request header는 읽어버리고 무시함.
  read_requesthdrs(&rio);

  // GET 요청에서 URI를 파싱해서 filename, cgiargs에 넣어줌
  is_static = parse_uri(uri, filename, cgiargs);
  
  // 서버 컴퓨터에 filename이 있으면 stat 호출시 0 리턴하고, sbuf에 해당 파일의 메타데이터 정보가 담김.
  if (stat(filename, &sbuf) < 0) {
    // 0 리턴 안되면 에러메시지 + 리턴.
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  } 

  // 파일 있으면 이제 정적 컨텐츠인지 확인
  if (is_static) {
    // 일반적인 파일이 아니거나 읽기권한이 없으면 에러
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }

  // 동적 컨텐츠인 경우
  else {
    if (!S_ISREG(sbuf.st_mode) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
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

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  
  // 일단 나머지 요청헤더중 첫번째꺼 하나 읽고
  Rio_readlineb(rp, buf, MAXLINE);
  // 이게 요청헤더 끝이 아니면 while 시작
  while (strcmp(buf, "\r\n")) {
    // 일단 한줄 읽고
    Rio_readlineb(rp, buf, MAXLINE);
    // 출력하기
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // uri에 "cgi-bin"이 없으면 정적 컨텐츠
  if (!strstr(uri, "cgi-bin")) {
    // 정적 컨텐츠 이름 담는곳 비워주기
    strcpy(cgiargs, "");
    // 파일 이름에 일단 현재 디렉토리를 나타내는 . 넣어주고
    strcpy(filename, ".");
    // 파일 이름을 뒤에 붙인다.
    strcat(filename, uri);
    
    // 근데 uri 맨 마지막이 /로 끝나면 그냥 uri = /인거라서 home.html 붙여주기
    if (uri[strlen(uri) - 1] == '/') {
      strcat(filename, "home.html");
    }
    return 1;
  }

  // 동적 컨텐츠
  else {
    // 우선 uri 내에서 ?의 인덱스(포인터)를 찾음
    ptr = index(uri, '?');

    // 찾았으면 NULL이 아님
    if (ptr) {
      // cgiargs 포인터를 한칸씩 움직이면서 복사하다가 '\0' 만나면 끝
      strcpy(cgiargs, ptr + 1);
      // '\0' 만나면 strcpy 중단되므로, 이제 ptr위치에 '\0'를 넣어줌.
      *ptr = '\0';
    }
    
    // 못 찾았으면 ?가 없음 -> 전달할 인자가 없음
    else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  // source fd
  int srcfd;

  // 파일 메모리에 올렸을 때 시작 위치, 파일 타입, 버퍼
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  
  // 응답 헤더를 클라이언트에게 보내기
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  // 응답 바디를 클라이언트에게 보내기

  // 일단 filename 이름을 가지는 파일을 read-only로 열기
  srcfd = Open(filename, O_RDONLY, 0);

  // filesize만큼 메모리 할당해서 읽기전용 private read-only data에 srcfd 파일을 올리고, 시작위치는 srcp
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);

  // 파일 식별자 닫고
  Close(srcfd);

  // 파일은 클라이언트에게 복사
  Rio_writen(fd, srcp, filesize);

  // 복사 끝나면 메모리에서 파일할당 해제
  Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  }
  else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  }
  else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  }
  else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  }
  else {
    strcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL };

  // HTTP 응답의 첫번째 부분을 리턴함
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스는 fork 반환값 = 0
  if (Fork() == 0) {
    // 환경변수 설정해서 인자 넘겨주기
    setenv("QUERY_STRING", cgiargs, 1);
    // 출력되는 fd를 fd로 바꿔주기(redirect stdout to client)
    // STDOUT(표준출력)을 fd로 재지정
    Dup2(fd, STDOUT_FILENO);
    // CGI 프로그램 실행, environ은 전역변수로, 환경변수 정보를 가리키는 문자열 배열(extern cahr **environ)
    Execve(filename, emptylist, environ);
  }
  // 부모 프로세스가 자식 프로세스를 reap 시킴.
  Wait(NULL);
}