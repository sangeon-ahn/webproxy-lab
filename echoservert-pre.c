#include "csapp.h"
#include "sbuf.h"
#define NTHREADS    4
#define SBUFSIZE    16

void echo_cnt(int connfd);
void *thread(void *vargp);

sbuf_t sbuf;

int main(int argc, char** argv)
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    // 포트 안줌
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // 리스닝 소켓 열고 반환
    listenfd = Open_listenfd(argv[1]);

    // sbuf 초기화하고 사이즈 초기화
    sbuf_init(&sbuf, SBUFSIZE);

    // NTHREADS 개수만큼 워커쓰레드 만들기
    for (i = 0; i < NTHREADS; i++) {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    // 클라이언트 요청 올때마다 받기
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
    }
}

void *thread(void *vargp)
{
    Pthread_detach(Pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        echo_cnt(connfd);
        Close(connfd);
    }
}