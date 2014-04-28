

#include "connect.h"

#include "common.h"

#include "hyp_conn_queen.h"

#include <linux/tcp.h>

#include "cJSON.h"

#include "exec_thread.h"

//check Mac exists ? exists: 1  ; not exists: 0
int check_Mac_exists(uint32_t number,uint32_t * lp_Macs){
    
    debug_msg("checking exists mac(total %i): \n",number);
	
	char str[11];

	for (;number >0 ;number -- ){
		uint32_t key = *lp_Macs;
        
		debug_msg("checking exists mac(%i): %i \n",number,*lp_Macs);
	    
		lp_Macs ++;

		int sem_id = semget(key,1,IPC_CREAT | IPC_EXCL | 0600);

		if (-1 == sem_id){
			debug_msg("semget fail ! key: %d , Mac exists...\n",key);
			return 1;
			//break;
		}else{
			union semun sem_union;
			semctl(sem_id, 0, IPC_RMID, sem_union);
			debug_msg("semget success ! key: %d shmid: %d,Mac not exists\n",key,sem_id);
            
			//add exists array to GET buff
            sprintf (str,"%d",key);
			strcat (lp_get_buff,"&exists[]=");
			strcat (lp_get_buff,str);

		}	
	}

	debug_msg("Get Buf: %s \n",lp_get_buff);
	return 0;				    
}



//ret:  0  ok
//     -1  end current connect
int request_router(){

	//比对request_header 
	//1. 是否为系统(保留)包头 (compare c_header.Module)
	//2. 在module列表中比对，有相符的分发处理之(create new thread)

	debug_msg ("\n Router said: ");
    
    if ((255 >= c_header.Module_NO[0]) && (0 == c_header.Module_NO[1]) && (0 == c_header.Module_NO[2]) \
		                               && (0 == c_header.Module_NO[3])){ // 系统指令 -》 无TID,无返回

        if (c_header.Request_size > RequestBuffSize){ //Request len more than Buf size,drop it
		    debug_msg("Request len more than Buf size,drop it");		
		}else{
			switch (c_header.Module_NO[0]){
				case 1:  //呼吸握手request
					
					debug_msg(" shake hand package,cmd: %i \n",c_header.Flag[0]);

					if (0 == c_header.Flag[0]){        //online
					    
						DroneStatus = -1;
						if (4 == c_header.Request_size){
                            debug_msg ("\n * online...DroneID: %i \n",* (int *)lp_buff_read);
							DroneID = * (uint32_t *)lp_buff_read;
							if (DroneID > 0){
								DroneStatus = 1;
							}else{
							    debug_msg ("\n DroneID !> 0  \n");
							}                            
						}else{
							 debug_msg ("\n DroneID fail to take... \n");	
							 return -1;
						    
						}
					}else if (1 == c_header.Flag[0]){  //error
						return -1;
					}else if (2 == c_header.Flag[0]){  //exists mac
					    debug_msg(" need to check exists mac: %s \n",lp_buff_read);
						
						if (c_header.Request_size > 400){ //too many exists Macs
						    if (-4 != DroneStatus)
								DroneStatus = -3;
						}else if (1 == check_Mac_exists(c_header.Request_size / 4,(uint32_t * )lp_buff_read)){
							if (-4 != DroneStatus)
								DroneStatus = -3;
						}else{
						    DroneStatus = 2;
						}
					}
					break;
				default: //未定义的系统保留request
					break;
			}

			debug_msg(" sys remaining request,No: %i \n",c_header.Module_NO[0]);
		}
    }else{ // 模块指令
	    if (c_header.Request_size > RequestBuffSize){ //Request len more than Buf size,drop route and respond it
	        debug_msg("Request len more than Buf size,drop route and respond it");
			newthread_creater((char *)&(c_header.Flag),0,0,2,0);
		}else{
			void * mod_exec = NULL;
			MODULE_LIST * c_ModuleList = DroneModuleList;
			debug_msg(" compare ... module List ... seek to : %i.%i.%i.%i \n",c_header.Module_NO[0],c_header.Module_NO[1],c_header.Module_NO[2],c_header.Module_NO[3]);
			while ((0 != c_ModuleList->Module_NO[0]) || (0 != c_ModuleList->Module_NO[1]) || (0 != c_ModuleList->Module_NO[2]) || 
				   (0 != c_ModuleList->Module_NO[3])){
				if (0 == memcmp(&(c_ModuleList->Module_NO[0]),&c_header.Module_NO[0],16)){
					mod_exec = c_ModuleList->Module_Func;
                    debug_msg("Ok, module has been found !! \n");
					break;
				}
				c_ModuleList ++;             
			}
			debug_msg("execute addr: %p ,RData: %s \n",mod_exec,lp_buff_read); 
			if (mod_exec){
                newthread_creater((char *)&(c_header.Flag),lp_buff_read,c_header.Request_size,0,mod_exec);
			}else{
			    debug_msg("Fail, module not found !! \n");
				newthread_creater((char *)&(c_header.Flag),0,0,1,0);
			}            
		}
	}



	return 0;

}

int request_parser(){
	
	while (dw_buff_available >= parser_wire){
		if (((char)0x00 == *lp_buff_read) && ((char)0xFF == *(lp_buff_read + 0x11)) && ((char)0xFF == *(lp_buff_read + 0x11 + 0x21))){
			debug_msg ("\n Found Header!!! \n");
			debug_msg("parser_wire: %i \n lp_buff_write: %p \n lp_buff_read: %p \n dw_buff_available: %i \n dw_buff_remained: %i \n ",parser_wire,lp_buff_write,lp_buff_read,dw_buff_available,dw_buff_remained);

			memcpy (&c_header,lp_buff_read,sizeof(REQUEST_HEADER_QUEEN));

			dw_buff_available -= sizeof(REQUEST_HEADER_QUEEN) ; 
			lp_buff_read += sizeof(REQUEST_HEADER_QUEEN);		

			debug_msg ("current request len: %i \n",c_header.Request_size);
			
			if (c_header.Request_size <= dw_buff_available){
				parser_step = 2;
				debug_msg ("request posited same step,len: %i contents: %s \n",c_header.Request_size,lp_buff_read);
			}else if (c_header.Request_size > RequestBuffSize){ //Request len more than Buf size,drop it
				parser_step = 2;	
				debug_msg ("request len more than Buf size,drop it in Route func, len: %i \n",c_header.Request_size);
			}else{
				parser_step = 1;
				parser_wire = c_header.Request_size;
			}		
			break;
		}else{
			dw_buff_available --; 
			lp_buff_read ++;		
		}
	}

    return 0;
}

int reset_buff(int Brutal){

    //debug_msg("parser_wire: %i \n lp_buff_base: %p \n lp_buff_write: %p \n lp_buff_read: %p \n dw_buff_available: %i \n dw_buff_remained: %i \n ",parser_wire,lp_buff_base,lp_buff_write,lp_buff_read,dw_buff_available,dw_buff_remained);
    
    if (1 == Brutal){ //强制清空当前缓存
		debug_msg("\n!reset...buffer..BRUTAL \n");
		lp_buff_write     = lp_buff_base;
		lp_buff_read      = lp_buff_base;
		dw_buff_remained  = RequestBuffSize;	
        dw_buff_available = 0;
	}else{
		debug_msg("\n!reset...buffer \n");
		memcpy (lp_buff_base,lp_buff_read,dw_buff_available);
		
		lp_buff_write     = lp_buff_base + dw_buff_available;
		lp_buff_read      = lp_buff_base;
		dw_buff_remained  = RequestBuffSize - dw_buff_available;
	}
	return dw_buff_remained;
}


int connect_breath_srv(char *lp_submitBuff){

    //Get buff
	lp_get_buff = lp_submitBuff;

	//ready buff
	lp_buff_base = malloc(RequestBuffSize);

	if (NULL == lp_buff_base){
	    debug_msg("fatal error: malloc request buff fail !\n");
		DroneStatus = -9;
		return -1;
	}else{
		//init buff values
		memset(lp_buff_base,0,RequestBuffSize);
		lp_buff_write = lp_buff_base;
		lp_buff_read  = lp_buff_base;
		dw_buff_available = 0;
        dw_buff_remained  = RequestBuffSize;
	}
  
    //init
	parser_wire = sizeof(REQUEST_HEADER_QUEEN);
	parser_step = 0;

	int n = 1;

    HYP_CONN_QUEEN_SOCKET breath_hcqs;
    if (0 == hyp_conn_queen_init(lp_submitBuff,0,0,&breath_hcqs,1)){
	    while ((n = hyp_conn_queen_read(&breath_hcqs,lp_buff_write, dw_buff_remained)) > 0){
			debug_msg("\n< DroneStatus: %i <", DroneStatus);			
            
			if (-9 == DroneStatus){
				break;
			}

		    lp_buff_write += n;
			dw_buff_available += n;
			dw_buff_remained  -= n;				
			
			if (dw_buff_available >= parser_wire){

				if (0 == parser_step){
					request_parser();
				}else{ //if (1 == parser_step){
					parser_step = 2;
				}
				
				if (2 == parser_step){
					if (-1 == request_router()){
						break; //sys quit...
					}
					parser_wire = sizeof(REQUEST_HEADER_QUEEN);
					parser_step = 0;
				}
			}

			if (0 == dw_buff_remained){
				if (0 == reset_buff(0)){
					reset_buff(1);
				}
			}
		}
		hyp_conn_queen_close(&breath_hcqs);
	}

    //free request buff
    free(lp_buff_base);

	//set offline flag
	switch (DroneStatus)
	{
	case 1:
	case 0:
		DroneStatus = -1;
	    break;
    default:
        break;	
	}
	
	return 0;
}


int init_srv(){
    
    char srv_init[1024] = {0};

	char GetData[256] = {0};
	strcpy (GetData,QueenHostDir);
	strcat (GetData,"init.php");

	int n = 0;

	int ret = 1;

    HYP_CONN_QUEEN_SOCKET init_hcqs;
    if (0 == hyp_conn_queen_init(GetData,0,0,&init_hcqs,1)){
		if ((n = hyp_conn_queen_read(&init_hcqs,srv_init,1024)) >= 12){
			uint32_t *p = (uint32_t *) &srv_init ;
			ResponseUnitSize = *p;
			p ++;
			RequestBuffSize = *p * 2;
			p ++;
			ResponseMaxSize = *p;
			ret = 0;
		}else{
		    debug_msg ("init_srv fail: %i \n",n);
		}
		hyp_conn_queen_close(&init_hcqs);
	}
	return ret;
}
