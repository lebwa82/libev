#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ev.h>
#include <unistd.h>
#include <pthread.h>



typedef struct Stack
{
    char buffer[1024];
    int client_sd;
    struct Stack *next;
}Stack;

Stack* add_to_Stack(Stack *head, Stack* a)
{
    Stack * p=head; 
    while(p->next !=NULL)
    {
        p = p->next;
    }
    p->next = a;
    return a;
}

Stack* pop_from_Stack(Stack *head)
{
    Stack * p=head;
    if(p->next ==NULL)
    {
        return NULL;//нечего удалять
    } 
    while(p->next->next !=NULL)
    {
        p = p->next;
    }
    Stack * a=p->next;
    p->next = NULL;
    return a; 
}

Stack* create_stack(char* str, int client_sd)
{
    Stack* a = (Stack*)malloc(sizeof(Stack));
    strcpy(a->buffer, str);
    a->client_sd = client_sd;
    a->next = NULL;
    return a;
}

Stack* zero_Stack_read_el;//нулевой элемент стека для чтения сообщения
Stack* zero_Stack_send_el;//нулевой элемент стека для посылки сообщения


void StrRev()
{
    Stack* a = pop_from_Stack(zero_Stack_read_el);
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
    add_to_Stack(zero_Stack_send_el, a);
    raise(SIGUSR2);//послать сигнал в главный поток о том, что можно отрпавлять

}

void *myThreadFun()
{
    struct ev_loop *loop = ev_loop_new(0);
    struct ev_signal w_signal;
    ev_signal_init(&w_signal, StrRev, SIGUSR1);
    ev_signal_start(loop, &w_signal);
    while(1)
    {
        ev_loop(loop, 0);
    }
}




int read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    //char buffer[1024];
    Stack* p = create_stack("\0", watcher->fd);
    ssize_t r = recv(watcher->fd, p->buffer, sizeof(p->buffer), MSG_NOSIGNAL);
    if(r<0)
    {
        return 0;
    }
    if(r==0)
    {
        ev_io_stop(loop, watcher);
        free(watcher);
        return 0;
    }
    if(r>0)
    {
        add_to_Stack(zero_Stack_read_el, p);
        raise(SIGUSR1);//послать сигнал в второй поток, что данные можно обрабатывать
    }
}


int accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    int client_sd = accept(watcher->fd, 0, 0);
    printf("New connection, allocated sd = %d\n", client_sd);
    struct ev_io *w_client = (struct ev_io *) malloc(sizeof(struct ev_io));
    ev_io_init(w_client, read_cb, client_sd, EV_READ);
    ev_io_start(loop, w_client);
}

int send_func()
{
    Stack* a = pop_from_Stack(zero_Stack_send_el);
    send(a->client_sd, a->buffer, strlen(a->buffer), 0);
    send(a->client_sd, "\n", 1 , 0);
}

int main(int argc, char **argv)
{
    zero_Stack_read_el = create_stack("\0", -1);
    zero_Stack_send_el = create_stack("\0", -1);
    pthread_t thread;
    pthread_create(&thread, NULL, myThreadFun, NULL);//создаем второй поток

    printf("Enter tcp port\n");
    int port;
    scanf("%d", &port);
    struct ev_loop *loop = ev_default_loop(0);

    int sd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int b = bind(sd, (struct sockaddr *)&addr, sizeof(addr));
    if (b < 0)
    {
        printf("port already bound\n");
        return 0;
    }

    listen(sd, 5);

    struct ev_io w_accept;
    ev_io_init(&w_accept, accept_cb, sd, EV_READ);
    ev_io_start(loop, &w_accept);

    struct ev_signal w_signal;
    ev_signal_init(&w_signal, send_func, SIGUSR2);//когда второй поток обработал - можно отправлять
    ev_signal_start(loop, &w_signal);

    while(1)
    {
        ev_loop(loop, 0);
    }
    return 0;

}