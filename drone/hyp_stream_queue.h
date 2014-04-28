
#define HYP_STREAM_QUEUE_NUMBER  ((DroneStreamQueueSize - sizeof(HYP_STREAM_QUEUE_HEAD))/sizeof(HYP_STREAM_QUEUE));

typedef struct{
        char                      tid[32];
		uint32_t                  tnumber;
		HYP_CONN_QUEEN_SOCKET hcqs;
}HYP_STREAM_QUEUE;


typedef struct{
	pthread_spinlock_t        spinLock;
    uint32_t                  usableUnit;
}HYP_STREAM_QUEUE_HEAD; 

HYP_STREAM_QUEUE_HEAD * hyp_queue_init();
int hyp_queue_insert(HYP_STREAM_QUEUE_HEAD *,char * ,unsigned int ,HYP_STREAM_QUEUE **);
int hyp_queue_destroy(HYP_STREAM_QUEUE_HEAD *,char *,unsigned int);