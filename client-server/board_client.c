#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include "board_server.h"

#define DATASIZE 200//size of data stored in server(messages or file paths)
#define NAMESIZE 20//size of channel name
#define BUFFERSIZE 2048

void client(char* path);
void client_getmessages(int CtS_fd,int StC_fd);
void client_shutdown(char* path,int CtS_fd,int StC_fd);

int main(int argc,char* argv[]){
	signal(SIGINT,SIG_IGN);//ignore SIGINT
	if(argc != 2){
		printf("failure: application call should be: ./board <path>\n");
		exit(EXIT_FAILURE);
	}	
	int status;
	status=mkdir(argv[1], 0755);
	if(status == 0){//board dir created,create fifos,launch server
		/*create named pipes*/
		char fifoname[100];
		strcpy(fifoname,argv[1]);
		strcat(fifoname,"/StC");
		if ( mkfifo (fifoname,0666) == -1 ){
			perror("failure creating fifo");
		}
		strcpy(fifoname,argv[1]);
		strcat(fifoname,"/CtS");
		if ( mkfifo (fifoname,0666) == -1 ){
			perror("failure creating fifo");
		}
		strcpy(fifoname,argv[1]);
		strcat(fifoname,"/BtS");
		if ( mkfifo (fifoname,0666) == -1 ){
			perror("failure creating fifo");
		}
		strcpy(fifoname,argv[1]);
		strcat(fifoname,"/StB");
		if ( mkfifo (fifoname,0666) == -1 ){
			perror("failure creating fifo");
		}
		/*launch server*/
		pid_t pid;
		pid = fork ();
		if ( pid == 0) {
			server(argv[1]);
		}
		else if( pid > 0){
			/*create file to store server pid*/
			char filename[100];
			strcpy(filename,argv[1]);
			strcat(filename,"/spid");
			FILE* fp=fopen(filename,"w");
			fprintf(fp,"%ld",(long int)pid);
			fclose(fp);			
			/*call client function*/
			client(argv[1]);
		}
		else{
			perror ( "failure on fork" ) ;
			exit (EXIT_FAILURE) ;
		}	
	}
	else if(errno == EEXIST){//directory already exists,manage server
		/*check if server process is online*/
		pid_t pid;
		char filename[100];
		strcpy(filename,argv[1]);
		strcat(filename,"/spid");
		FILE* fp=fopen(filename,"r");
		if (fp==NULL){
			perror("error opening spid file");
			exit(EXIT_FAILURE);
		}		
		fscanf(fp,"%ld",(long int *)&pid);
		fclose(fp);
		if(kill(pid,0)==0){//server is online
			printf("board server is online\n");
			client(argv[1]);
		}
		else{
			if(errno==ESRCH){//server has gone offline,launch it again
				printf("board server is offline.Resuming\n");
				pid = fork ();
				if ( pid == 0) {
					server(argv[1]);
				}
				else if( pid > 0){
					/*update spid file with the new pid*/
					if  ((fp=fopen(filename,"w"))==NULL){
						perror("error opening spid file");
						exit(EXIT_FAILURE);
					}
					fprintf(fp,"%ld",(long int)pid);
					fclose(fp);			
					/*call client function*/
					client(argv[1]);
				}
				else{
					perror ( "failure on fork" ) ;
					exit (EXIT_FAILURE) ;
				}				
			}
		}
	}
	else{//error creating directory
		perror("failure creating directory");
		exit(EXIT_FAILURE);
	}
}


//decode input send commands to server
void client(char* path){
	/*open necessary pipes*/
	int CtS_fd,StC_fd;
	char fifoname[100];
	strcpy(fifoname,path);
	strcat(fifoname,"/StC");
	if (( StC_fd = open (fifoname,O_RDONLY)) < 0){
		perror("fifo open error");
		exit (EXIT_FAILURE) ;
	}
	strcpy(fifoname,path);
	strcat(fifoname,"/CtS");
	if ((CtS_fd = open (fifoname,O_WRONLY)) < 0){
		perror("fifo open error");
		exit (EXIT_FAILURE) ;
	}
	/*get commands from stdin*/
	char buf[100];
	char* token;
	char *delimiters=" \n";
	while(1){
		fgets(buf,sizeof(buf),stdin);
		token=strtok(buf,delimiters);
		if(token==NULL) continue;
		if(strcmp(token,"createchannel")==0){//CREATECHANNEL
			int arg_num=1;
			int id;
			char name[NAMESIZE];
			while((token=strtok(NULL,delimiters))!=NULL){
				if (arg_num==1){//get id
					id=atoi(token);
					arg_num++;
					continue;
				}
				if (arg_num==2){//get name
					strcpy(name,token);
					arg_num++;
					break;
				}				
			}
			if (arg_num<3){
				printf("call like this : createhannel <id> <name>\n");
				continue;
			}
			/*send create channel command on server*/
			char* cmd="CRT";
			if (( write ( CtS_fd,cmd,strlen(cmd)+1) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}	
			/*send id and name of the new channel*/
			if (( write ( CtS_fd,&id,sizeof(id)) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}
			if (( write ( CtS_fd,name,sizeof(name)) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}
		}
		else if (strcmp(token,"getmessages")==0){//GETMESSAGES
			int arg_num=1;
			int id;
			while((token=strtok(NULL,delimiters))!=NULL){
					id=atoi(token);
					arg_num++;
					break;
			}
			if (arg_num<2){
				printf("call like this : getmessages <id>\n");
				continue;
			}
			/*send getmessages command to server,and id of channel*/
			char* cmd="GET";
			if (( write ( CtS_fd,cmd,strlen(cmd)+1) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}
			if (( write ( CtS_fd,&id,sizeof(int)) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}
			/*call getmessages function*/
			client_getmessages(CtS_fd,StC_fd);	
		}
		else if (strcmp(token,"exit")==0){//EXIT
			/*close file descs*/
			close(CtS_fd);
			close(StC_fd);
			/*exit from client process*/
			exit(EXIT_SUCCESS);
		}
		else if (strcmp(token,"shutdown")==0){//SHUTDOWN
			/*send shutdown command on server*/
			char* cmd="SHT";
			if (( write ( CtS_fd,cmd,strlen(cmd)+1) ) == -1){
				perror ( " Error in Writing ") ;
				continue;
			}
			/*call shutdown function*/
			client_shutdown(path,CtS_fd,StC_fd);	
		}
		else{
			printf("failure:not a valid command.Try again\n");
		}	
	}
}

//get messages from the requested channel
void client_getmessages(int CtS_fd,int StC_fd){
	char type;	
	char data[DATASIZE];
	if(read(StC_fd,&type,sizeof(char)) < 0){
		perror ("Error in reading") ;
		exit(EXIT_FAILURE);
	}
	while (type != 'e'){
		if (type=='m'){//if message
			if (read(StC_fd,data,DATASIZE)<0){
				perror ("Error in reading");
				exit(EXIT_FAILURE);
			}
			printf("Message received: %s\n",data);
		}
		else{//if file		
			/*read file name*/
			char filename[DATASIZE];
			if(read(StC_fd,filename,sizeof(filename)) < 0){
				perror ("Error in reading") ;
				exit(EXIT_FAILURE);	
			}
			/*read file size*/
			int file_size; 
			if(read(StC_fd,&file_size,sizeof(int)) < 0){
				perror ("Error in reading") ;
				exit(EXIT_FAILURE);	
			}
			/*create new file*/
			int fd;
			while((fd=open(filename,O_EXCL | O_CREAT | O_WRONLY,0755))<0){
				if (errno==EEXIST){
					strcat(filename,"_new");
					//fd=open(filename,O_CREAT | O_WRONLY,0755);
				}
				else{
					perror("<client>Error in opening ");
					exit(EXIT_FAILURE);
				}		
			}
			/*read file chunks from server and copy it to newly opened file*/
			char buffer[BUFFERSIZE];
			int read_bytes_total=0;                
			int read_bytes=0;
			int write_bytes_total=0;
			int write_bytes=0;
			int left_to_read=file_size;
			while (read_bytes_total < file_size ){
				/*if left_to_read bytes >= BUFFERSIZE then read BUFFERSIZE chunks*/
				/*else read what is left*/
				if (left_to_read >= BUFFERSIZE){
				    if ((read_bytes=read(StC_fd,buffer,BUFFERSIZE) ) == -1){
						perror ("Error in reading client") ;
						exit(EXIT_FAILURE); 
					}
				}
				else{
					if ((read_bytes=read(StC_fd,buffer,left_to_read) ) == -1){
						perror ("Error in reading client") ;
						exit(EXIT_FAILURE); 
					}
				}
				if ((write_bytes=write(fd,buffer,read_bytes) ) == -1){
					perror ("Error in writing client") ;
					exit(EXIT_FAILURE); 
				}
				read_bytes_total+=read_bytes;
				write_bytes_total+=write_bytes;
				left_to_read-=read_bytes;
		  	}
			close(fd);
			if(write_bytes_total==file_size){
				printf("file %s received successfully\n",filename);
			}
			else{
				printf("file %s received unsuccessfully\n",filename);
			}
		}
		/*read type for next iteration of while*/
		if(read(StC_fd,&type,sizeof(char)) < 0){
			perror ("Error in reading") ;
			exit(EXIT_FAILURE);
		}
	}
	printf("No more messages to receive.\n");
}


//manage board directory ,kill shutdown server and exit
void client_shutdown(char* path,int CtS_fd,int StC_fd){
	/*give server some time to complete its shutdown operations*/
	sleep(1);
	/*close file descs*/
	close(CtS_fd);
	close(StC_fd);
	/*delete files of board,and board dir*/
	char fifoname[100];
	char filename[100];
	strcpy(fifoname,path);
	strcat(fifoname,"/StC");
	unlink(fifoname);

	strcpy(fifoname,path);
	strcat(fifoname,"/CtS");
	unlink(fifoname);

	strcpy(fifoname,path);
	strcat(fifoname,"/BtS");
	unlink(fifoname);

	strcpy(fifoname,path);
	strcat(fifoname,"/StB");
	unlink(fifoname);

	strcpy(filename,path);
	strcat(filename,"/spid");
	remove(filename);

	rmdir(path);
	/*exit from client process*/
	exit(EXIT_SUCCESS);
}
