
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "common.h"
#include "hyp_conn_queen.h"
#include "hyp_stream_queue.h"


HYP_STREAM_QUEUE_HEAD * hyp_queue_init(){ 
    HYP_STREAM_QUEUE_HEAD * lpQueue = malloc (DroneStreamQueueSize);  
	if (lpQueue){
		memset((void *)lpQueue,0,DroneStreamQueueSize);	
		pthread_spin_init(&lpQueue->spinLock, 0);
		lpQueue->usableUnit = HYP_STREAM_QUEUE_NUMBER;
	}
	return lpQueue;
}

// 0 success
//-1 conn queen fail
//-2 queue full
//-3 illegal tid or tNumber
int hyp_queue_insert(HYP_STREAM_QUEUE_HEAD * lpQueue,char * tid,unsigned int tNumber,HYP_STREAM_QUEUE ** retPtr){
	int ret = -1;
	if (lpQueue->usableUnit > 0){		
		//check exists
		int used_unit  = HYP_STREAM_QUEUE_NUMBER;
		    used_unit -= lpQueue->usableUnit;
		HYP_STREAM_QUEUE * lpCurrentQueueUnit = (HYP_STREAM_QUEUE *)(lpQueue + sizeof(HYP_STREAM_QUEUE_HEAD));
		while (used_unit > 0){
            if (lpCurrentQueueUnit->hcqs.fd){
				if ((0 == memcmp(&lpCurrentQueueUnit->tid,tid,32)) && (lpCurrentQueueUnit->tnumber == tNumber)){ //exists found!
					debug_msg("exists found!!!");
					* retPtr = lpCurrentQueueUnit;
					ret = 0;
					break;
				}
				used_unit --;
            }
			lpCurrentQueueUnit ++;
		}

		if (0 != ret){		
			//conn queen
			HYP_CONN_QUEEN_SOCKET stream_queue_hcqs;
			char GetData[256] = {0};
			sprintf (GetData,"%s""upstream.php?tid=""%.32s""&tno=""%i",QueenHostDir,tid,tNumber);
			if (0 != hyp_conn_queen_init(GetData,0,0,&stream_queue_hcqs,1)){
                ret = -1;
			}else{			
				//lock
				pthread_spin_lock(&lpQueue->spinLock);
				//keep socket record
				if (lpQueue->usableUnit > 0){				
					lpCurrentQueueUnit = (HYP_STREAM_QUEUE *)(lpQueue + sizeof(HYP_STREAM_QUEUE_HEAD));
					while (lpCurrentQueueUnit->hcqs.fd){
						lpCurrentQueueUnit ++;
					}
					memcpy(lpCurrentQueueUnit->tid,tid,32);  
					lpCurrentQueueUnit->tnumber = tNumber;
					lpCurrentQueueUnit->hcqs    = stream_queue_hcqs;
					lpQueue->usableUnit --;
					* retPtr = lpCurrentQueueUnit;
					ret = 0;
				}else{
					ret = -2;
				}
				//unlock
				pthread_spin_unlock(&lpQueue->spinLock);
			}
		}
	}else{
		ret = -2;
	}
    return ret;
}

//tid == 0 ? clear all
//  0 success
// -1 not found
int hyp_queue_destroy(HYP_STREAM_QUEUE_HEAD * lpQueue,char * tid,unsigned int tNumber){
	int ret = -1;
    int n = HYP_STREAM_QUEUE_NUMBER;
	n -= lpQueue->usableUnit;
    HYP_STREAM_QUEUE * lpCurrentQueueUnit = (HYP_STREAM_QUEUE *)(lpQueue + sizeof(HYP_STREAM_QUEUE_HEAD));	
    
	//lock
	pthread_spin_lock(&lpQueue->spinLock);

	while (n > 0){		
        if (lpCurrentQueueUnit->hcqs.fd){		    			
		    n --;
			if ((0 == tid) || ((0 == memcmp(&lpCurrentQueueUnit->tid,tid,32)) && (lpCurrentQueueUnit->tnumber == tNumber))){
				ret = 0;

				if (0 != hyp_conn_queen_close(&lpCurrentQueueUnit->hcqs)){
				    debug_msg("shut socket failed: tid: %s tnumber: %i\n",lpCurrentQueueUnit->tid,lpCurrentQueueUnit->tnumber);
				}else{
					debug_msg("shut socket success: tid: %s tnumber: %i\n",lpCurrentQueueUnit->tid,lpCurrentQueueUnit->tnumber);
				}
				
				
				memset(lpCurrentQueueUnit,0,sizeof(HYP_STREAM_QUEUE));
				lpQueue->usableUnit ++;
			}			
		}
		lpCurrentQueueUnit ++;
	}
	
	//unlock
	pthread_spin_unlock(&lpQueue->spinLock);

    if (0 == tid){
		pthread_spin_destroy(&lpQueue->spinLock);
	    free (lpQueue);
    }
	return ret;
}


void hyp_queue_print(HYP_STREAM_QUEUE_HEAD * lpQueue){
	int n = HYP_STREAM_QUEUE_NUMBER;
	n -= lpQueue->usableUnit;
	printf("lock:%i usable num:%i n:%i\n",lpQueue->spinLock,lpQueue->usableUnit,n);
	HYP_STREAM_QUEUE * lpCurrentQueueUnit = (HYP_STREAM_QUEUE *)(lpQueue + sizeof(HYP_STREAM_QUEUE_HEAD));
	while (n > 0){		
        if (lpCurrentQueueUnit->hcqs.fd){
		    printf("tid:%.*s"" ""tNumber:%i\n",32,lpCurrentQueueUnit->tid,lpCurrentQueueUnit->tnumber);
		    n --;
		}
		lpCurrentQueueUnit ++;
	}

}
/*
void *consumer01(HYP_STREAM_QUEUE_HEAD * lpQueue){
   int t=1;
   for (;t<100 ;t++ ){
	   usleep (10);
	   hyp_queue_insert(lpQueue,"zbcdefghijkbcdefghijkbcdefghijki",t);
   }  
}

void *consumer02(HYP_STREAM_QUEUE_HEAD * lpQueue){
   int t=5000;
   for (;t<5100 ;t++ ){
	   usleep (11);
	   hyp_queue_insert(lpQueue,"hhhdefghijkbcdefghijkbcdefghijkZ",t);
   }  
}*/
/*
int main(){

   QueenDomainName = "192.168.93.1";
        QueenHostPort   =  80;
		QueenHostName   = "192.168.93.1";
		QueenHostDir    = "/bmc/queen/public_html/srv/";


   HYP_STREAM_QUEUE_HEAD * buff = hyp_queue_init();
   hyp_queue_print(buff);
 //
   pthread_t thr1, thr2;
   pthread_create(&thr1, NULL, consumer01, buff);
   pthread_create(&thr2, NULL, consumer02, buff);
   pthread_join(thr1, NULL);
   pthread_join(thr2, NULL);

   hyp_queue_print(buff);
//
     
  HYP_STREAM_QUEUE * ptr = 0;
  int aa = hyp_queue_insert  (buff,"hhhdefghijkbcdefghijkbcdefghijkZ",5055,&ptr);  
  printf("insert: %p %i \n",ptr,aa);
   aa = hyp_queue_insert  (buff,"hhhdefghijkbcdefghijkbcdefghijkZ",5055,&ptr);  
  printf("insert: %p %i \n",ptr,aa);
   aa = hyp_queue_insert  (buff,"hhhdefghijkbcdefghijkbcdefghijkZ",5055,&ptr);  
  printf("insert: %p %i \n",ptr,aa);
   aa = hyp_queue_insert  (buff,"hhhdefghijkbcdefghijkbcdefghijkZ",1,&ptr);  
  printf("insert: %p %i \n",ptr,aa);
  
  if ((aa == 0) && (ptr)) {

  sleep (50);
printf ("\n\n\n");
	  hyp_queue_destroy(buff,"hhhdefghijkbcdefghijkbcdefghijkZ",5055);
	  aa = hyp_queue_insert  (buff,"hhhdefghijkbcdefghijkbcdefghijkZ",1,&ptr);  
	  printf("insert: %p %i \n",ptr,aa);

  char echo[1024]={0};
  hyp_conn_queen_read(&ptr->hcqs,echo,1024);
  printf("echo: %s \n",echo);
  
  }
  
 hyp_queue_destroy(buff,0,0);
   hyp_queue_print(buff);

   return 0;
}
*/