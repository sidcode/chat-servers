/*
Event-driven concurrent Chat server
Written by:
Daman Arora
Siddhant Shrivastava
Gokul Krishnan
Ruchika Luthra 
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>

#include <netinet/in.h>

#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>


#define M_SIZE 4000 //Maximum text character limit in the queue
#define KEY 4214 // key for shared memory


//Message Queue Structure
typedef struct msgbuf {
	long  mtype;
	char  mtext[M_SIZE];
	}message_buf;

//Function header for sending message to the message queue
void sendMessage( char * message );

//Function header for receiving message from the message queue
void recvMesssage( char * msgBuf ) ;

#define C_MAX 20 //Maximum number of clients

//Function header for processing by the thread
void * QueryProcessor( void * );

int PORT[C_MAX]; //Ports assigned to every client
//IP assigned for every client
char IP[C_MAX][20]; 
 //Number of accepted connections
int CLIENTS = 0;
//number of clients who have joined a chat session
int jCLIENTS=0; 

//connection file descriptors for all clients
int conFd[C_MAX];

//Nicknames for all joined clients
char NICK[C_MAX][20];

//error reporting variable
extern int errno;

//listen backlog limit
#define LISTENQ 20
/* Maximum bytes fetched by a single read() */
#define MAX_BUF 300
/* Maximum number of events to be returned from
a single epoll_wait() call */
#define MAX_EVENTS 5		


//error exit
void errExit (char *s) {
	perror (s);
	exit (1);
}

//Main Thread
int main (int argc, char *argv[]) {

  if(argc!=2)
  {
    printf("Usage: ./a.out <port_no>\n");
    exit(1);
  }
  
  //epoll fd, ready events, number of open fds
	int epfd, ready, fd, s, j, num0penFds , k;

	//epoll event structure
	struct epoll_event ev;

	//epoll event list
	struct epoll_event evlist[MAX_EVENTS];
	char buf[MAX_BUF];

	//Socket fd
	int sock, clilen;

	struct sockaddr_in cliaddr, servAddr;

	//Socket creation - set to be nonblocking
	sock = socket (AF_INET, SOCK_STREAM, 0);


	bzero (&servAddr, sizeof (servAddr));

	//setting up the server address structure
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl (INADDR_ANY);
	servAddr.sin_port = htons (atoi (argv[1]));

	//binding the socket to the particular port and address
	if (bind ( sock , (struct sockaddr *) &servAddr, sizeof (servAddr)) < 0)
		perror ("bind");

	//listening on socketfd
	listen (sock, LISTENQ);

	//create epoll fd
	epfd = epoll_create (20);
	if (epfd == -1)
		errExit ("epoll_create");

	//Adding listening socket to the epoll interest list
	ev.events = EPOLLIN;		/* Only interested in input events */
	ev.data.fd = sock;

	//add listening socket to interest list
	if (epoll_ctl (epfd, EPOLL_CTL_ADD,sock, &ev) == -1 )
		errExit ("epoll_ctl");

	//thread variable
	pthread_t tid;

	//create pthread
	pthread_create( &tid, NULL, QueryProcessor, NULL);

	//start the chat server
	for ( ; ; ) {
				
			//get a list of file descriptors of ready events
		ready = epoll_wait (epfd, evlist, MAX_EVENTS, 0 );

		if (ready == -1) {
			if (errno == EINTR)
				continue;		/* Restart if interrupted by signal */
			else
				errExit ("epoll_wait");
		}

		//handle ready events
		for (j = 0; j < ready; j++) {

			
			if (evlist[j].events & EPOLLIN) {

				if ( evlist[j].data.fd == sock ) {

					clilen = sizeof (cliaddr);
					char ip[128];
					memset (ip, '\0', 128);
				

					conFd[CLIENTS] = accept (sock, (struct sockaddr *) &cliaddr, &clilen);
					if (cliaddr.sin_family == AF_INET) {
						if (inet_ntop (AF_INET, &(cliaddr.sin_addr), ip, 128) ==  '\0' )
							perror ("Error in inet_ntop\n");
					}

					if (cliaddr.sin_family == AF_INET6) {
						inet_ntop (AF_INET6, &(cliaddr.sin_addr), ip, 128);
					}			


					strcpy(IP[CLIENTS],ip);
					PORT[CLIENTS] = ntohs(cliaddr.sin_port);					
					ev.events = EPOLLIN;
					ev.data.fd = conFd[CLIENTS];
					
					if (epoll_ctl (epfd, EPOLL_CTL_ADD, conFd[CLIENTS] , &ev) == -1)
							errExit ("epoll_ctl");

					CLIENTS++;
				}
				
				else {
					char temp[50];
					char msg_dump[MAX_BUF];
					int s = read (evlist[j].data.fd, buf, MAX_BUF);					
					buf[s] = '\0';

					if (s == -1)
						errExit ("read");
					if (s == 0)
						close (evlist[j].data.fd);
					if (s > 0)
						sprintf(msg_dump,"R#%d#%s",evlist[j].data.fd,buf);
						sendMessage(msg_dump);

				}
			}
		}
	}
}

void * QueryProcessor ( void * arg ){
	fflush(stdout);
	char queBuf[MAX_BUF];
	char clientBuf[MAX_BUF];
	int sender;
	char temp[20];
	int k =0;
	int i=0;
	int j;

	strcpy(queBuf,"MSG_QUE_INIT");
	sendMessage(queBuf);

	queBuf[0] = '\0';

	recvMesssage(queBuf);

	while(1) {

		char mode;
		queBuf[0] = '\0';

		recvMesssage(queBuf);

		mode = queBuf[0];

		i=0;

		for(k=2;k<strlen(queBuf)-1;k++) {

			if(queBuf[k]=='#')
				break;

			temp[i] = queBuf[k];
			i++;
		}

		temp[i] = '\0';

		sender = atoi(temp);

		i=0;

		k++;

		while(k<strlen(queBuf)) {
			clientBuf[i] = queBuf[k];
			k++;
			i++;
		}

		clientBuf[i] = '\0';

		int flag = 0;

		for(k=0;k<jCLIENTS;k++){
			if(conFd[k] == sender) {
				if( strcmp("NULL",IP[k]) == 0)
					flag++;
			}
		}

		if(flag==0) {

			if( mode == 'W' ) {	
				send(sender,clientBuf,strlen(clientBuf)-1,0);
			}

			else if ( mode == 'R') {

				char command[5];

				for(i=0;i<5;i++)
					command[i] = clientBuf[i];

				command[4] = '\0';

				if(strcmp(command,"JOIN")==0) {

					k=0;
					while(i<MAX_BUF) {

						if(clientBuf[i]=='\r'){
							if(clientBuf[i+1]=='\n'){								
								break;									
							}
						}							

					NICK[jCLIENTS][k] = clientBuf[i];
					i++;
					k++;
					}

				NICK[jCLIENTS][k] = '\0';
				jCLIENTS++;
			
				}

				else if(strcmp(command,"LIST")==0) {
					j=1;
					char LIST_clientBuffer[MAX_BUF];
					char msg_dump_L[MAX_BUF];
					LIST_clientBuffer[0] = '\0';
					char LIST_temp[100];
					strcpy(LIST_clientBuffer,"List of Online Clients -\n");
					for(k=0;k<jCLIENTS;k++) {
						//if(conFd[k]!=sender){
						 	if( strcmp(IP[k],"NULL") !=0 ) {
								LIST_temp[0] = '\0';
								sprintf( LIST_temp , "\n\t%d. |%s| \t\tAddress - ( %s , %d )" , j, NICK[k], IP[k], PORT[k] );
								strcat(LIST_clientBuffer,LIST_temp);
							j++;
							}
						//}
					}
					strcat(LIST_clientBuffer,"\n\n");
					LIST_clientBuffer[strlen(LIST_clientBuffer)] = '\0';
					sprintf(msg_dump_L,"W#%d#%s",sender,LIST_clientBuffer);
					sendMessage(msg_dump_L);

				}

				else if(strcmp(command,"BMSG")==0) {
					char msg_dump_B[MAX_BUF];
					msg_dump_B[0] = '\0';
					char BMSG_clientBuffer[MAX_BUF];
					char BMSG_clientBuffer1[MAX_BUF];
					char msg_dump[MAX_BUF];
					char temp[100];
					BMSG_clientBuffer[0] = '\0';
					k=0;

					while(i<MAX_BUF) {

						if(clientBuf[i]=='\\'){
							if(clientBuf[i+1]=='r'){
								if(clientBuf[i+2]=='\\'){
									if(clientBuf[i+3]=='n'){
										break;
									}
								}
							}
						}

						BMSG_clientBuffer[k] = clientBuf[i];
						i++;
						k++;
					}

					char Nick[20];
					char Ip[20];

					for(k=0;k<jCLIENTS;k++) {
						if (conFd[k] == sender)
							strcpy(Nick,NICK[k]);
							strcpy(Ip,IP[k]);
					}

					sprintf(BMSG_clientBuffer1,"Broadcast Message by -  %s ->  %s \n\t : %s \n\n", Ip , Nick ,BMSG_clientBuffer );

					for(k=0;k<jCLIENTS;k++) {
						//if(conFd[k]!=sender) {
						 	if( strcmp(IP[k],"NULL") !=0 ) {
								sprintf(msg_dump_B,"W#%d#%s",conFd[k],BMSG_clientBuffer1);								
								sendMessage(msg_dump_B);
							}
						//}
					}

				}

				else if(strcmp(command,"LEAV")==0) {

					for(k=0;k<jCLIENTS;k++) {
						if(conFd[k]==sender) {
							strcpy(NICK[k],"NULL");
							strcpy(IP[k],"NULL");
							PORT[k] = 0;
						}
					}
				}

				else if(strcmp(command,"UMSG")==0) {
					char UMSG_clientBuffer[MAX_BUF];
					char UMSG_clientBuffer1[MAX_BUF];
					char user[20];
					char msg_dump_U[MAX_BUF];
					char temp[10];
					msg_dump_U[0] = '\0'; 
	 				UMSG_clientBuffer[0] = '\0';
					k=0;
					printf("\n\t Socket Read - ]%s[\t",clientBuf);
					while(i<MAX_BUF) {

						if(clientBuf[i]=='\\'){
								if(clientBuf[i+1]=='r'){
									if(clientBuf[i+2]=='\\'){
										if(clientBuf[i+3]=='n'){
											break;
										}
									}
								}
							}

						user[k] = clientBuf[i];
						i++;
						k++;
					}

					user[k] = '\0';
					i+=5;
					UMSG_clientBuffer[0] = '\0';
					k=0;

					while(i<MAX_BUF) {
							if(clientBuf[i]=='\\'){
								if(clientBuf[i+1]=='r'){
									if(clientBuf[i+2]=='\\'){
										if(clientBuf[i+3]=='n'){
											break;
										}
									}
								}
							}
						UMSG_clientBuffer[k] = clientBuf[i];
						i++;
						k++;
					}
					strcat(UMSG_clientBuffer,"\n");

					char Nick[20];
					char Ip[20];

					for(k=0;k<jCLIENTS;k++) {
						if (conFd[k] == sender)
							strcpy(Nick,NICK[k]);
							strcpy(Ip,IP[k]);
					}

					UMSG_clientBuffer[strlen(UMSG_clientBuffer)] = '\0';
					int flag = 0;

					sprintf(UMSG_clientBuffer1,"New Personal Message by -  %s ->  %s \n\t : %s \n\n", Ip , Nick ,UMSG_clientBuffer );

					for(k=0;k<jCLIENTS;k++) {
						if( strcmp(NICK[k],user) == 0 && strcmp(IP[k],"NULL") !=0 ) {
							flag++;
							sprintf(msg_dump_U,"W#%d#%s",conFd[k],UMSG_clientBuffer1);
							sendMessage(msg_dump_U);
							break;
						}
					}

					if(flag==0) {
						char UMSG_temp[20];
						UMSG_clientBuffer[0] = '\0';
						sprintf(UMSG_temp,"\nERROR - USER |%s| NOT ONLINE\n\n",user);
						strcpy(UMSG_clientBuffer,UMSG_temp);
						UMSG_clientBuffer[strlen(UMSG_clientBuffer)] = '\0';
						sprintf(msg_dump_U,"W#%d#%s",sender,UMSG_clientBuffer);
						sendMessage(msg_dump_U);
					}
				}
			}
		}
	}
}


void recvMesssage ( char * msgBuf ) {

	int msqid;
	key_t key = KEY;
	message_buf  rbuf;
	if ((msqid = msgget(key, 0666 | IPC_NOWAIT )) < 0) {
		strcpy(msgBuf,"error");
	}
	while(1) {
		if (msgrcv(msqid, &rbuf, M_SIZE, 1, 0) > 0) {
			strcpy(msgBuf,rbuf.mtext);
			break;
		}
	}
}

void sendMessage ( char * message ) {

	int msqid;
	int msgflg = IPC_CREAT | 0666;
	key_t key = KEY ;
	message_buf sbuf;
	size_t buf_length;
	if ((msqid = msgget(key, msgflg )) < 0) {
		perror(".......");
	}
	
	else {
		sbuf.mtype = 1;
	}
	strcpy(sbuf.mtext,message);
	buf_length = strlen(sbuf.mtext) + 1 ;

	msgsnd(msqid, &sbuf, buf_length,0);
}	