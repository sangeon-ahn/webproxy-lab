#include "csapp.h"

int main(int argc, char **argv) {
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio; // rio_t 구조체
    
    // 터미널 입력이 3개가 아니면 오류
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = argv[2];

    // host와 port를 인자로 넣어서 서버의 소켓을 찾은 후, 해당 소켓과 connect까지 해서 소켓 반환
    // 모든 서버 소켓과 연결 실패하면 어차피 오류메시지 떠서 clientfd -1인지 체크 안해줘도 됨.
    clientfd = open_clientfd(host, port);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf)); // buf에 담긴 문자열을 clientfd에 쓰는것
        Rio_readlineb(&rio, buf, MAXLINE); // 
        Fputs(buf, stdout);
        fflush(stdout);
    }

    Close(clientfd);
    exit(0);
}