/******************************
**   @author: Irina Preda
**   @author: 1102452p
**   @title: OP3 Exercise 1
**
*******************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#define PORT 8080

pthread_mutex_t mutex;
pthread_cond_t cond;

struct queue{
  unsigned int front, rear;
  int array[10];
};

void enqueue(struct queue*,long element);
int dequeue(struct queue*);
int queue_full(struct queue*);
int queue_resize(struct queue*);
int queue_empty(struct queue*);

//void signal_callback_handler(int signum){
//	printf("caught sigpipe %d\n",signum);
//}

char *getFileName(char buffer[]){

	char *p;
        char *fileName;
        char *tempbuffer = calloc(strlen(buffer)+1, sizeof(char));;
	if(!tempbuffer)	printf("Error: %s\n",  strerror(errno));

        strcpy(tempbuffer, buffer);
	//printf("%s\n",tempbuffer);
        p = strtok(tempbuffer, " ");

        if(!strcmp(p, "GET")){

                fileName = strtok(NULL, " "); //pass GET
                fileName++; //pass '/'

                p = strtok(NULL, " \r\n");

                if(!strcmp(p, "HTTP/1.1"))
			return fileName;  
		else
			printf("Error: %s\n", fileName);                 
        }

        return NULL;
}

char *getFileType(char *fileName){

        char *p;
        char *currP;
        char *temp = calloc(strlen(fileName)+1, sizeof(char));

        strcpy(temp, fileName);

	//find extension

        p = strtok(temp, "."); 
        p = strtok(NULL, ".");
	currP = p;

        return currP;

}

char *getFileBuffer(char *fileName){

	FILE * file;
        long size;
        char * buffer;
        size_t r;

        file = fopen(fileName, "rb");        
	if(fseek(file, 0, SEEK_END) == -1) printf("Error: %s\n",  strerror(errno));

        size = ftell(file);
	if(size == -1) printf("Error: %s\n",  strerror(errno));

        rewind(file);

        buffer = (char*) malloc(sizeof(char)*size);
	if(!buffer)	printf("Error: %s\n",  strerror(errno));

        r = fread(buffer, 1, size, file);
	if((long)r != size) printf("Error: %s\n",  strerror(errno));

        fclose(file);
        return buffer;
}

char *getHostName(){

        char hostName[1024];
        gethostname(hostName, 1023);
        hostName[1023] = '\0';
	int i = 0;
	while(hostName[i++])
                 hostName[i] = tolower(hostName[i]); // lower case
	char *p = hostName;
        return p;

}

int checkHostHeader(char buffer[]){

        char *finalHost = (char *)malloc(1000);
        strcat(finalHost,getHostName());
	if(!strcmp(finalHost, "serendipity"))	
        strcat(finalHost, ".dcs.gla.ac.uk");
	else
		finalHost = "localhost";

        char *tempbuffer = calloc(strlen(buffer)+1, sizeof(char));
	if(!tempbuffer)	printf("Error: %s\n",  strerror(errno));

        strcpy(tempbuffer, buffer);

        return 1;

}

char *getContentType(char *fileType){

        char *content;

        if(!(strcmp(fileType, "html")) || !(strcmp(fileType, "htm")))
                content = "text/html";

        else if(!strcmp(fileType, "txt"))
                content= "text/plain";

        else if(!(strcmp(fileType, "jpg")) || !(strcmp(fileType, "jpeg")))
                content = "image/jpeg";

        else if(!strcmp(fileType, "gif"))
                content = "image/gif";

        else content = "application/octet-stream";

        return content;

}

void OKResponse(char *fileType, char *filename, char *fileBuffer, int connfd){

        struct stat fs;
        int fd = open(filename, O_RDONLY);
        if(fstat(fd, &fs) == -1)	printf("Error: %s\n",  strerror(errno));

        char *contentType = (char *)malloc(1000);
	if(!contentType) printf("Error: %s\n",  strerror(errno));

        char *contentLength = (char *)malloc(1000);
	if(!contentLength) printf("Error: %s\n",  strerror(errno));

	sprintf(contentType,"Content-Type: %s\r\n",getContentType(fileType));
        sprintf(contentLength,"Content-Length: %ld\r\n",fs.st_size);

        write(connfd, "HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"));
        write(connfd, contentType, strlen(contentType));
        write(connfd, contentLength, strlen(contentLength));
        write(connfd, "Connection: keep-alive\r\n\r\n", strlen("Connection: keep-alive\r\n\r\n"));
        write(connfd, fileBuffer,fs.st_size);
	printf("%lu\n",fs.st_size); // response size
}

void NotFoundError(int connfd){

	write(connfd, "HTTP/1.1 404 Not Found\r\n", strlen("HTTP/1.1 404 Not Found\r\n")); 
	write(connfd, "Content-Type: text/html\r\n", strlen("Content-Type: text/html\r\n")); 
	write(connfd, "Content-Length: 110\r\n", strlen("Content-Length: 110\r\n"));	
	write(connfd, "Connection: keep-alive\r\n\r\n", strlen("Connection: keep-alive\r\n\r\n"));	
	write(connfd, "<html><head><title>404 Not Found</title></head><body><p>The requested file cannot be found.</p></body></html>", strlen("<html><head><title>404 Not Found</title></head><body><p>The requested file cannot be found.</p></body></html>"));

}

void BadRequestError(int connfd){	

	write(connfd, "HTTP/1.1 400 Bad Request\r\n", strlen("HTTP/1.1 400 Bad Request\r\n")); 
	write(connfd, "Content-Type: text/html\r\n", strlen("Content-Type: text/html\r\n"));	
	write(connfd, "Content-Length: 88\r\n", strlen("Content-Length: 88\r\n")); 
	write(connfd, "Connection: keep-alive\r\n\r\n", strlen("Connection: keep-alive\r\n\r\n"));
	write(connfd, "<html><head><title>404 Bad Request</title></head><body><p>Bad Request.</p></body></html>", strlen("<html><head><title>404 Bad Request</title></head><body><p>Bad Request.</p></body></html>"));

}

void InternalServiceError(int connfd){	

	write(connfd, "HTTP/1.1 500 Internal Service Error\r\n", strlen("HTTP/1.1 500 Internal Service Error\r\n")); 
	write(connfd, "Content-Type: text/html\r\n", strlen("Content-Type: text/html\r\n"));	
	write(connfd, "Content-Length: 110\r\n", strlen("Content-Length: 110\r\n")); 
	write(connfd, "Connection: keep-alive\r\n\r\n", strlen("Connection: keep-alive\r\n\r\n"));
	write(connfd, "<html><head><title>500 Internal Service Error</title></head><body><p>Internal Service Error.</p></body></html>", strlen("<html><head><title>500 Internal Service Error</title></head><body><p>Internal Service Error.</p></body></html>"));

}

void *processRequest(void* pConnfd){
	printf("%ld\n",(long)pthread_self());
	printf("Processing Request...\n");	
	int connfd = *(int*) pConnfd;

	while(1){
		pthread_mutex_lock(&mutex); //m
		char buffer[1501]; 
		ssize_t count;
		int hostCheck = 0;
		char *fileName = NULL;
		char *fileType = NULL;
		char *fileBuffer = NULL;

		count = read(connfd, buffer, 1500); 
		if(count == -1){
			InternalServiceError(connfd);
		}

		if(count==0){
			printf("Closing\n");
			close(connfd);
			break;
		}

		buffer[count] = '\0';
	//	printf("%s\n",buffer);
	
		fileName = getFileName(buffer);
		hostCheck = checkHostHeader(buffer);
		if(fileName && hostCheck){
			fileType = getFileType(fileName);
			fileBuffer = getFileBuffer(fileName);
			if(fileBuffer){
				printf("File: %s\n", fileName);
				OKResponse(fileType,fileName,fileBuffer,connfd);
			}
			else	
				NotFoundError(connfd);
		}
		else	
			BadRequestError(connfd);

		pthread_mutex_unlock(&mutex); //m
	}
	close(connfd);	
    	pthread_exit(NULL);  //m
}

int main(void) {
	printf("Started...\n");

	// create socket address var
	struct sockaddr_in addr;

	// create socket
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd == -1){
		// an error occurred
		perror("Error when creating socket");
		return -1;
	}
	else
		printf("Socket created...\n");

	// make socket into TCP server and bind socket to port
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);

	if(bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1){
		// an error occurred
		perror("Error when binding socket");
		return -1;
	}
	else
		printf("Socket binded...\n");
	

	// start listening
	int backlog = 10;

	if(listen(fd, backlog) == -1){
		// an error occurred
		perror("Error when starting listening");
		return -1;			
	}
	else
		printf("Listening...\n");	

//	int i=0;
//	pthread_t thread[10];

/*	pthread_t thr_listener, thr_scheduler, thr_server[10];
	int thr_no = 10;
	pthread_attr_t attr;*/

	pthread_t thread;

	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);		
/*	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);*/

	// START LOOP
	while(1){ 

		// accept new connections
		int connfd;
		struct sockaddr_in cliaddr;
		socklen_t cliaddrlen = sizeof(cliaddr);
		connfd = accept(fd, (struct sockaddr *) &cliaddr, &cliaddrlen);

		if(connfd == -1){
			// an error occurred
			perror("Error when accepting socket");
			return -1;	
		}
		else
			printf("Socket accepted...\n");
		
		//create thread
		if(pthread_create(&thread, NULL, processRequest, &connfd)){
			// an error occurred
			perror("Error creating thread");
			return -1;
		}
		else
			printf("Thread created...\n");

		//join thread
		if(pthread_join(thread,NULL)){
			// an error occurred
			perror("Error joining thread\n");
			return -1;
		}
		else
			printf("Threads joined...\n");

		// close socket
		close(connfd);
	}  // END LOOP

/*		//join thread
	int j;
	for(j=0;j<i;j++){
		if(pthread_join(thread[j],NULL)){
			// an error occurred
			perror("Error joining thread\n");
			return -1;
		}
		else
			printf("Threads joined...\n");
	}
	signal(SIGPIPE,signal_callback_handler);

	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
	pthread_exit(NULL);*/
	return 0;
}


