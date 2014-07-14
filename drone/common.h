#include <stdint.h>
#include <stdio.h> 
#include <stdlib.h>
#include <netinet/in.h>

#ifdef DEBUG
#define debug_msg(format,...)   \
    fprintf(stdout,format,##__VA_ARGS__)
#else
    #define debug_msg(format,...)
#endif   /* DEBUG */

#ifdef QueenSSL_NonBlock
    #define QueenSSL_nonBlock 1
#else
	#define QueenSSL_nonBlock 0
#endif

//Drone's Url submit Buffer
#define DroneSubmitBuffSize 1024 * 1024

//Drone's stream queue Size
#define DroneStreamQueueSize 1024

//Queen's Request Buffer
uint32_t RequestBuffSize; //2* 1024 * 1024
uint32_t ResponseUnitSize;
uint32_t ResponseMaxSize;

//Drone's module List Buffer
#define DroneModuleListBuffSize 1024 * 64 //32 * 64 = 2048 (mod) in 64 bit
                                          //42 * 64 = 2730 (mod) in 32 bit

//drone's  status
int DroneStatus;
//drone's ID
uint32_t DroneID;
//drone's module List ptr
#pragma pack(1)
typedef struct{
	uint32_t Module_NO[4];
	void *   Module_Func;
	void *   Module_Notify;
	void *   Module_Base;	
}MODULE_LIST;
#pragma pack()

MODULE_LIST * DroneModuleList;

//Queen srv 's contents
void * lpQueenSin;
struct sockaddr_in QueenSin;
struct protoent * lpQueenPpe;

//upgrade Drone online
void * newDroneModuleList;
void * newDroneExec;