#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define debug 0
#define NTHREADS 4
#define SBUFSIZE 16
typedef struct 
{
    char url[MAXLINE];
    char cache_obj[MAX_OBJECT_SIZE];
    time_t last_time;
} cache_block;
typedef struct 
{
    int cache_n;
    int cache_size;
    cache_block* cache;
    int readercnt;
    sem_t mutex;/*保护readercnt*/
    sem_t w;/*保护cache*/
} Cache;
Cache cache;
/*初始化cache*/
void cache_init(Cache *cache,int n){
    cache->cache= malloc(sizeof(cache_block)*n);
    cache->cache_size= n;
    cache->cache_n= -1;
    cache->readercnt= 0;
    sem_init(&cache->mutex,0,1);
    sem_init(&cache->w,0,1);
}

/*存入cache_body*/
void cache_put(Cache *cache,char* uri,char* body){
    P(&cache->w);
    /*cache未满*/
    if (cache->cache_n!=(cache->cache_size-1))
    {
        strcpy((cache->cache[cache->cache_n+1].cache_obj),body);
        strcpy((cache->cache[cache->cache_n+1].url),uri);
        cache->cache[cache->cache_n+1].last_time=time(NULL);
        cache->cache_n++;
        V(&cache->w);
        return;
    }
    /*cache已满*/
    else{
        int old_index=0;
        time_t old_time=cache->cache[0].last_time;
        for (size_t i = 0; i < cache->cache_size; i++)
        {
            if (cache->cache[i].last_time<old_time)
            {
                old_time=cache->cache[i].last_time;
                old_index=i;
            }
        }
        strcpy((cache->cache[old_index].cache_obj),body);/*Update body*/
        strcpy((cache->cache[old_index].url),uri);/*Update url*/
        cache->cache[old_index].last_time=time(NULL);/*Update time*/
        V(&cache->w);
        return;
    }
    
    
}

/*获取cache_body*/
int cache_get(Cache *cache,char* uri,char* body){
    P(&cache->mutex);
    cache->readercnt++;
    /*没有cache*/
    if (cache->cache_n==-1)
    {
        V(&cache->mutex);
        return 0;
    }
    /*遍历cache*/
    for (int i = 0; i <=cache->cache_n ; i++)
    {
        if (!strcmp(uri,cache->cache[i].url))
        {
            memcpy(body,cache->cache[i].cache_obj,MAX_OBJECT_SIZE);
            cache->cache[i].last_time=time(NULL);
            V(&cache->mutex);
            return 1;
        }
    }
    V(&cache->mutex);
    return 0;
}

/*deinit cache*/
void cache_deinit(Cache *cache){
    free(cache->cache);
}

typedef struct {
    int *buf;       /* Buffer array */
    int n;          /* Maximum number of slots */
    int front;      /* buf[(front+1)%n] is first item */
    int rear;       /* buf[rear%n] is last item */
    sem_t mutex;    /* Protects accesses to buf */
    sem_t slots;    /* Counts available slots */
    sem_t items;    /* Counts available items */
} sbuf_t;
sbuf_t sbuf;

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                           /* Wait for available slot */
    P(&sp->mutex);                           /* Lock the buffer */
    sp->buf[(++sp->rear) % (sp->n)] = item;  /* Insert the item */
    V(&sp->mutex);                           /* Unlock the buffer */
    V(&sp->items);                           /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                           /* Wait for available item */
    P(&sp->mutex);                           /* Lock the buffer */
    item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
    V(&sp->mutex);                           /* Unlock the buffer */
    V(&sp->slots);                           /* Announce available slot */
    return item;
}
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) ;
void request_and_doProxy(int fd, char* url, char* hostname,char* path,char* port,rio_t* rio_client);
void parse_uri(char *uri, char *hostname, char *path, char *port);
void* thread(void* vargp);
int main(int argc,char** argv)
{
    int listenfd, connfd,*connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }
    /*监听本地代理请求端口*/
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    cache_init(&cache,10);
    for (int i = 0; i < 4; i++)  /* Create worker threads */
        Pthread_create(&tid, NULL, thread, NULL);
    while (1) {
	clientlen = sizeof(clientaddr);
    /*连接本地要代理的client*/
    
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
    sbuf_insert(&sbuf, connfd);
    // connfdp=malloc(sizeof(int));
    // *connfdp=connfd;
    // Pthread_create(&tid,NULL,thread,connfdp);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    if(debug)
        printf("Accepted connection from (%s, %s)[Local]\n", hostname, port);
    }
    sbuf_deinit(&sbuf);
}



/*代替客户端向服务器发起请求*/
void doit(int fd){
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE], path[MAXLINE], port[MAXLINE];
    rio_t rio;
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    if(debug)
    printf("HEAD::FIREST_LINE:%s", buf);
    /*得到method、uri、version*/
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    parse_uri(uri,hostname,path,port);
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }
    if(debug)
    printf("HOST:%s",hostname);
    request_and_doProxy(fd,uri,hostname,path,port,&rio);
}

/*解析uri如http://www.cmu.edu:8080/hub/index.html*/
void parse_uri(char *uri, char *hostname, char *path, char *port) {
    char temp[MAXLINE];
    sscanf(uri,"http://%s",temp);
    strcpy(path,strchr(temp,'/'));
    //获取hostname(带端口,如果有的话)
    strncpy(hostname,temp,strlen(temp)-strlen(path));
    //输出请求路径：/hub/index.html
    if(debug)
    printf("PATH:%s", path);
    int tempPort=0;
    if( strchr(temp,':')!=0 ){
        sscanf(strchr(temp,':')+1,"%d",&tempPort);
        sprintf(port,"%d",tempPort);
    }else{
        sprintf(port,"80");
    }
    if(debug)
    printf("%s",port);
    int len = strlen(temp);
    for(int i ; i<=len;i++){
        if(temp[i]!='/'&&temp[i]!=':'){
            hostname[i]=temp[i];
        }else{
            hostname[i]='\0';
            break;
        }
    }
    printf("HOST:%s,path:%s,port:%s",hostname,path,port);
    return;
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */

/* 
 * 发起并转发http请求
 */
void request_and_doProxy(int fd, char* url, char* hostname, char* path, char* port,rio_t* rio_client){
    rio_t rio_server;
    char buf[MAXLINE], buf_cache[MAX_OBJECT_SIZE];
        int body_size=0, n, can_cache=1;
    char *res_body[MAX_OBJECT_SIZE], *body[MAX_OBJECT_SIZE];
    if (cache_get(&cache,url,res_body))
    {
       Rio_writen(fd,res_body,strlen(res_body));
       return;
    }
    int proxy_host_fd = Open_clientfd(hostname,port);
    Rio_readinitb(&rio_server,proxy_host_fd);
    {
        sprintf(buf,"GET %s HTTP/1.0\r\n",path);
        Rio_writen(proxy_host_fd, buf, strlen(buf));
        sprintf(buf,"Host: %s:%s \r\n",hostname,port);
        Rio_writen(proxy_host_fd, buf, strlen(buf));
        sprintf(buf,user_agent_hdr);
        Rio_writen(proxy_host_fd, buf, strlen(buf));
        sprintf(buf,"Connection: close\r\n");
        Rio_writen(proxy_host_fd, buf, strlen(buf));
        sprintf(buf,"Proxy-Connection: close\r\n");
        Rio_writen(proxy_host_fd, buf, strlen(buf));
        Rio_readlineb(rio_client,buf, MAXLINE);
        while(strcmp(buf,"\r\n")){
            if(!strstr(buf,"Host") && !strstr(buf,"User-Agent") && !strstr(buf,"Connection") && !strstr(buf,"Prox-Connection")){
                Rio_writen(proxy_host_fd, buf, strlen(buf));
            }
            Rio_readlineb(rio_client,buf, MAXLINE);
        }
        sprintf(buf,"\r\n");
        Rio_writen(proxy_host_fd, buf,strlen(buf));
    }

    
    // printf("HTTP response start\n");
    while((n= Rio_readlineb(&rio_server,buf,MAXLINE))!=0)
    {   
        if(debug)printf("end:%d,fd:%d",n,fd);
        Rio_writen(fd,buf,n);
        if( ((n+body_size)<MAX_OBJECT_SIZE) && can_cache){
            strcat(body,buf);
            body_size+= n;
        }else{
            can_cache=0;
        }
        // Fputs(buf1,stdout);
        // printf("%s",buf1);
    }
    if (can_cache)
    {
        cache_put(&cache,url,body);
        printf("URL:%s,cached!",url);
    }
    
    
    Close(proxy_host_fd);
    return;
}
void *thread(void *vargp){
    Pthread_detach(pthread_self());
    // int connfd;
    // int connfd = *(int*)((int*)vargp);
    // free(vargp);
    while (1) {
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */
        doit(connfd);               /* Service client */
        Close(connfd);
    }
}