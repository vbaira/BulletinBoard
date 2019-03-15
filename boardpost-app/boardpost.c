#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <libgen.h>

#define DATASIZE 200//size of data stored in server(messages or file paths)
#define NAMESIZE 20//size of channel name
#define BUFFERSIZE 2048

int list(char* path);
int write_message(int id,char* message,int BtS_fd);
int send_file(char* filepath,int id,int BtS_fd);
void sigpipe_handler(int signum); 

int main (int argc,char * argv[]){	
	signal(SIGPIPE, sigpipe_handler); //handle SIGPIPE (server offline)
	signal(SIGINT,SIG_IGN); //ignore SIGINT
	/*command line input check*/
	if(argc != 2){
		printf("failure: application call should be: ./boardpost <path>\n");
		exit(EXIT_FAILURE);
	}
	/* check if directory exists*/
	DIR* dir = opendir(argv[1]);
	if (dir){
		closedir(dir);
	}
	else if (errno==ENOENT){
		printf("failure: directory doesnt exist\n");
		exit(EXIT_FAILURE);
	}
	else{
		perror("Error opening directory");
		exit(EXIT_FAILURE);
	}
	/*open pipe to communicate with server*/
	int BtS_fd;
	char fifoname[100];
	strcpy(fifoname,argv[1]);
	strcat(fifoname,"/BtS");
	if ( (BtS_fd = open (fifoname, O_WRONLY)) < 0){
		perror("fifo open error");
		exit (EXIT_FAILURE) ;
	}
	/*take input from stdin*/
	char buf[DATASIZE+100];
	char* token;
	char *delimiters=" \n";
	while(1){
		fgets(buf,sizeof(buf),stdin);
		token=strtok(buf,delimiters);
		if(token==NULL) continue;
		if ( strcmp(token,"list")==0 ){//LIST
			/*send list command to server*/
			char* cmd="LST";
			if (( write ( BtS_fd,cmd,strlen(cmd)+1) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}
			/*call list function*/
			if (list(argv[1]) < 0){
				printf("List failed\n");
				perror(" ");
				exit(EXIT_FAILURE);
			}	
		}
		else if ( strcmp(token,"write")==0 ){//WRITE
			int arg_num=1;
			int id;
			char message[DATASIZE];
			while((token=strtok(NULL,delimiters))!=NULL){
				if (arg_num==1){//get id
					id=atoi(token);
					arg_num++;
					continue;
				}
				else{
					if (arg_num==2) strcpy(message,token);
					else{
						strcat(message," ");
						strcat(message,token);
					}
					arg_num++;
				}
			}
			if (arg_num<3){
				printf("call like this:write <id> <messages>\n");
				continue;
			}
			/*send write messages command to server*/
			char* cmd="WRT";
			if (( write ( BtS_fd,cmd,strlen(cmd)+1) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}	
			/*call write_message function*/
			if (write_message(id,message,BtS_fd) < 0){
				printf("Write message failed\n");
				perror(" ");
				exit(EXIT_FAILURE);
			}	
		}
		else if ( strcmp(token,"send")==0 ){//SEND
			int arg_num=1;
			int id;
			char filepath[DATASIZE];
			while((token=strtok(NULL,delimiters))!=NULL){
				if (arg_num==1){//get id
					id=atoi(token);
					arg_num++;
					continue;
				}
				if (arg_num==2){//get filename
					strcpy(filepath,token);
					arg_num++;
					break;
				}			
			}
			if (arg_num<3){
				printf("call like this:send <id> <file>\n");
				continue;
			}
			/*send send file command to server*/
			char* cmd="SND";
			if (( write ( BtS_fd,cmd,strlen(cmd)+1) ) == -1){
				perror ( " Error in Writing ") ;
				continue ; 
			}
			/*call send file function*/
			int status;	
			if ((status=send_file(filepath,id,BtS_fd)) <0){
				printf("Send file failed\n");
				if (status ==-2){//if file doesnt exist
					continue;
				}
				else{
					perror(" ");
					exit(EXIT_FAILURE);
				}
			}
		}
		else if (strcmp(token,"exit")==0){
			close(BtS_fd);
			exit(EXIT_SUCCESS);
		}
		else{
			printf("failure:not a valid command.Try again\n");
		}

	}
}


int send_file(char* filepath,int id,int BtS_fd){
	/*open file*/
	int file_fd=open(filepath,O_RDONLY);
	if (file_fd < 0){
		perror("error opening file");
		int error_id=-1;
		write(BtS_fd,&error_id,sizeof(int));//send error id to server
		return -2;
	}
	/*send id and file name to server*/
	if (( write ( BtS_fd,&id,sizeof(int)) ) == -1){
		perror ( " Error in Writing ") ;
		return -1 ; 
	}
	char filename[DATASIZE];
    strcpy(filename,basename(filepath));
	if (( write ( BtS_fd,filename,sizeof(filename)) ) == -1){
		perror ( " Error in Writing ") ;
		return -1 ; 
	}
	/*get file size and send it to server*/
	struct stat results;          
 	if (stat(filepath,&results) < 0){
		perror ("Error in stat") ;
		return -1 ;         
	}
	int file_size=results.st_size;
	if ((write( BtS_fd,&file_size,sizeof(int)) ) == -1){
		perror ( " Error in Writing ") ;
	    return -1 ; 
    }             
	/*read file chunks and send them to server*/
	char buffer[BUFFERSIZE];
	int read_bytes=0;                
	int write_bytes_total=0;
	int write_bytes=0;
	while (write_bytes_total < file_size ){
		if ((read_bytes=read(file_fd,buffer,BUFFERSIZE)) == -1){
			perror ( " Error in Reading ") ;
			return -1 ; 
		}
		if ((write_bytes=write ( BtS_fd,buffer,read_bytes)) == -1){
			perror ( " Error in Writing ") ;
			return -1 ; 
		}
		write_bytes_total+=write_bytes;   
	}
	if (write_bytes_total == file_size) printf("file transfer complete\n");
	/*close file*/
	close(file_fd);
}




int list(char* path){
	/*open pipe to read from server*/
	int StB_fd;
	char fifoname[100];
	strcpy(fifoname,path);
	strcat(fifoname,"/StB");
	if ( (StB_fd = open (fifoname, O_RDONLY)) < 0){
		perror("fifo open error");
		return -1;
	}
	/*read ids and names of channels*/
	/*id<0 signals that no more channelsare available*/
	int id;
	char name[NAMESIZE];
	printf("CHANNELS LIST:\n");
	int read_bytes=read(StB_fd,&id,sizeof(int)); //get channel id
	if (read_bytes != sizeof(int)) return -1;
	while (id >= 0){
		read_bytes=read(StB_fd,name,NAMESIZE); 
		if (read_bytes != NAMESIZE) return -1;
		printf("ID:%d | NAME:%s\n",id,name);
		read_bytes=read(StB_fd,&id,sizeof(int)); 
		if (read_bytes != sizeof(int)) return -1;
	}
	/*close pipe*/
	close(StB_fd);
	return 0;
}


int write_message(int id,char* message,int BtS_fd){
	if (( write ( BtS_fd,&id,sizeof(int)) ) == -1){
		perror ( " Error in Writing ") ;
		return -1 ; 
	}
	if (( write ( BtS_fd,message,DATASIZE) ) == -1){
		perror ( " Error in Writing ") ;
		return -1 ; 
	}	
	return 0;
}


void sigpipe_handler(int signum){
	printf("Pipe is broken.Server is offline.Boardpost will now exit\n");
	exit(EXIT_SUCCESS);
}


