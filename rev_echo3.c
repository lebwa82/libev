#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ev.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t my_mutex;

typedef struct Queue
{
    char buffer[1024];
    int client_sd;
    struct Queue *next;
}Queue;

Queue* add_to_Queue(Queue *head, Queue *tail, Queue* a)
{
    tail->next = a;
    tail = a;
    return a;
}

Queue* pop_from_Queue(Queue *head, Queue* tail)//правда теперь это уже не стек, а очередь
{
    if(head->next ==NULL)
    {
        return NULL;//нечего удалять
    } 
    //пропишу более громоздко, чтобы лучше понимать логику
    if(head->next == tail)//всего один элемент
    {
        tail = head;
        Queue *a = head->next;
        head->next = NULL;
        return a;
    }
    else//элементов много - хвост спокоен
    {
        Queue *a = head->next;
        head->next = NULL;
        return a;
    }
}

Queue* create_Queue(int client_sd)
{
    Queue* a = (Queue*)malloc(sizeof(Queue));
    strcpy(a->buffer, "\0");
    a->client_sd = client_sd;
    a->next = NULL;
    return a;
}

Queue* head_Queue_read_el;//нулевой элемент стека для чтения сообщения
Queue* tail_Queue_read_el;
Queue* head_Queue_send_el;//нулевой элемент стека для посылки сообщения
Queue* tail_Queue_send_el;


struct ev_loop* main_loop;
struct ev_loop* thread_loop;
struct ev_async w_main_async;
struct ev_async w_thread_async;


void StrRev()
{
    pthread_mutex_lock(&my_mutex);
    Queue* a = pop_from_Queue(head_Queue_read_el, tail_Queue_read_el);
    pthread_mutex_unlock(&my_mutex);
    char *buffer = a->buffer;//поскольку это указатель он должен менять строку
    int i,j,l;
    char t;
    l=strlen(buffer);
    i=0;
    j=l-1;
    while (i<j)
    {
        t=buffer[i];
        buffer[i]=buffer[j];
        buffer[j]=t;
        i++;j--;
    }
    //тут добавить во второй стек
    //послать сигнал
    pthread_mutex_lock(&my_mutex);//операции со стеком надо защитить, тк с ним работают два процесса одновременно
    //а с буфером может работать только один
    add_to_Queue(head_Queue_send_el, tail_Queue_send_el ,a);
    pthread_mutex_unlock(&my_mutex);

    //raise(SIGUSR2);//послать сигнал в главный поток о том, что можно отрпавлять
    ev_async_send(main_loop, &w_main_async);


}

void *myThreadFun()
{
    thread_loop = ev_loop_new(0);
    //переписываем через асинхронный ватчер
    ev_async_init(&w_thread_async, StrRev);
    ev_async_start(thread_loop, &w_thread_async);
    ev_loop(thread_loop, 0);
}



int read_cb(struct ev_loop *main_loop, struct ev_io *watcher, int revents)
{
    Queue* p = create_Queue(watcher->fd);
    ssize_t r = recv(watcher->fd, p->buffer, sizeof(p->buffer), MSG_NOSIGNAL);
    if(r<0)
    {
        printf("error in %d\n", watcher->fd);
        ev_io_stop(main_loop, watcher);
        free(watcher);
        free(p);
        return 0;
    }
    if(r==0)
    {
        printf("disconnected %d\n", watcher->fd);
        ev_io_stop(main_loop, watcher);
        free(watcher);
        free(p);
        return 0;
    }
    if(r>0)
    {
        pthread_mutex_lock(&my_mutex);
        add_to_Queue(head_Queue_read_el, tail_Queue_read_el ,p);
        pthread_mutex_unlock(&my_mutex);
        //послать сигнал в второй поток, что данные можно обрабатывать
        ev_async_send(thread_loop, &w_thread_async);
    }
}


int accept_cb(struct ev_loop *main_loop, struct ev_io *watcher, int revents)
{
    int client_sd = accept(watcher->fd, 0, 0);
    printf("New connection, allocated sd = %d\n", client_sd);
    struct ev_io *w_client = (struct ev_io *) malloc(sizeof(struct ev_io));
    ev_io_init(w_client, read_cb, client_sd, EV_READ);
    ev_io_start(main_loop, w_client);
}

int send_func()
{
    pthread_mutex_lock(&my_mutex);
    Queue* a = pop_from_Queue(head_Queue_send_el, tail_Queue_send_el);
    pthread_mutex_unlock(&my_mutex);
    send(a->client_sd, a->buffer, strlen(a->buffer), 0);
    send(a->client_sd, "\n", 1 , 0);
    free(a);
}

int main(int argc, char **argv)
{
    head_Queue_read_el = create_Queue(-1);
    tail_Queue_read_el = head_Queue_read_el;
    head_Queue_send_el = create_Queue(-1);
    tail_Queue_send_el = head_Queue_send_el;

    pthread_t thread;
    pthread_create(&thread, NULL, myThreadFun, NULL);//создаем второй поток

    printf("Enter tcp port\n");
    int port;
    scanf("%d", &port);
    main_loop = ev_default_loop(0);

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("HH_ERROR: error in calling socket()");
        exit(1);
    };

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int b = bind(sd, (struct sockaddr *)&addr, sizeof(addr));
    if (b < 0)
    {
        perror("HH_ERROR: bind() call failed");
        return 0;
    }

    int l = listen(sd, 5);
    if(l < 0)
    {
        perror("HH_ERROR: listen() call failed");
        exit(1);
    }

    struct ev_io w_accept;
    ev_io_init(&w_accept, accept_cb, sd, EV_READ);
    ev_io_start(main_loop, &w_accept);
    
    ev_async_init(&w_main_async, send_func);
    ev_async_start(main_loop, &w_main_async);
    ev_loop(main_loop, 0);
    
    return 0;

}