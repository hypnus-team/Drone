
#include "common.h"
#include <string.h> 
#include "cJSON.h"
#include <pthread.h>
#include "hyp_conn_queen.h"
#include "hyp_stream_queue.h"
#include "exec_thread.h"

int ReadStreamFunc(REQUEST_HEADER * RequestHeaderPtr,int tNumber,void * buff,int buffSize){
    int ret = -1;
	HYP_STREAM_QUEUE * ptr = 0;
	if (RequestHeaderPtr->StreamStruct){
		if (0 == hyp_queue_insert(RequestHeaderPtr->StreamStruct,RequestHeaderPtr->Tid,tNumber,&ptr)){
			ret = hyp_conn_queen_read(&ptr->hcqs,buff,buffSize);
			if (ret <= 0){
                hyp_queue_destroy(RequestHeaderPtr->StreamStruct,RequestHeaderPtr->Tid,tNumber);
			}
		}
	}
	return ret;
}

void * execfunc_caller(void * tmp){
    REQUEST_HEADER * buff = tmp;
    debug_msg ("NewThread: request len: %i contents: %.*s  addr: %p \n",buff->RDataSize,buff->RDataSize,buff->RData,buff->RData);
	debug_msg ("NewThread: task ID: %.*s \n",32,buff->Tid);
	debug_msg ("NewThread: ExecFunc: %p \n",buff->ExecCallFunc);

	if (0 == buff->RStatus){		
		//call module->exec
		void (*exec) (char *); 
		exec = buff->ExecCallFunc;
		//init HYP_STREAM_QUEUE
		buff->StreamStruct = hyp_queue_init();		
		//exec mod
		exec((char *)buff);		
	}else{ //error status
        debug_msg ("buff->RStatus: %i \n",buff->RStatus);
	    response_sender(buff,0,0);
	}

	if (buff->lpJsonRoot){
		cJSON_Delete(buff->lpJsonRoot);
		debug_msg ("cJSON_Delete invoked \n");
    }

	if (buff->StreamStruct){
		hyp_queue_destroy(buff->StreamStruct,0,0);
	}
	
	free(buff);
	return 0;
}



int newthread_creater(char * TId,char * RData,uint32_t RDataSize,uint32_t RStatus,void * ExecCallFunc){
    
    void * buff = malloc(1024 * (1 + ((sizeof(REQUEST_HEADER) + RDataSize + 1) / 1024)));
	if (buff){	
		memset(buff,0,1024 * (1 + ((sizeof(REQUEST_HEADER) + RDataSize + 1) / 1024)));
		REQUEST_HEADER * Buf = (REQUEST_HEADER * )buff;
		Buf->RHeaderSize = sizeof(REQUEST_HEADER);
		memcpy ((void *)(Buf->Tid),TId,32);
		Buf->ExecCallFunc = ExecCallFunc;
		Buf->RData = (void *)(buff + sizeof(REQUEST_HEADER));
		Buf->RDataSize = RDataSize;
		Buf->RStatus = RStatus;
		if (RDataSize){
			memcpy (Buf->RData,RData,RDataSize + 1);
		}
		Buf->ResponseSender = (int *)&response_sender;
		Buf->ResponseStreamSender = (int *)&response_stream_sender;
		Buf->cJSON_GetObjectItem = (cJSON *)&cJSON_GetObjectItem;
		Buf->lpJsonRoot = 0;
		Buf->ReadStreamFunc = (int *)&ReadStreamFunc;
		Buf->StreamStruct   = 0;

		
		char *ep;
		if (RDataSize){
			cJSON * jsonRoot = cJSON_Parse(Buf->RData,(const char **)&ep);
			if (!jsonRoot){
				debug_msg ("cJSON_Parse fail : %s \n",ep);
			}
			Buf->lpJsonRoot = jsonRoot;
		}

		if ((0 == Buf->RStatus) && (!ExecCallFunc)){
			debug_msg ("ExecCallFunc is Nil,buff->RStatus: %i \n",Buf->RStatus);
			Buf->RStatus = 7;			
		}

		pthread_t tid;		
		pthread_create(&tid,NULL,execfunc_caller,buff); 
		pthread_detach(tid);  
    }else{
	    debug_msg ("newthread_creater malloc fail : size: %d \n",1024 * (1 + ((sizeof(REQUEST_HEADER) + RDataSize + 1) / 1024)));
	}
    return 0;
}

//ret: 0 ok .. task accomplished
//     1 ok .. continue
//   < 0 fail
int response_stream_sender(REQUEST_HEADER * request,size_t streamSize,size_t responseLen,char* responseBuff){    
	int ret = 0;
    char GetData[255] = {0};
	int c_sent_size = responseLen;

    sprintf (GetData,"%s""response.php?status=%i&tid=%.*s&size=%zu&cid=%i",QueenHostDir,request->RStatus,32,request->Tid,streamSize,DroneID);
    
	if ((0 < ResponseMaxSize) && (ResponseMaxSize < responseLen)){ // more than Max response size ,give up
		return 0;
	}   

    while (responseLen > 0){
		c_sent_size = responseLen;
        
		if ((0 < ResponseUnitSize) && (ResponseUnitSize < responseLen)){ //more than Max post size ,chunk it
		    c_sent_size = ResponseUnitSize;
		}   
		responseLen  -= c_sent_size;	   
		ret = send_stream_data_to_queen(c_sent_size,responseBuff,GetData);
		responseBuff += c_sent_size;
		if (1 != ret){
			break;
		}
	}
    return ret;	
}
//ret: 0 ok .. task accomplished
//     1 ok .. continue
//   < 0 fail
int send_stream_data_to_queen(int responseLen,char* responseBuff,char * httpHead){
    HYP_CONN_QUEEN_SOCKET send_hcqs;
    int ret = 1;
	debug_msg ("response stream sender[%i] : %s \n",responseLen,httpHead);
	if (0 == hyp_conn_queen_init(httpHead,responseBuff,responseLen,&send_hcqs,1)){	    
		if (0 <= hyp_conn_queen_read(&send_hcqs,(char *)&ret,1)){
			ret -= 0x30;
		}else{
		    ret = -1;
		}
		hyp_conn_queen_close(&send_hcqs);
	}else{
	    ret = -1;
	}
   debug_msg ("Queen return: [%i] \n",ret);
   return ret;
}

int response_sender(REQUEST_HEADER * request, size_t responseLen,char* responseBuff){

    char GetData[255] = {0};
    
    if ((0 < ResponseUnitSize) && (ResponseUnitSize < responseLen)){      // more than Max post size ,ret status = (-)6
		sprintf (GetData,"%s""response.php?status=6&tid=%.*s&cid=%i",QueenHostDir,32,request->Tid,DroneID);
		responseBuff = 0;
		responseLen  = 0;
    }else if ((0 < ResponseMaxSize) && (ResponseMaxSize < responseLen)){ // more than Max response size ,ret status = (-)5
        sprintf (GetData,"%s""response.php?status=5&tid=%.*s&cid=%i",QueenHostDir,32,request->Tid,DroneID);
		responseBuff = 0;
		responseLen  = 0;		
	}else{ 
       	sprintf (GetData,"%s""response.php?status=%i&tid=%.*s&cid=%i",QueenHostDir,request->RStatus,32,request->Tid,DroneID);
	}	
    return	send_data_to_queen(responseLen,responseBuff,(char *)&GetData);
}

int send_data_to_queen(int responseLen,char* responseBuff,char * httpHead){
    HYP_CONN_QUEEN_SOCKET send_hcqs;
    char tmp;
	int ret = -1;
	debug_msg ("response sender[%i] : %s \n",responseLen,httpHead);
	if (0 == hyp_conn_queen_init(httpHead,responseBuff,responseLen,&send_hcqs,0)){	    
		if (0 <= hyp_conn_queen_read(&send_hcqs,&tmp,1)){
            ret = 0;
		}
		hyp_conn_queen_close(&send_hcqs);
	}
	return ret;
}
