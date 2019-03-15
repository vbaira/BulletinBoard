#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>
#include "channel.h"

#define CMDSIZE 3
#define NAMESIZE 20//size of channel name
#define DATASIZE 200//size of data stored in server(messages or file paths)
#define BUFFERSIZE 2048

void server_shutdown(char* path,struct Channel* channels,int BtS_fd,int CtS_fd,int StB_fd,int StC_fd);
int server_createchannel(int CtS_fd,struct Channel** channels);
int server_getmessages(int CtS_fd,int Stc_fd,struct Channel* channels);
int server_list(char* path,struct Channel* channels);
int server_write(int BtS_fd,struct Channel* channels);
int server_send(int BtS_fd,struct Channel* channels,char* path);

void server(char* path){
	struct Channel* channels=NULL;//pointer to the first channel
	/*Open all the necessary pipes*/
	int BtS_fd,CtS_fd,StB_fd,StC_fd;
	char fifoname[100];
	strcpy(fifoname,path);
	strcat(fifoname,"/StC");
	if (( StC_fd = open (fifoname, O_WRONLY)) < 0){
		perror("fifo open error");
		exit (EXIT_FAILURE) ;
	}
	strcpy(fifoname,path);
	strcat(fifoname,"/CtS");
	if (( CtS_fd = open (fifoname, O_RDONLY | O_NONBLOCK)) < 0){
		perror("fifo open error");
		exit (EXIT_FAILURE) ;
	}
	strcpy(fifoname,path);
	strcat(fifoname,"/BtS");
	if (( BtS_fd = open (fifoname, O_RDONLY | O_NONBLOCK)) < 0){
		perror("fifo open error");
		exit (EXIT_FAILURE) ;
	}

	/*read commands from pipes,all commands are 3 char long*/
	char cmd[CMDSIZE+1];	
	while(1){
		/*read commands from client*/
		int read1=read(CtS_fd,cmd,sizeof(cmd)); 
        if (read1 == CMDSIZE+1) {
            printf("<BOARD SERVER> Message Received: %s\n", cmd);
            fflush(stdout);

			if (strcmp(cmd,"CRT")==0){
				if(server_createchannel(CtS_fd,&channels) < 0){
					printf("<BOARD SERVER> Createchannel failed,server exiting\n");
					exit(EXIT_FAILURE);
				}
			}
			if (strcmp(cmd,"GET")==0){
				if(server_getmessages(CtS_fd,StC_fd,channels) < 0){
                    printf("<BOARD SERVER> Getmessages failed,server exiting\n");
					exit(EXIT_FAILURE);
                }
			}
			if (strcmp(cmd,"SHT")==0){			
				server_shutdown(path,channels,BtS_fd,CtS_fd,StB_fd,StC_fd);
			}
        }	


	
		/*read commands from boardpost*/
        int read2=read(BtS_fd,cmd,sizeof(cmd));        
        if (read2 == CMDSIZE+1) {
            printf("<BOARD SERVER> Message Received: %s\n", cmd);
            fflush(stdout);
       
			if (strcmp(cmd,"LST")==0){
				if (server_list(path,channels) < 0){
					printf("<BOARD SERVER> List failed,server exiting\n");
					exit(EXIT_FAILURE);
				}
			}
			if (strcmp(cmd,"WRT")==0){
				if(server_write(BtS_fd,channels) < 0 ){
					printf("<BOARD SERVER> Write message failed,server exiting\n");
					exit(EXIT_FAILURE);
				}
			}
			if (strcmp(cmd,"SND")==0){
				if (server_send(BtS_fd,channels,path) < 0 ){
					printf("<BOARD SERVER> Send file failed,server exiting\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		
        usleep(100000);	
	}	
}



//receive file from boardpost
int server_send(int BtS_fd,struct Channel* channels,char* path){
	int id;
	char filename[DATASIZE];
	/*uset NON_BLOCK flag*/
	if (fcntl(BtS_fd,F_SETFL,O_RDONLY) < 0) return -1;
	/*read id and filename from boardpost*/
	if (( read ( BtS_fd,&id,sizeof(int)) ) == -1){
		perror ( " Error in Reading ") ;
		return -1 ; 
	}
	if (id<0){
		printf("<BOARD SERVER> error getting file from boardpost\n");
		fcntl(BtS_fd,F_SETFL, O_RDONLY | O_NONBLOCK);//re-set NON_BLOCK flag
		return 0;
	}
	if (( read ( BtS_fd,filename,sizeof(filename)) ) == -1){
		perror ( " Error in Writing ") ;
		return -1 ; 
	}
	/*get file size from boardpost*/
	int file_size;
	if ((read( BtS_fd,&file_size,sizeof(int)) ) == -1){
		perror ( " Error in Writing ") ;
	    return -1 ; 
    }  
	/*create new file*/
	int file_fd;
	char filepath[DATASIZE];
	strcpy(filepath,path);
	strcat(filepath,"/");
	strcat(filepath,filename); 
	while((file_fd=open(filepath,O_EXCL | O_CREAT | O_WRONLY,0755))<0){
		if (errno==EEXIST){
			strcat(filepath,"_new");
			//file_fd=open(filepath,O_CREAT | O_WRONLY,0755);
		}
		else{
			perror("<BOARD SERVER>Error in opening ");
			return -1;
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
		/*if left_to_read bytes >= BUFFERSIZE then read BUFFERSIZE chunks
		else read what is left*/
		if (left_to_read >= BUFFERSIZE){
		    if ((read_bytes=read(BtS_fd,buffer,BUFFERSIZE) ) == -1){
				perror ("Error in reading client") ;
				return -1; 
			}
		}
		else{
			 if ((read_bytes=read(BtS_fd,buffer,left_to_read) ) == -1){
				perror ("Error in reading client") ;
				return -1; 
			}
		}
		if ((write_bytes=write(file_fd,buffer,read_bytes) ) == -1){
			perror ("Error in writing client") ;
			return -1; 
		}
		read_bytes_total+=read_bytes;
		write_bytes_total+=write_bytes;
		left_to_read-=read_bytes; 
  	}
	close(file_fd);
	if(write_bytes_total==file_size){
		printf("<BOARD SERVER>file %s received successfully\n",filename);
	}
	else{
		printf("<BOARD SERVER>file %s received unsuccessfully\n",filename);
	}
	/*push file to channel*/
	/*search channel with id*/
	struct Channel* wanted_channel=NULL;
	wanted_channel=Search_channel(id,channels);
	if (wanted_channel==NULL){//if not found
		remove(filepath);//remove received file 
		printf("<BOARD SERVER> Channel with id:%d not found\n", id);
	}
	else{
		Push(wanted_channel,filepath,'f');
	}
	fcntl(BtS_fd,F_SETFL, O_RDONLY | O_NONBLOCK);//re-set NON_BLOCK flag
	return 0;
}



//receive message from boardpost
int server_write(int BtS_fd,struct Channel* channels){
	int id;
	char message[DATASIZE];
	/*uset NON_BLOCK flag*/
	if (fcntl(BtS_fd,F_SETFL,O_RDONLY) < 0) return -1;
	/*read channel id and message from boardpost*/
	int read_bytes=read(BtS_fd,&id,sizeof(int)); 
    if (read_bytes != sizeof(int)) return -1;
	read_bytes=read(BtS_fd,message,DATASIZE);
	if (read_bytes != DATASIZE) return -1;
	/*search for channel with the given id*/
	struct Channel* wanted_channel=NULL;
	wanted_channel=Search_channel(id,channels);
	if (wanted_channel==NULL){//if not found 
		printf("<BOARD SERVER> Channel with id:%d not found\n", id);
		fcntl(BtS_fd,F_SETFL, O_RDONLY | O_NONBLOCK);//re-set NON_BLOCK flag
		return 0;
	}
	else{//if found
		Push(wanted_channel,message,'m');
		fcntl(BtS_fd,F_SETFL, O_RDONLY | O_NONBLOCK);//re-set NON_BLOCK flag
		return 0;
	}
}


//send names and ids of channels at boardpost
int server_list(char* path,struct Channel* channels){
	/*open pipe to write to boardpost*/
	int StB_fd;
	char fifoname[100];
	strcpy(fifoname,path);
	strcat(fifoname,"/StB");
	if (( StB_fd = open (fifoname,O_WRONLY)) < 0){
		perror("fifo open error");
		return -1 ;
	}
	/*get names and ids of channels and send them to boardpost*/
	char name[NAMESIZE];
	int id;
	struct Channel* temp=channels;
	while (temp != NULL){
		id=GetId(temp);
		strcpy(name,GetName(temp));
		if (( write ( StB_fd,&id,sizeof(int)) ) == -1){
			perror ( " Error in Writing ") ;
			return -1 ; 
		}
		if (( write ( StB_fd,name,NAMESIZE) ) == -1){
			perror ( " Error in Writing ") ;
			return -1 ; 
		}
		temp=GetNext(temp);
	}
	/*inform boardbost that no more channels exist */
	id=-1;
	if (( write ( StB_fd,&id,sizeof(int)) ) == -1){
		perror ( " Error in Writing ") ;
		return -1 ; 
	}
	/*close pipe*/
	close (StB_fd);
	return 0;
}

//send the contents of a channel to client
int server_getmessages(int CtS_fd,int StC_fd,struct Channel* channels){
	int id ;
	if (fcntl(CtS_fd,F_SETFL,O_RDONLY) < 0) return -1;//uset NON_BLOCK flag
	int read_bytes=read(CtS_fd,&id,sizeof(int));//get channel id
	if (read_bytes != sizeof(int)) return -1;
	struct Channel* wanted_channel=NULL;
	wanted_channel=Search_channel(id,channels);
	if (wanted_channel==NULL){//if not found 
		printf("<BOARD SERVER> Channel with id:%d not found\n", id);
		fcntl(CtS_fd,F_SETFL, O_RDONLY | O_NONBLOCK);
		char empty='e';
		if (( write(StC_fd,&empty,sizeof(char)) ) == -1){//send 'e' to client,to notify about empty channel
			perror ( " Error in Writing ") ;
			return -1 ; 
		}
		return 0;	
	}
	else{//if found
		char type;
		char data[DATASIZE];
		while((type=Pop(wanted_channel,data)) != 'e' ){//while channel is not empty
			if (( write ( StC_fd,&type,sizeof(char)) ) == -1){//send type of data to the client
				perror ( " Error in Writing ") ;
				return -1 ; 
			}
			if (type=='m'){//if message
				if (( write ( StC_fd,data,DATASIZE) ) == -1){
					perror ( " Error in Writing ") ;
					return -1 ; 
				}
			}
			else{//if file
				/*send file name to client*/ 
                char filename[DATASIZE];
                strcpy(filename,basename(data));
                 if (( write ( StC_fd,filename,sizeof(filename)) ) == -1){
				    perror ( " Error in Writing ") ;
				    return -1 ; 
			    }
				/*get file size ,send it to client*/                     
                struct stat results;          
                if (stat(data,&results) < 0){
                    perror ("Error in stat") ;
					return -1 ;         
                }
                int file_size=results.st_size;
                if ((write(StC_fd,&file_size,sizeof(int)) ) == -1){
				    perror ( " Error in Writing ") ;
				    return -1 ; 
			    }
				/*open file*/
				int fd;
				if((fd=open(data,O_RDONLY))<0){
					perror("error in oppening");
					return -1;
				}
				/*read file chunks and send them to client*/
                char buffer[BUFFERSIZE];
				int read_bytes=0;                
                int write_bytes_total=0;
				int write_bytes=0;
                while (write_bytes_total < file_size ){
					if ((read_bytes=read(fd,buffer,BUFFERSIZE)) == -1){
						perror ( " Error in Reading ") ;
				        return -1 ; 
					}
                    if ((write_bytes=write ( StC_fd,buffer,read_bytes)) == -1){
				        perror ( " Error in Writing ") ;
				        return -1 ; 
			        }
					write_bytes_total+=write_bytes;   
                }
                if (write_bytes_total == file_size) printf("<BOARD SERVER> file transfer complete\n");
				/*remove file*/
                close(fd);
                if(remove(data) < 0){
                    perror ("Error in remove") ;
					return -1 ; 
                }                   
		    }
		}		
	}
	char empty='e';
	if (( write(StC_fd,&empty,sizeof(char)) ) == -1){//send 'e' to client,to notify about empty channel
		perror ( " Error in Writing ") ;
		return -1 ; 
	}	
	if (fcntl(CtS_fd,F_SETFL, O_RDONLY | O_NONBLOCK)<0) return -1;//re-set NON_BLOCK flag
	return 0;
}


//create a new channel
int server_createchannel(int CtS_fd,struct Channel** channels){
	int id;
	char name[NAMESIZE];
	if (fcntl(CtS_fd,F_SETFL,O_RDONLY) < 0) return -1;//uset NON_BLOCK flag
	int read_bytes=read(CtS_fd,&id,sizeof(int)); //get channel id
    if (read_bytes != sizeof(int)) return -1;
	read_bytes=read(CtS_fd,name,NAMESIZE); //get channel name
	if (read_bytes != NAMESIZE) return -1;
	if (fcntl(CtS_fd,F_SETFL, O_RDONLY | O_NONBLOCK)<0) return -1;//re-set NON_BLOCK flag
	Add_channel(channels,id,name);  //add the channel
	return 0;
}


//server shutdown
void server_shutdown(char* path,struct Channel* channels,int BtS_fd,int CtS_fd,int StB_fd,int StC_fd){
	/*destroy channels,delete files on channels*/
	Destroy_channels(channels);
	/*close file descs*/
	close(BtS_fd);
	close(CtS_fd);
	close(StC_fd);
	/*exit*/
	exit(EXIT_SUCCESS);
}
