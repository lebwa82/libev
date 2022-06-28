#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ev.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t my_mutex;

int status=0;
char buffer[1024];
int client_sd;


int read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void StrRev();
void *myThreadFun();
int accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);




void *myThreadFun()
{
    struct ev_loop *loop = ev_loop_new(0);
    struct ev_io w_read;
    ev_io_init(&w_read, read_cb, client_sd, EV_READ);
    ev_io_start(loop, &w_read);
    while(1)
    {
        ev_loop(loop, 0);
    }
}

int read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    pthread_mutex_lock(&my_mutex);
    ssize_t r = recv(watcher->fd, buffer, sizeof(buffer), MSG_NOSIGNAL);
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
        pthread_mutex_unlock(&my_mutex);
        raise(SIGUSR1);//послать сигнал в главный поток
        while(status!=1)//подождать обработки
        {
            //sleep(0.01);
        }
        status = 0;
        pthread_mutex_lock(&my_mutex);
        send(watcher->fd, buffer, r, 0);
        for(int i=0; i<r; i++)
        {
            buffer[i] = '\0';
        }
        
    }
    pthread_mutex_unlock(&my_mutex);
}


int accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    client_sd = accept(watcher->fd, 0, 0);
    pthread_t thread;
	pthread_create(&thread, NULL, myThreadFun, NULL);
}

void StrRev()
{
    pthread_mutex_lock(&my_mutex);
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
    status = 1;
    pthread_mutex_unlock(&my_mutex);
}


int main(int argc, char **argv)
{
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
    ev_signal_init(&w_signal, StrRev, SIGUSR1);
    ev_signal_start(loop, &w_signal);

    while(1)
    {
        ev_loop(loop, 0);
    }
    return 0;

}