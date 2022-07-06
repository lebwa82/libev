#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ev.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/queue.h> 

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <resolv.h>
#include <sys/uio.h>

static pthread_mutex_t my_mutex;

struct entry {
    char buffer[1024];
    int client_fd;
    STAILQ_ENTRY(entry) entries;        
};

STAILQ_HEAD(stailhead, entry);
static struct stailhead head_read; 
static struct stailhead head_send; 

static struct ev_loop* main_loop;
static struct ev_loop* thread_loop;
static struct ev_async w_main_async;
static struct ev_async w_thread_async;




void StrRev()
{
    pthread_mutex_lock(&my_mutex);
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

    STAILQ_INSERT_TAIL(&head_send, a, entries);

    pthread_mutex_unlock(&my_mutex);

    ev_async_send(main_loop, &w_main_async);


}

void *myThreadFun()
{
    thread_loop = ev_loop_new(0);
    //переписываем через асинхронный ватчер
    ev_async_init(&w_thread_async, StrRev);
    ev_async_start(thread_loop, &w_thread_async);
    ev_loop(thread_loop, 0);
    return NULL;
}



void read_cb(struct ev_loop *main_loop, struct ev_io *watcher, int revents)
{
    struct entry *p = (struct entry *)malloc(sizeof(struct entry));
    if(p == NULL)
    {
        printf("error in malloc\n");
        return;
    }
    p->client_fd = watcher->fd;
    *(p->buffer) = '\0';
    ssize_t r = recv(watcher->fd, p->buffer, sizeof(p->buffer), MSG_NOSIGNAL);
    if(r<0)
    {
        printf("error in reading^ r<0 %d\n", watcher->fd);
        ev_io_stop(main_loop, watcher);
        free(watcher);
        free(p);
        return;
    }
    if(r==0)
    {
        printf("disconnected %d\n", watcher->fd);
        ev_io_stop(main_loop, watcher);
        free(watcher);
        free(p);
        return;
    }
    if(r>0)
    {
        pthread_mutex_lock(&my_mutex);

        STAILQ_INSERT_TAIL(&head_read, p, entries);
        pthread_mutex_unlock(&my_mutex);

        //послать сигнал в второй поток, что данные можно обрабатывать
        ev_async_send(thread_loop, &w_thread_async);
        return;
    }
}


void accept_cb(struct ev_loop *main_loop, struct ev_io *watcher, int revents)
{
    int client_fd = accept(watcher->fd, 0, 0);
    printf("New connection, allocated fd = %d\n", client_fd);
    struct ev_io *w_client = (struct ev_io *) malloc(sizeof(struct ev_io));
    if(w_client == NULL)
    {
        printf("error in accept_cb malloc\n");
        return;
    }
    ev_io_init(w_client, read_cb, client_fd, EV_READ);
    ev_io_start(main_loop, w_client);
}

void send_func()
{
    pthread_mutex_lock(&my_mutex);
    struct entry *a = STAILQ_FIRST(&head_send);
    STAILQ_REMOVE_HEAD(&head_send, entries);
    pthread_mutex_unlock(&my_mutex);
    
    int bytes;
    struct iovec io[2];
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
        
    io[0].iov_base = a->buffer;
    io[0].iov_len = strlen(a->buffer);
    io[1].iov_base = "\n";
    io[1].iov_len = 1;

    msg.msg_iov = io;
    msg.msg_iovlen = 2;

    if ( (bytes = sendmsg(a->client_fd, &msg, 0)) < 0 )
        perror("sendmsg");
    
    free(a);
}

int main()
{
    //создаем очередь
    STAILQ_INIT(&head_read);
    STAILQ_INIT(&head_send);

    int m = pthread_mutex_init(&my_mutex, NULL);
    if(m!=0)
    {
        printf("error in creating mutex\n");
        return 1;
    }  

    pthread_t thread;
    int pth = pthread_create(&thread, NULL, myThreadFun, NULL);//создаем второй поток
    if(pth!=0)
    {
        printf("error in pthread_create");
        return 1;
    }

    printf("Enter tcp port\n");
    uint16_t port;
    scanf("%hd", &port);
    main_loop = ev_default_loop(0);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("error in calling socket");
        return 1;
    };

    struct sockaddr_in addr;
    memset(&addr, '0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int b = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (b < 0)
    {
        perror("bind call failed");
        return 1;
    }

    int l = listen(fd, 5);
    if(l < 0)
    {
        perror("listen call failed");
        return 1;
    }

    struct ev_io w_accept;
    ev_io_init(&w_accept, accept_cb, fd, EV_READ);
    ev_io_start(main_loop, &w_accept);
    
    ev_async_init(&w_main_async, send_func);
    ev_async_start(main_loop, &w_main_async);
    ev_loop(main_loop, 0);
    
    return 0;

}