#pragma pack(1)
typedef struct{
	uint32_t RHeaderSize;
	char     Tid[32];
	void *   RData;
	uint32_t RDataSize;
    uint32_t RStatus;
	void *   ExecCallFunc;
	void *   ResponseSender;
	void *   ResponseStreamSender;
	void *  cJSON_GetObjectItem;
	cJSON *  lpJsonRoot;
	void *   ReadStreamFunc;
	void *   StreamStruct;
}REQUEST_HEADER;
#pragma pack()

int response_sender(REQUEST_HEADER *, size_t,char* );
int response_stream_sender(REQUEST_HEADER *,size_t,size_t,char*);
int newthread_creater(char * TId,char * RData,uint32_t RDataSize,uint32_t RStatus,void * ExecCallFunc);
int send_stream_data_to_queen(int responseLen,char* responseBuff,char * httpHead);
int send_data_to_queen(int responseLen,char* responseBuff,char * httpHead);