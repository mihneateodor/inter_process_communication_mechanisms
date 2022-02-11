#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


int main(){
	if(mkfifo("RESP_PIPE_18143",0600)!=0){
		printf("ERROR\ncannot create the response pipe\n");
		return 0;
	}

	int fdRead=-1, fdWrite=-1, fdSharedMemory=-1, fdFile=-1;

	fdRead=open("REQ_PIPE_18143", O_RDONLY);
	if(fdRead==-1){
		printf("ERROR\ncannot open the request pipe\n");
		return 0;
	}

	unsigned int sizeConnect=strlen("CONNECT"), sizeERROR=strlen("ERROR"), sizeSUCCESS=strlen("SUCCESS");

	fdWrite=open("RESP_PIPE_18143", O_WRONLY);
	// if(write(fdWrite,&sizeConnect,1)!=-1 && write(fdWrite,"CONNECT",sizeConnect)!=-1)
	// 	printf("SUCCESS");
	if(write(fdWrite,&sizeConnect,1)!=-1){
		if(write(fdWrite,"CONNECT",sizeConnect)!=-1){
			printf("SUCCESS\n");
		}
	}

	unsigned int size=0, create_shmSize=0, write_to_shmOffset=0, write_to_shmValue=0, map_fileSize=0,
	readFromFileOffset=0, readFromFileNoOfBytes=0;
	char * message=NULL, * sharedChar=NULL, * map_file=NULL, * dataRead=NULL;
	off_t sizeFile=0;

	unsigned int variant=18143;


	while(1){
		read(fdRead,&size,1);								//SIZE OF REQUEST MESSAGE
		message=(char *)malloc(size*sizeof(char));			//REQUEST MESSAGE ALLOCATION
		read(fdRead,message,size);							//REQUEST MESSAGE

		if(strcmp(message,"PING")==0){						//write : PING PONG 18143
			write(fdWrite,&size,1);							//size PING
			write(fdWrite,message,size);						//PING
			write(fdWrite,&size,1);							//size PONG
			write(fdWrite,"PONG",size);						//PONG
			write(fdWrite,&variant,sizeof(unsigned int));	// 18143
		}		

		if(strcmp(message,"CREATE_SHM")==0){	
			read(fdRead,&create_shmSize,sizeof(create_shmSize));				// size of shared memory
			fdSharedMemory=shm_open("/bu8a5zY1", O_CREAT | O_RDWR, 0664);		
			if(fdSharedMemory<0){
				write(fdWrite,&size,1);											//size CREATE_SHM
				write(fdWrite,message,size);									//CREATE_SHM
				write(fdWrite,&sizeERROR,1);									//size ERROR
				write(fdWrite,"ERROR",sizeERROR);								//ERROR
			}
			else{
				ftruncate(fdSharedMemory,create_shmSize);
				sharedChar=(char*)mmap(0, create_shmSize, PROT_READ | PROT_WRITE, MAP_SHARED,fdSharedMemory, 0);
				if(sharedChar!=(void*)-1){
					write(fdWrite,&size,1);										//size CREATE_SHM
					write(fdWrite,message,size);								//CREATE_SHM
					write(fdWrite,&sizeSUCCESS,1);								//size SUCCESS
					write(fdWrite,"SUCCESS",sizeSUCCESS);						//SUCCESS
				}
			}
			
		}

		if(strcmp(message,"WRITE_TO_SHM")==0){
			read(fdRead,&write_to_shmOffset,sizeof(write_to_shmOffset));		//OFFSET
			read(fdRead,&write_to_shmValue,sizeof(write_to_shmValue));			//VALUE
			if(0<=write_to_shmOffset && write_to_shmOffset<=create_shmSize && write_to_shmOffset+sizeof(write_to_shmValue)<=create_shmSize)
			{
					memcpy(sharedChar+write_to_shmOffset,&write_to_shmValue,sizeof(write_to_shmValue));
					write(fdWrite,&size,1);										//size WRITE_TO_SHM
					write(fdWrite,message,size);								//WRITE_TO_SHM
					write(fdWrite,&sizeSUCCESS,1);								//size SUCCESS
					write(fdWrite,"SUCCESS",sizeSUCCESS);						//SUCCESS					
			}
			else{
				write(fdWrite,&size,1);											//size WRITE_TO_SHM							
				write(fdWrite,message,size);									//WRITE_TI_SHM
				write(fdWrite,&sizeERROR,1);									//size ERROR
				write(fdWrite,"ERROR",sizeERROR);								//ERROR
			}
		}

		if(strcmp(message,"MAP_FILE")==0){
			read(fdRead,&map_fileSize,1);										//filename length	
			map_file=(char*)malloc(map_fileSize*sizeof(char));
			read(fdRead,map_file,map_fileSize);									//filename
			map_file[map_fileSize]='\0';										//end of string
			fdFile=open(map_file, O_RDONLY);									//open file
			if(fdFile<0){
				write(fdWrite,&size,1);											//size MAP_FILE
				write(fdWrite,message,size);									//MAP_FILE
				write(fdWrite,&sizeERROR,1);									//size ERROR
				write(fdWrite,"ERROR",sizeERROR);								//ERROR
			}
			else{
				sizeFile=lseek(fdFile,0,SEEK_END);						//size of file
				lseek(fdFile,0,SEEK_SET);										
				ftruncate(fdFile,sizeFile);										//redimensionare
				sharedChar=(char*)mmap(0,sizeFile, PROT_READ, MAP_SHARED,fdFile,0);		//mapare
				write(fdWrite,&size,1);											//size MAP_FILE
				write(fdWrite,message,size);									//MAP_FILE
				write(fdWrite,&sizeSUCCESS,1);									//size SUCCESS
				write(fdWrite,"SUCCESS",sizeSUCCESS);							//SUCCESS
			}
		}

		if(strcmp(message,"READ_FROM_FILE_OFFSET")==0){
			read(fdRead,&readFromFileOffset,sizeof(unsigned int));
			read(fdRead,&readFromFileNoOfBytes,sizeof(unsigned int));
			if(fdFile!=-1 && sharedChar!=(void*)-1 && readFromFileOffset+readFromFileNoOfBytes<sizeFile ){
				int count=0;
				for(int i=readFromFileOffset;i<=readFromFileOffset+readFromFileNoOfBytes;i++){
					dataRead[count++]=sharedChar[i];
				}
				dataRead[count]='\0';
				write(fdWrite,&size,1);
				write(fdWrite,message,size);
				write(fdWrite,&sizeSUCCESS,1);
				write(fdWrite,"SUCCESS",sizeSUCCESS);
			}
			else{
				write(fdWrite,&size,1);
				write(fdWrite,message,size);
				write(fdWrite,&sizeERROR,1);
				write(fdWrite,"ERROR",sizeERROR);
			}
		}

		if(strcmp(message,"EXIT")==0){
			unlink("RESP_PIPE_18143");
			close(fdWrite);
			close(fdRead);
			return 0;
		}
	}	

	return 0;
}