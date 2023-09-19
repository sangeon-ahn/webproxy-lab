#include "csapp.h"

void *thread(void *vargp);

volatile long cnt = 0;

int main(int argc, char **argv)
{
    long niters; // n번 iter(반복)한다는 뜻
    pthread_t tid1, tid2;

    // 터미널로 넘겨준 인자 확인
    if (argc != 2) {
        printf("usage: %s <niters>\n", argv[0]);
        exit(0);
    }
    niters = atoi(argv[1]);

    // 쓰레드 만들고 끝날 때까지 기다리기
    Pthread_create(&tid1, NULL, thread, &niters);
    Pthread_create(&tid2, NULL, thread, &niters);
    Pthread_join(tid1, NULL);
    Pthread_join(tid2, NULL);

    // 결과 확인
    if (cnt != 2 * niters) {
        printf("BOOM! cnt=%ld\n", cnt);
    }
    else {
        printf("OK cnt=%ld\n", cnt);
    }
    exit(0);
}

void *thread(void *vargp)
{
    long i, niters = *((long *)vargp);

    for (int i = 0; i < niters; i++) {
        cnt++;
    }
    return NULL;
}