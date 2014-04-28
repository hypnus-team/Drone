
//init Queen's options
char * QueenDomainName;
char * QueenHostName;
char * QueenHostDir;
unsigned short QueenHostPort;


#ifdef QueenSSL
	#include <openssl/crypto.h>

	#include <openssl/ssl.h>

	#include <openssl/err.h>

	#include <openssl/rand.h>
#endif

#ifdef QueenSSL
	typedef struct{
		SSL     * ssl;
        SSL_CTX * ctx;
		int       fd;
	}HYP_CONN_QUEEN_SOCKET;
#else        
	typedef struct{
		int       fd;
	}HYP_CONN_QUEEN_SOCKET;
#endif

#define READ_STATUS     0

#define WRITE_STATUS    1

#define EXCPT_STATUS    2

int hyp_conn_queen_init(char *,char *,int,HYP_CONN_QUEEN_SOCKET *,int);
int hyp_conn_queen_read(HYP_CONN_QUEEN_SOCKET *,char *,int);
int hyp_conn_queen_close(HYP_CONN_QUEEN_SOCKET *);
int hyp_conn_queen_write(HYP_CONN_QUEEN_SOCKET *,char *,size_t);
void hyp_conn_queen_drophead(HYP_CONN_QUEEN_SOCKET *);
int xnet_select(int, int, int, short);
int create_tcpsocket();
