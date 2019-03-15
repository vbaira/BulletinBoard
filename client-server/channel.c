#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DATASIZE 200
#define NAMESIZE 20




typedef struct Channel{
	int id;	
	char name[NAMESIZE];
	struct Channel_node* start;
	struct Channel* next;
}Channel;


typedef struct Channel_node{
	char type;//f for file,m for message
	char data[DATASIZE];//the actual message,or file path if file
	struct Channel_node* next;
}Channel_node;


Channel* GetNext(Channel* chnl){
	return chnl->next;
}


char* GetName(Channel* chnl){
	return chnl->name;
}

int GetId(Channel* chnl){
	return chnl->id;
}

//searches for channel with id <id>.
//if found returns pointer to it .If not returns NULL
Channel* Search_channel(int id,Channel* chnl){
	while (chnl!=NULL){
		if(chnl->id==id) return chnl;
		else chnl=chnl->next;
	}
	return NULL;	
}

//add a new channel
void Add_channel(Channel** chnl,int id,char* name){
	if (id<0){//channel id must be positive
		printf("failure: channel id must be > 0\n");
		return;
	}
	Channel* found=NULL;
	found=Search_channel(id,*chnl);
	if (found == NULL){
		Channel* new=malloc(sizeof(Channel));
		new->id=id;
		strcpy(new->name,name);
		new->start=NULL;
		new->next=*chnl;
		*chnl=new;
		printf("channel with id:%d and name:%s created\n",id,name);
	}
	else printf("failure:channel with id:%d already exists\n",id);			
}



//insert new channel node at the end of channel <chnl>
void Push(Channel* chnl,char* data,char type){
	Channel_node* temp=chnl->start;
	if(temp==NULL){//if empty insert at start
		Channel_node* new = malloc(sizeof(Channel_node));
		new->type=type;
		strcpy(new->data,data);
		new->next=NULL;
		chnl->start=new;	
	}
	else{
		while (temp->next!=NULL){
			temp=temp->next;
		}
		Channel_node* new = malloc(sizeof(Channel_node));
		new->type=type;
		strcpy(new->data,data);
		new->next=NULL;
		temp->next=new;
	}
}

//pop the first channel node and put its data into <data>
//returns the f if file,m if message,e if empty
char Pop(Channel *chnl,char * data){
	if (chnl->start==NULL){//if empty
		return 'e';//indicates empty queue
	}
	else{
		Channel_node* todel=chnl->start;
		strcpy(data,todel->data);
		char ret=todel->type;
		chnl->start=todel->next;
		free(todel);
		return ret;
	}
}

//destroy all channels(for shutdown)
void Destroy_channels(Channel* chnl){
	char data[DATASIZE];
	char type;
	Channel* todel;
	while(chnl != NULL){
		while((type=Pop(chnl,data)) != 'e'){
			if (type=='f') remove(data);
		}
		todel=chnl;
		chnl=chnl->next;
		free(todel);
	}
}


