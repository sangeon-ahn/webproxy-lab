#include "csapp.h"
void echo(int connfd);

void sigchld_handler(int sig)
{
    while (waitpid(-1, 0, WNOHANG) > 0) {
        ;
    }
    return;
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGCHLD, sigchld_handler);
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        // 제네릭 소켓주소 구조체인 sockaddr_storage 사이즈만큼 만듬.
        clientlen = sizeof(struct sockaddr_storage);
        // 일단 연결될 시 connected descriptor 생성
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        
        // 자식 프로세스인 경우.
        if (Fork() == 0) {
            // 부모 프로세스로부터 listening descriptor 복사되니까 삭제(자식은 필요없음)
            Close(listenfd);
            echo(connfd); // 자식 프로세스에서 echo 로직 수행
            Close(connfd); // echo 끝냈으니까 이제 connfd 닫기
            exit(0); // 프로세스 종료
        }
        // 부모 프로세스에서 생성된 file descriptor는 종료. 파일 테이블 엔트리에서 참조카운트 -1 혹은 connfd socket을 닫았다고 이해해도 됨.
        Close(connfd);
    }
}
