#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ev.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/queue.h> 

pthread_mutex_t my_mutex;

struct entry {
    int data;
    char buffer[1024];
    int client_sd;
    STAILQ_ENTRY(entry) entries;        
};

STAILQ_HEAD(stailhead, entry);
struct stailhead head_read; 
struct stailhead head_send; 

struct ev_loop* main_loop;
struct ev_loop* thread_loop;
struct ev_async w_main_async;
struct ev_async w_thread_async;


void StrRev()
{
    pthread_mutex_lock(&my_mutex);
    //Queue* a = pop_from_Queue(head_Queue_read_el, tail_Queue_read_el);
    struct entry *a = STAILQ_FIRST(&head_read);
    STAILQ_REMOVE_HEAD(&head_read, entries);

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
    pthread_mutex_lock(&my_mutex);//операции со стеком надо защитить, тк с ним работают два процесса одновременноа с буфером может работать только один
    //add_to_Queue(head_Queue_send_el, tail_Queue_send_el ,a);
    STAILQ_INSERT_TAIL(&head_send, a, entries);

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
    //Queue* p = create_Queue(watcher->fd);
    struct entry *p = (struct entry *)malloc(sizeof(struct entry));
    p->client_sd = watcher->fd;
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

        //add_to_Queue(head_Queue_read_el, tail_Queue_read_el ,p);
        STAILQ_INSERT_TAIL(&head_read, p, entries);
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
    struct entry *a = STAILQ_FIRST(&head_send);
    STAILQ_REMOVE_HEAD(&head_send, entries);     /* Deletion from the head */
    //Queue* a = pop_from_Queue(head_Queue_send_el, tail_Queue_send_el);
    pthread_mutex_unlock(&my_mutex);
    send(a->client_sd, a->buffer, strlen(a->buffer), 0);
    send(a->client_sd, "\n", 1 , 0);
    free(a);
}

int main(int argc, char **argv)
{
    //создаем очередь

    STAILQ_INIT(&head_read);
    STAILQ_INIT(&head_send);  

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