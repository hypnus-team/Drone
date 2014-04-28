/*

 * OpenSSL SSL/TLS Https Client example

 * Only for Unix/Linux:

 *    cc -c https.c

 *    cc -o https https.c -lssl

 * OpenSSL library needed.

 *

 * 同时支持普通的socket连接以及基于普通socket基础之上的ssl

 * 连接。这对于已有的socket程序修改来说会比较方便，不至于

 * 和原来的结构发生太大的冲突.

 * 要注意的一点，似乎当使用socket套接字来创建ssl连接的时候,

 * 如果套接字是采用非阻塞方式建立的话，会导致ssl会话失败，不

 * 知道为什么。所以这里对于提供给https的套接字采用了普通的

 * connect方法创建。

 *

 */

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


#include <openssl/crypto.h>

#include <openssl/ssl.h>

#include <openssl/err.h>

#include <openssl/rand.h>


//xnet_select x defines

#define READ_STATUS     0

#define WRITE_STATUS    1

#define EXCPT_STATUS    2

/* flag to set request with ssl or not. */

//Requests pointers
static char * lp_buff_write  = 0;
static char * lp_buff_read   = 0;
static char * lp_buff_base   = 0;
static uint32_t dw_buff_available = 0;
static uint32_t dw_buff_remained  = 0;

//
static char * lp_get_buff;

//
#pragma pack(1)
typedef struct{
	char Delimiter_1;
    uint32_t Module_NO[4];
	char Delimiter_2;
	char Flag[32];
	char Delimiter_3;
	uint32_t Request_size;
}REQUEST_HEADER_QUEEN;
#pragma pack()

#pragma pack(1)
typedef struct{
	uint32_t Request_size[4];
}MODULE_NUMBER;
#pragma pack()

//

union semun  
{  
    int val;  
    struct semid_ds *buf;  
    unsigned short *arry;  
};  


static    uint32_t parser_wire = sizeof(REQUEST_HEADER_QUEEN);

static    int parser_step = 0; //0: ready to get header  1: ready to get request string  2: ready to be route

static    REQUEST_HEADER_QUEEN c_header;


//

int connect_breath_srv(char *);

int init_srv();

