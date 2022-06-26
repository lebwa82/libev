#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ev.h>


void StrRev(char *S)
{
     int i,j,l;
     char t;
     l=strlen(S);
     i=0;
     j=l-1;
     while (i<j)
     {
        t=S[i];
        S[i]=S[j];
        S[j]=t;
        i++;j--;
      }
}

int read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    char buffer[1024];
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
        send(watcher->fd, buffer, r, MSG_NOSIGNAL);
    }
}


int accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    int client_sd = accept(watcher->fd, 0, 0);//accept я еще видимо делаю в главном потоке

    struct ev_io *w_client = (struct ev_io *) malloc(sizeof(struct ev_io));//а читаю уже видимо в другом потоке
    ev_io_init(w_client, read_cb, client_sd, EV_READ);
    ev_io_start(loop, w_client);
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
    bind(sd, (struct sockaddr *)&addr, sizeof(addr));

    listen(sd, SOMAXCONN);

    struct ev_io w_accept;
    ev_io_init(&w_accept, accept_cb, sd, EV_READ);
    ev_io_start(loop, &w_accept);

    while(1)
    {
        ev_loop(loop, 0);
    }
    return 0;

}