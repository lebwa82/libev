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
    struct ev_loop *loop = ev_loop_new(0);// создали свой loop
    struct ev_io w_read;
    printf("thread1\n");
    ev_io_init(&w_read, read_cb, client_sd, EV_READ);//проинициализировали
    printf("client_sd in thread1 = %d\n", client_sd);
    ev_io_start(loop, &w_read);
    while(1)
    {
        ev_loop(loop, 0); //одноразово запустили
    }
    
    printf("thread2\n");
    
    //pthread_mutex_lock(&my_mutex);
    //pthread_mutex_unlock(&my_mutex);
}

int read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    //char buffer[1024];
   // printf("read_cb1\n");
    pthread_mutex_lock(&my_mutex);
    ssize_t r = recv(watcher->fd, buffer, sizeof(buffer), MSG_NOSIGNAL);
    printf("r = %d\n", r);
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
        raise(SIGUSR1);
        printf("status = %d\n", status);
        while(status!=2)
        {
            //sleep(1);
            //printf("status = %d\n", status);
        }
        status = 0;//вот есть такое ощущение
        pthread_mutex_lock(&my_mutex);
        send(watcher->fd, buffer, r, MSG_NOSIGNAL);
    }
    pthread_mutex_unlock(&my_mutex);
}


int accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    client_sd = accept(watcher->fd, 0, 0);//accept я еще видимо делаю в главном потоке
    pthread_t thread;
    printf("client_sd = %d\n", client_sd);
	pthread_create(&thread, NULL, myThreadFun, NULL);
   // printf("main thread\n");
}

void StrRev()
{
  //  printf("strrev1\n");
    pthread_mutex_lock(&my_mutex);
    printf("buffer = %s\n",buffer);
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
    //raise(SIGUSR2);
    printf("buffer = %s\n",buffer);
    pthread_mutex_unlock(&my_mutex);
    status = 2;
}


int main(int argc, char **argv)
{
    printf("Hello world!\n");
    struct ev_loop *loop = ev_default_loop(0);

    int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;
    int b = bind(sd, (struct sockaddr *)&addr, sizeof(addr));
    if (b < 0)
    {
        printf("port already bound");
        return 0;
    }

    listen(sd, 5);
    struct ev_io w_accept;
    ev_io_init(&w_accept, accept_cb, sd, EV_READ);
    ev_io_start(loop, &w_accept);//добавить в loop
    printf("listen5\n");


    struct ev_signal w_signal;
    ev_signal_init(&w_signal, StrRev, SIGUSR1);
    ev_signal_start(loop, &w_signal);//добавить в loop

    while(1)
    {
        ev_loop(loop, 0);
    }
    return 0;

}