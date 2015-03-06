/*
Prethreaded Chat server
Written by:
Daman Arora
Siddhant Shrivastava
Gokul Krishnan
Ruchika Luthra 
*/

#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>
#include <sys/select.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>


#define	MAXN	16384		/* max # bytes client can request */
#define MAXLINE 108
#define MAX_BUF 300		/* Maximum bytes fetched by a single read() */
#define C_MAX 20 

int tot_conns = 0; //total number of connections
int connfds [20] = {0}; //array of connection fds
char nicks [20][20]; //array of nicks


void web_child(int sockfd)
{
	int			ntowrite,i,a;
	ssize_t		nread;

	char msg[MAXLINE],line[MAXLINE],line1[MAXLINE], result[MAXN],command[5],nick[MAXLINE];

	for ( ; ; ) {
			strcpy(line,"");
			int a = recv(sockfd, line, sizeof(line),0);
			int k = 0;
			for (i = 0; i < 5; ++i)
			{
				command[i] = line[i];	
			}
			command[4] = '\0';
			
			if(strcmp("JOIN",command)==0)
			{
				//int a = recv(sockfd, line, sizeof(line),0);
				
				for(i=5;line[i]!='\0';i++)
					nick[i-5] = line[i];
					nick[i-5] = '\0';
				for(i=0;i<20;i++)
				{
					if(strlen(nicks[i])==0)
					{
						strcpy(nicks[i],nick);
						connfds[i] = sockfd;
						tot_conns++;
						break;
					}						
				}
				if(i == 20)
				{
					send(sockfd,"Too many users, try later",strlen("Too many users, try later"),0);
				}

			}

			if(strcmp("LIST",command)==0)
			{
				for(i=0;i<20;i++)
				{
					if(strlen(nicks[i]) != 0)
					{
						send(sockfd,nicks[i],strlen(nicks[i]),0);
					}
				}
			}

			if(strcmp("UMSG",command)==0)
			{
				//code for messaging
				
				strcpy(line1,"");
				//get user name
				if ( (nread = recv(sockfd,line1,MAXLINE,0)) == 0)
				return;		/* connection closed by other end */
				strcpy(line,"");
				//get message
				if ( (nread = recv(sockfd,line,MAXLINE,0)) == 0)
					return;		/* connection closed by other end */
					for(i=0;i<strlen(line);i++)
					{
						if(line[i]=='\n')
						{
							line[i+1] = '\0';
							break;
						}	
					}
					printf("\n SENDING: %s to %s \n",line,line1);
				for(i=0;i<20;i++)
				{
					if(strcmp(nicks[i],line1)==0)
					{
						send(connfds[i],line, strlen(line),0);
						break;
					}
				}
		
			}

			if(strcmp("BMSG",command)==0)
			{
				//code for broadcast
				strcpy(line,"");
				if ( (nread = recv(sockfd,line,MAXLINE,0)) == 0)
				return;		/* connection closed by other end */
				
				for(i=0;i<strlen(line);i++)
				{
					if(line[i]=='\n')
					{
						line[i+1] = '\0';
						break;
					}	
				}
				printf("\n SENDING: %s \n",line);
				for(i=0;i<20;i++)
				{
					if(connfds[i]!=0)
					{

						send(connfds[i],line, strlen(line),0);

					}
				}
			}

			if(strcmp("LEAV",command)==0)
			{	//code for leaving
				for(i=0;i<20;i++)
				{
					if(connfds[i] == sockfd)
					{
						connfds[i]=0;
						strcpy(nicks[i],"");
						tot_conns--;
					}
				}
				close(sockfd);

			}			
	}
}

typedef struct {
  pthread_t		thread_tid;		/* thread ID */
  long			thread_count;	/* # connections handled */
} Thread;
Thread	*tptr;		/* array of Thread structures; calloc'ed */

int				listenfd, nthreads;
socklen_t		addrlen;
pthread_mutex_t	mlock;
pthread_mutex_t	mlock = PTHREAD_MUTEX_INITIALIZER;

void
sig_int(int signo)
{
	int		i;

	for (i = 0; i < nthreads; i++)
		printf("thread %d, %ld connections\n", i, tptr[i].thread_count);

	exit(0);
}

void
thread_make(int i)
{
	void	*thread_main(void *);

	pthread_create(&tptr[i].thread_tid, NULL, &thread_main, (void *) i);
	return;		/* main thread returns */
}

void *
thread_main(void *arg)
{
	
	int				connfd;
	void			web_child(int);
	//void * QueryProcessor ( void * arg );
	socklen_t		clilen;
	struct sockaddr	*cliaddr;

	cliaddr = malloc(addrlen);

	printf("thread %d starting\n", (int) arg);
	for ( ; ; ) {
		clilen = addrlen;
    	pthread_mutex_lock(&mlock);
		connfd = accept(listenfd, cliaddr, &clilen);
		pthread_mutex_unlock(&mlock);
		tptr[(int) arg].thread_count++;

		web_child(connfd);		/* process request */
		
		close(connfd);
	}
}


int main(int argc, char **argv)
{
	int		i;
	void	sig_int(int), thread_make(int);

	if(argc != 3)
	{
		perror("usage: prethread_chat_server <port#> <#of threads>");
		exit(1);
	}

	listenfd =   socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	addr.sin_port = htons(atoi(argv[1]));
	
	bzero(addr.sin_zero,sizeof(addr.sin_zero));

	bind(listenfd, (struct sockaddr *)&addr,sizeof(addr));
	listen(listenfd,10);

	nthreads = atoi(argv[2]);
	tptr = calloc(nthreads, sizeof(Thread));

	for (i =0; i< 20;i++)
	{
		connfds[i] = 0;
		strcpy(nicks[i],"");
	}
	for (i = 0; i < nthreads; i++)
		thread_make(i);			/* only main thread returns */

	signal(SIGINT, sig_int);

	for ( ; ; )
		pause();	/* everything done by threads */
}

