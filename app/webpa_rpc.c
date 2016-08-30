/**
 * @file webpa_rpc.c
 *
 * @description This file defines WebPA RPC functions
 *
 * Copyright (c) 2015  Comcast
 */
#include "wal.h"
#include <pthread.h>
#include <errno.h>


/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define WEBPA_ENABLE_RPC

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
static pthread_mutex_t rpcNotifyMutex = PTHREAD_MUTEX_INITIALIZER;

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
void receiveRpcMsgCB(int n, char const* buff);

extern int WebPA_ClientConnector_Start();
//extern int WebPA_ClientConnector_SetDispatchCallback(WebPA_ClientConnector_Dispatcher callback);
extern WAL_STATUS WebPA_ClientConnector_DispatchMessage(char const* topic, char const* buff, int n);
extern WAL_STATUS msgStatus;
extern pthread_mutex_t msgStatusMutex;
extern pthread_cond_t msgStatusCond;

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

/**
 * @brief Initializes WebPA RPC.
 *
 * @return WAL_STATUS.
 */
WAL_STATUS WebpaRpcInit()
{
#ifdef WEBPA_ENABLE_RPC
	WebPA_ClientConnector_SetDispatchCallback(&receiveRpcMsgCB);
	WebPA_ClientConnector_Start();
	WalInfo("Successfully initialized WebPA RPC\n");
#else
	WalInfo("WebPA RPC interface is disabled\n");
#endif
	return WAL_SUCCESS;
}

/**
 * @brief sendIoTMessage interface sends message to IoT. 
 *
 * @param[in] msg Message to be sent to IoT.
 * @return WAL_STATUS
 */
WAL_STATUS sendIoTMessage(const void *msg)
{
#ifdef WEBPA_ENABLE_RPC
	WAL_STATUS ret = WAL_FAILURE;
	struct timespec     ts;
	struct timespec     now;
	int n;

	memset(&ts, 0, sizeof(struct timespec));
	memset(&now, 0, sizeof(struct timespec));

	if(msg != NULL)
	{
		pthread_mutex_lock(&msgStatusMutex);
	
		ret = WebPA_ClientConnector_DispatchMessage("iot", (char*)msg, strlen(msg));

		if(ret != WAL_SUCCESS)
		{
			WalError("sendIoTMessage(): DispatchMessage Failed\n");
			pthread_mutex_unlock(&msgStatusMutex);
			return ret;
		}
		else
		{
			WalPrint("sendIoTMessage(): Dispatch messge Success\n");
		}

		clock_gettime(CLOCK_REALTIME, &now);
		ts.tv_sec = now.tv_sec + 2;

		n = pthread_cond_timedwait(&msgStatusCond, &msgStatusMutex, &ts);

		if(n == ETIMEDOUT)
		{
			ret = WAL_ERR_TIMEOUT;
			WalError("sendIoTMessage(): Timedout");
		}
		else
		{
			ret = msgStatus;
			WalInfo("sendIoTMessage(): Message status %d\n", ret);
		}
		pthread_mutex_unlock(&msgStatusMutex);
	}
	else
	{
		WalError("sendIoTMessage(): Null Message");
	}

	return ret;
#endif
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/

void receiveRpcMsgCB(int n, char const* buff)
{
#ifdef WEBPA_ENABLE_RPC
	if(buff != NULL)
	{
		WalInfo("Received message: [%d]'%.*s'\n", n, n, buff);
		pthread_mutex_lock(&rpcNotifyMutex);
		sendIoTNotification((void*)buff, n);
		pthread_mutex_unlock(&rpcNotifyMutex);
	}
	else
	{
		WalError("Received NULL message from IoT\n");
	}
#endif
}
