
#include "common.h"
#include "hyp_conn_queen.h"

#include <sys/ipc.h>
#include <sys/sem.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <stdarg.h>

#include <errno.h>

#include <fcntl.h>

#include <unistd.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <netdb.h>

#include <linux/tcp.h>

#include <errno.h>

static int timeout_sec = 25;

static int timeout_microsec = 0;

//ret:  0 success
//     -1 fail
int hyp_conn_queen_init(char * GetData,char * PostData,int PostSize,HYP_CONN_QUEEN_SOCKET * hcqs,int dropHead,int nonBlock){

    //construct http head
	char method[5];
	char PostHead[128] = {0};	
	if (PostData){
		strcpy (method,"POST");
		if (0 == PostSize){
			PostSize = strlen(PostData);
		}		
		sprintf (PostHead,"Content-Length: %i\r\nContent-Type: application/x-www-form-urlencoded\r\n",PostSize);
	}else{
	    strcpy (method,"GET");
	}
    
	char httpHead[1024] = {0};
	sprintf (httpHead,"%s %s HTTP/1.0\r\nHost: %s\r\n""%s""Connection: close\r\n\r\n",method,GetData,QueenHostName,PostHead);

    //debug_msg("httpHead: %s",httpHead);


    int fd;
    int ret;

    /* make connection to the cache server */

    fd = create_tcpsocket(nonBlock);
	if (fd < 0 ){
		debug_msg("create_tcpsocket failed (ret: %i)\n",fd);
        return -1;
	}

	#ifdef QueenSSL    
      SSL *ssl;
      SSL_CTX *ctx;
        
      ctx = SSL_CTX_new(SSLv23_client_method());
      if ( ctx == NULL ){
		  debug_msg("init SSL CTX failed\n");//debug_msg("init SSL CTX failed:%s\n", ERR_reason_error_string(ERR_get_error()));
      }
	  ssl = SSL_new(ctx);
	  if ( ssl == NULL ){
		  debug_msg("new SSL with created CTX failed:\n");//debug_msg("new SSL with created CTX failed:%s\n",ERR_reason_error_string(ERR_get_error()));
      }
      ret = SSL_set_fd(ssl, fd);
      if ( ret == 0 ){
         debug_msg("add SSL to tcp socket failed:\n"); //debug_msg("add SSL to tcp socket failed:%s\n",ERR_reason_error_string(ERR_get_error()));
      }
	  /* PRNG */
      RAND_poll();
	  while ( RAND_status() == 0 ){
		  unsigned short rand_ret = rand() % 65536;
          RAND_seed(&rand_ret, sizeof(rand_ret));
	  }

	  int status = 0;

	  do{
		  /* SSL Connect */
		  status = SSL_connect(ssl);

		  switch (SSL_get_error (ssl, status))
		  {
			case SSL_ERROR_NONE:          
				debug_msg("SSL_ERROR_NONE \n");
			  status = 0;               // To tell caller about success
			  break;                    // Done

			case SSL_ERROR_WANT_WRITE:      
				debug_msg("SSL_ERROR_WANT_WRITE \n");    
			  status = 1;               // Wait for more activity
			  break;

			case SSL_ERROR_WANT_READ:
				debug_msg("SSL_ERROR_WANT_READ \n");
			  status = 1;               // Wait for more activity
			  break;

			case SSL_ERROR_ZERO_RETURN:
				debug_msg("SSL_ERROR_ZERO_RETURN \n");
			  status = -1;
			  break;
            case SSL_ERROR_SYSCALL:
				if (SSL_want_write (ssl)){
                    status = 1;
                }else if (SSL_want_read (ssl)){
                    status = 1;
                }else{
				    status = -1;
				}
			    break;
			default:
				debug_msg("default \n");
			  status = -1;
			  break;
		   }
		  debug_msg("SSL connection ret: %i \n",status);
          if (1 == status){
			  if (xnet_select(fd, timeout_sec, timeout_microsec, READ_STATUS)>0){    
				  status = 1;
			  }else{
				  status = -1;
			  }
		  }
	  }while(status == 1 && !SSL_is_init_finished (ssl));      
     
	  if(0 != status){
		  debug_msg("SSL connection failed:\n");//debug_msg("SSL connection failed:%s\n",ERR_reason_error_string(ERR_get_error()));
		  ret = -1;
      }else{
		  hcqs->fd  = fd;
		  hcqs->ssl = ssl;
		  hcqs->ctx = ctx;
		  ret = hyp_conn_queen_write(hcqs,httpHead,strlen(httpHead));
          if ((0 == ret) && (PostData)){
			  ret = hyp_conn_queen_write(hcqs,PostData,PostSize);			  
		  }		
		  if ((0 == ret) && (dropHead)){ //try to drop received http Head
			  hyp_conn_queen_drophead(hcqs);		
		  }	
	  }	 
    #else
	if(xnet_select(fd, timeout_sec, timeout_microsec, WRITE_STATUS)>0){
		hcqs->fd = fd;
        ret = hyp_conn_queen_write(hcqs,httpHead,strlen(httpHead));
		if ((0 == ret) && (PostData)){
			ret = hyp_conn_queen_write(hcqs,PostData,PostSize);
		}
		if ((0 == ret) && (dropHead)){ //try to drop received http Head
            hyp_conn_queen_drophead(hcqs);		
		}		
    }else{
       ret = -1;
       debug_msg("Socket I/O Write Timeout %s:%d\n", QueenDomainName, QueenHostPort);

	}
	#endif

	return ret;
}

void hyp_conn_queen_drophead(HYP_CONN_QUEEN_SOCKET * hcqs){
    int n = 1;
	int flag = 0;
	int check = 0;

    //debug_msg("\n Drop head start: \n");

	while (1 == n){
		n = hyp_conn_queen_read(hcqs,(char *)&flag,1,QueenSSL_nonBlock);

        //debug_msg("%s",(char *)&flag);

		if ((flag == 0x0D) || ((flag == 0x0A) && (check))){
			check ++;
		}else{
			check = 0;
		}
		if (4 == check){
			break;
		}
	}
	debug_msg("Drop head drop finished \n");
}

// > 0  success :buff size
//== 0  timeout 
//<  0  fail  | (select ok & read zero) => conn broken

int hyp_conn_queen_read(HYP_CONN_QUEEN_SOCKET * hcqs,char * buff,int buffSize,int nonBlock){
	int n;
	#ifdef QueenSSL		
		if (nonBlock){ //#ifdef QueenSSL_NonBlock
			while (1){
				if ((n = xnet_select(hcqs->fd, timeout_sec, timeout_microsec, READ_STATUS)>0)){   
					if (0 == (n = SSL_read(hcqs->ssl, buff, buffSize))){
						n = -1;
					}else if (-1 == n){
						if (SSL_ERROR_WANT_READ == SSL_get_error (hcqs->ssl,n)){
							continue;
						}
					}	
				}
				break;
			}	
	    }else{ //#else
		    if (0 == (n = SSL_read(hcqs->ssl, buff, buffSize))){
				n = -1;
			}
		}      //#endif		
    #else     
	 if ((n = xnet_select(hcqs->fd, timeout_sec, timeout_microsec, READ_STATUS)>0)){    
         if (0 == (n = read(hcqs->fd, buff, buffSize))){
		     n = -1;
		 }
	 }	
	#endif    
    return n;
}


int hyp_conn_queen_close(HYP_CONN_QUEEN_SOCKET * hcqs){
    #ifdef QueenSSL		
	  int ret = SSL_shutdown(hcqs->ssl); 
	  if (0 == ret){
		  ret = SSL_shutdown(hcqs->ssl);		  
	  }
	  if( ret != 1 ){
	      debug_msg("SSL shutdown failed[%i]: %s \n",ret,ERR_reason_error_string(ERR_get_error()));//debug_msg("SSL shutdown failed:%s\n",ERR_reason_error_string(ERR_get_error()));
	  }
	  // close the plain socket handler.
	  close(hcqs->fd);
	  // clear ssl resource.
	  SSL_free(hcqs->ssl); 
	  SSL_CTX_free(hcqs->ctx);
	  return 0;
    #else     
	 return close(hcqs->fd);
	#endif  

}

int hyp_conn_queen_write(HYP_CONN_QUEEN_SOCKET * hcqs,char * buff,size_t TotalSize){
	int sent = 0;
	while (TotalSize > 0){	
	    #ifdef QueenSSL
          sent = SSL_write(hcqs->ssl,buff,TotalSize);
        #else
		  sent = write(hcqs->fd,buff,TotalSize);			
		#endif
		debug_msg("hyp_conn_queen_write ret: %i \n", sent);
		if (sent < 0){
			debug_msg("hyp_conn_queen_write fail,totalSize[%i] errno[%d][%s]\n", TotalSize,errno,strerror(errno));
			if (EAGAIN == errno){ // Resource temporarily unavailable
				sleep (1);
				sent = 0;
			}else{
				return -1;
			}
		}
		buff       += sent;
		TotalSize  -= sent;       
	} 
	return 0;
}


/* create common tcp socket connection */

int create_tcpsocket(int nonBlock){

    int ret;


    char * transport = "tcp";

    struct hostent *phe; /* pointer to host information entry */

    //struct protoent *ppe; /* pointer to protocol information entry */

    //struct sockaddr_in sin; /* an Internet endpoint address */

    int s; /* socket descriptor and socket type */

    if (0 == lpQueenSin){

        memset(&QueenSin, 0, sizeof(QueenSin));

		QueenSin.sin_family = AF_INET;    

		if ((QueenSin.sin_port = htons(QueenHostPort)) == 0){

			debug_msg("invalid port \"%d\"\n", QueenHostPort);
			return -13;
		}

		

		/* Map host name to IP address, allowing for dotted decimal */

		if(( phe = gethostbyname(QueenDomainName) )){

			memcpy(&QueenSin.sin_addr, phe->h_addr, phe->h_length);

		}else{

			
			if( (QueenSin.sin_addr.s_addr = inet_addr(QueenDomainName)) == INADDR_NONE ){
				debug_msg("can't get \"%s\" host entry\n", QueenDomainName);
				return -12;
			}
		}

		lpQueenSin = &QueenSin;
    }

    /* Map transport protocol name to protocol number */
    if (0 == lpQueenPpe){
		if ((lpQueenPpe = getprotobyname(transport)) == 0){
			debug_msg("can't get \"%s\" protocol entry\n", transport);
			return -11;
		}
	}

    

    /* Allocate a common TCP socket */

    s = socket(PF_INET, SOCK_STREAM, lpQueenPpe->p_proto);

    if (s < 0){
        debug_msg("can't create socket: %s\n", strerror(errno));
		return s;
	}

    
    int enable = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));

    #ifdef QueenSSL
		/* create common tcp socket.seems non-block type is not supported by ssl. */
        
        ret = connect(s, (struct sockaddr *)&QueenSin, sizeof(QueenSin)); 
		
		if (nonBlock){ //#ifdef QueenSSL_NonBlock
			fcntl(s,F_SETFL, O_NONBLOCK);
		}              //#endif

    #else

		/* Connect the socket with timeout */

        fcntl(s,F_SETFL, O_NONBLOCK);

        if (connect(s, (struct sockaddr *)&QueenSin, sizeof(QueenSin)) == -1){

            if (errno == EINPROGRESS){// it is in the connect process 

                struct timeval tv; 

                fd_set writefds; 

                tv.tv_sec = timeout_sec; 

                tv.tv_usec = timeout_microsec; 

                FD_ZERO(&writefds); 

                FD_SET(s, &writefds); 

                if(select(s+1,NULL,&writefds,NULL,&tv)>0){ 

                    int len=sizeof(int); 

                    //下面的一句一定要，主要针对防火墙 

                    getsockopt(s, SOL_SOCKET, SO_ERROR, &errno, (socklen_t *)&len); 

                    if(errno != 0) 

                        ret = 1;

                    else

                        ret = 0;

                }

                else

                    ret = 2;//timeout or error happen 

            }

            else ret = 1; 


        }

        else{

            ret = 1;

        }
        

    #endif


    if(ret != 0){

        close(s);

        debug_msg("can't connect to %s:%d\n", QueenDomainName, QueenHostPort);

    }


    return s;

}


/*

s    - SOCKET

sec  - timeout seconds

usec - timeout microseconds

x    - select status

*/

int xnet_select(int s, int sec, int usec, short x){

    int st = errno;

    struct timeval to;

    fd_set fs;

    to.tv_sec = sec;

    to.tv_usec = usec;

    FD_ZERO(&fs);

    FD_SET(s, &fs);

    switch(x){

        case READ_STATUS:

        st = select(s+1, &fs, 0, 0, &to);

        break;

        case WRITE_STATUS:

        st = select(s+1, 0, &fs, 0, &to);

        break;

        case EXCPT_STATUS:

        st = select(s+1, 0, 0, &fs, &to);

        break;

    }

    return(st);

}

/*

int main(){
	#ifdef QueenSSL
		SSL_library_init();
	    //SSL_load_error_strings();	
	#endif

	#ifdef QueenSSL
	    QueenDomainName = "www.hypnusoft.com";
        QueenHostPort   =  443;
		QueenHostName   = "www.hypnusoft.com";
        char GetData[] = "/cpanel/srv/upstream.php?tid=PPv25PvkQZdQSXcmTxR7LIfjQ2AAQmow&tno=0";
	#else
		QueenDomainName = "www.hypnusoft.com";
        QueenHostPort   =  80;
		QueenHostName   = "www.hypnusoft.com";
		char GetData[] = "/cpanel/test.php";			
	#endif
	
	char PostData[] = "";

    HYP_CONN_QUEEN_SOCKET test;
	
	if (0 == hyp_conn_queen_init(GetData,0,0,&test,0)){
		char tmp[1024] = {0};

        int j = 1;
		while (1){		
		    j = hyp_conn_queen_read(&test,tmp,1024);
			
			debug_msg("ret[%i]",j);					
            
			if (j == 0){
				debug_msg("out time...\n");
			}else if (j < 0){
				int i = SSL_get_error (test.ssl, j);

              switch (i){
              
				  case SSL_ERROR_NONE:          
					debug_msg("SSL_ERROR_NONE \n");
				  break;                    // Done

				case SSL_ERROR_WANT_WRITE:      
					debug_msg("SSL_ERROR_WANT_WRITE \n");    
				  break;

				case SSL_ERROR_WANT_READ:
					debug_msg("SSL_ERROR_WANT_READ \n");
				  break;

				case SSL_ERROR_ZERO_RETURN:
					debug_msg("SSL_ERROR_ZERO_RETURN \n");
				  break;
				case SSL_ERROR_SYSCALL:
			       debug_msg("SSL_ERROR_SYSCALL \n");		
					break;
				default:
					debug_msg("default \n");
				    break;
			  }
				sleep (1);
				//break;
			}else{
			
			    debug_msg(",received ok \n",tmp);	
			}
			
		}

		int n = hyp_conn_queen_close(&test);
		debug_msg("\n close: %i \n",n);		

	}
    
    #ifdef QueenSSL
		//ERR_free_strings();
	#endif

	return 0;
}
*/