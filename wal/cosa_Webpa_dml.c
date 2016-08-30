
#include "ansc_platform.h"
#include "cosa_Webpa_dml.h"
#include "ccsp_trace.h"
#include "msgpack.h"
#include "base64.h"
#include "wal.h"
#include "ccsp_base_api.h"

void (*notifyCbFnPtr)(NotifyData*) = NULL;
void sendUpstreamNotification(char *UpstreamMsg, int size);

WAL_STATUS
Webpa_DmlDiGetWebPACfg
    (
        char*                       ParamName,
        char*                       pValue
    );

WAL_STATUS
Webpa_DmlDiSetWebPACfg
    (
        char*                       ParamName,
        char*                       pValue
    );

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Webpa_SetParamValues
            (
                ANSC_HANDLE                 hInsContext,
                char**                      ppParamArray,
                PSLAP_VARIABLE*             ppVarArray,
                ULONG                       ulArraySize,
                PULONG                      pulErrorIndex
            );

    description:

        This function is called to set bulk parameter values; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char**                      ppParamName,
                The parameter name array;

                PSLAP_VARIABLE*             ppVarArray,
                The parameter values array;

                ULONG                       ulArraySize,
                The size of the array;

                PULONG                      pulErrorIndex
                The output parameter index of error;

    return:     TRUE if succeeded.

**********************************************************************/

BOOL
Webpa_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
	char*                       pString
    )
{
	msgpack_zone mempool;
	msgpack_object deserialized;
	
	msgpack_unpack_return unpack_ret;
	char * decodeMsg =NULL;
	int decodeMsgSize =0;
	int size =0;
#ifdef USE_NOTIFY_COMPONENT
	
	char* p_write_id;
	char* p_new_val;
	char* p_old_val;
	char* p_notify_param_name;
	char* st;
	char* p_interface_name = NULL;
	char* p_mac_id = NULL;
	char* p_status = NULL;
	char* p_val_type;
	UINT value_type,write_id;
	parameterSigStruct_t param = {0};
#endif

	WalPrint("<========= Start of Webpa_SetParamStringValue ========>\n");
	WalInfo("Received data ParamName %s,data length: %d bytes, Value : %s\n",ParamName, strlen(pString), pString);
	
	if( AnscEqualString(ParamName, "PostData", TRUE))
    	{    		
    		//Start of b64 decoding
    		WalPrint("----Start of b64 decoding----\n");
    		decodeMsgSize = b64_get_decoded_buffer_size(strlen(pString));
    		WalPrint("expected b64 decoded msg size : %d bytes\n",decodeMsgSize);
    		
		decodeMsg = (char *) malloc(sizeof(char) * decodeMsgSize);
				
    		size = b64_decode( pString, strlen(pString), decodeMsg );
    		WalPrint("base64 decoded data containing %d bytes is :%s\n",size, decodeMsg);
    		
    		WalPrint("----End of b64 decoding----\n");
    		//End of b64 decoding
    		
    		//Start of msgpack decoding just to verify
    		WalPrint("----Start of msgpack decoding----\n");
		msgpack_zone_init(&mempool, 2048);
		unpack_ret = msgpack_unpack(decodeMsg, size, NULL, &mempool, &deserialized);
		WalPrint("unpack_ret is %d\n",unpack_ret);
		switch(unpack_ret)
		{
			case MSGPACK_UNPACK_SUCCESS:
				WalInfo("MSGPACK_UNPACK_SUCCESS :%d\n",unpack_ret);
				WalPrint("\nmsgpack decoded data is:");
				msgpack_object_print(stdout, deserialized);
			break;
			case MSGPACK_UNPACK_EXTRA_BYTES:
				WalError("MSGPACK_UNPACK_EXTRA_BYTES :%d\n",unpack_ret);
			break;
			case MSGPACK_UNPACK_CONTINUE:
				WalError("MSGPACK_UNPACK_CONTINUE :%d\n",unpack_ret);
			break;
			case MSGPACK_UNPACK_PARSE_ERROR:
				WalError("MSGPACK_UNPACK_PARSE_ERROR :%d\n",unpack_ret);
			break;
			case MSGPACK_UNPACK_NOMEM_ERROR:
				WalError("MSGPACK_UNPACK_NOMEM_ERROR :%d\n",unpack_ret);
			break;
			default:
				WalError("Message Pack decode failed with error: %d\n", unpack_ret);	
		}

		msgpack_zone_destroy(&mempool);
		WalPrint("----End of msgpack decoding----\n");
		//End of msgpack decoding
		
		notifyCbFnPtr = getNotifyCB();
		if (NULL == notifyCbFnPtr) {
			WalError("Fatal: notifyCbFnPtr is NULL\n");
			return FALSE;
		}
		else
		{
			WalPrint("before sendUpstreamNotification in cosaDml\n");
			sendUpstreamNotification(decodeMsg, size);
			WalPrint("After sendUpstreamNotification in cosaDml\n");
		}
		WalPrint("----End PostData parameter----\n");

		return TRUE;
	}   
	
	if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_WebPA_Notification", TRUE))
	{
	#ifdef USE_NOTIFY_COMPONENT

		WalPrint(" \n WebPA : Notification Received \n");
		
		p_notify_param_name = strtok_r(pString, ",", &st);
		p_write_id = strtok_r(NULL, ",", &st);
		p_new_val = strtok_r(NULL, ",", &st);
		p_old_val = strtok_r(NULL, ",", &st);
		p_val_type = strtok_r(NULL, ",", &st);
		
		//printf(" \n Notification : Parameter Name = %s \n", p_notify_param_name);
		if(p_val_type !=NULL && p_write_id !=NULL)
		{
			value_type = atoi(p_val_type);
			write_id = atoi(p_write_id);

			WalPrint(" \n Notification : Parameter Name = %s \n", p_notify_param_name);
			WalPrint(" \n Notification : New Value = %s \n", p_new_val);
			WalPrint(" \n Notification : Old Value = %s \n", p_old_val);
			WalPrint(" \n Notification : Value Type = %d \n", value_type);
			WalPrint(" \n Notification : Component ID = %d \n", write_id);

			param.parameterName = p_notify_param_name;
			param.oldValue = p_old_val;
			param.newValue = p_new_val;
			param.type = value_type;
			param.writeID = write_id;

			ccspWebPaValueChangedCB(&param,0,NULL);
		}
		else
		{
			WalError("Received insufficient data to process notification\n");
		}
		
#endif
		return TRUE;
	}    

	if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_Connected-Client", TRUE))
	{
	#ifdef USE_NOTIFY_COMPONENT
	
                WalPrint(" \n WebPA : Connected-Client Received \n");
                p_notify_param_name = strtok_r(pString, ",", &st);
                WalPrint("PString value for X_RDKCENTRAL-COM_Connected-Client:%s\n", pString);

                p_interface_name = strtok_r(NULL, ",", &st);
                p_mac_id = strtok_r(NULL, ",", &st);
                p_status = strtok_r(NULL, ",", &st);

                WalPrint(" \n Notification : Parameter Name = %s \n", p_notify_param_name);
                WalPrint(" \n Notification : Interface = %s \n", p_interface_name);
                WalPrint(" \n Notification : MAC = %s \n", p_mac_id);
                WalPrint(" \n Notification : Status = %s \n", p_status);
                
                notifyCbFnPtr = getNotifyCB();
                
                if (NULL == notifyCbFnPtr) 
                {
                        WalError("Fatal: notifyCbFnPtr is NULL\n");
                        return FALSE;
                }
                else
                {
                        // Data received from stack is not sent upstream to server for Connected Client
                        sendConnectedClientNotification(p_mac_id, p_status);
                }
		
#endif
		return TRUE;
	}    

	/* Required for xPC sync */
	if( AnscEqualString(ParamName, "X_COMCAST-COM_CID", TRUE))
    {
		if (syscfg_set(NULL, "X_COMCAST-COM_CID", pString) != 0) 
		{
			WalError("syscfg_set failed\n");
			
		}
		else 
		{
			if (syscfg_commit() != 0) 
			{
				WalError("syscfg_commit failed\n");
				
			}
			
			return TRUE;
		}
    }
    	
    	
	if( AnscEqualString(ParamName, "X_COMCAST-COM_SyncProtocolVersion", TRUE))
    {

		if (syscfg_set(NULL, "X_COMCAST-COM_SyncProtocolVersion", pString) != 0) 
		{
		     WalError("syscfg_set failed\n");
		     
		} 
		else 
		{
		    if (syscfg_commit() != 0)
		    {
		    	WalError("syscfg_commit failed\n");
		    	
		    }
		}
			
		return TRUE;

   	 }
	
	if (AnscEqualString(ParamName, "ServerURL", TRUE))
	{	
	       
	        if(WAL_SUCCESS != Webpa_DmlDiSetWebPACfg(ParamName,pString))
		{
			WalError("set failed for %s parameter\n", ParamName);
			return FALSE;
		}
	        
	        return TRUE;
	        
	}      
	
	if (AnscEqualString(ParamName, "DeviceNetworkInterface", TRUE))
	{	
	        
	        if(WAL_SUCCESS != Webpa_DmlDiSetWebPACfg(ParamName,pString))
		{
			WalError("set failed for %s parameter\n", ParamName);
			return FALSE;
		}
       		
	        return TRUE;
	}
	
	       	         
        if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_LastReconnectReason", TRUE))
        {
                if (syscfg_set(NULL, "X_RDKCENTRAL-COM_LastReconnectReason", pString) != 0) 
				{
					WalError("syscfg_set failed\n");
			
				}
		else 
		{
			if (syscfg_commit() != 0) 
			{
				WalError("syscfg_commit failed\n");
				
			}
			
			return TRUE;
		}
        }
	WalPrint("<=========== End of Webpa_SetParamStringValue ========\n");

    return FALSE;
}

ULONG
Webpa_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
	
	/* Required for xPC sync */
	if( AnscEqualString(ParamName, "X_COMCAST-COM_CID", TRUE))
    {
        	/* collect value */
		char buf[64];
		syscfg_get( NULL, "X_COMCAST-COM_CID", buf, sizeof(buf));

    		if( buf != NULL )
    		{
			AnscCopyString(pValue, buf);
			return 0;
		}
		
		WalError("Failed to get value for %s parameter\n", ParamName);
		return -1;
    }
    	
    	
    	
	if( AnscEqualString(ParamName, "X_COMCAST-COM_SyncProtocolVersion", TRUE))
    {
        /* collect value */
		char buf[5];

		syscfg_get( NULL, "X_COMCAST-COM_SyncProtocolVersion", buf, sizeof(buf));
		
    		if( buf != NULL )
    		{
    		    AnscCopyString(pValue,  buf);
		    return 0;
    		}
    		
    		WalError("Failed to get value for %s parameter\n", ParamName);
		return -1;
    }
    	
    	
	  
	if (AnscEqualString(ParamName, "ServerURL", TRUE))
	{			
	        
    		if(WAL_SUCCESS != Webpa_DmlDiGetWebPACfg(ParamName,pValue))
		{
		
			WalError("Failed to get value for %s parameter\n", ParamName);
			return 1;
		}
		
		return 0;
			
	}
	
	if (AnscEqualString(ParamName, "DeviceNetworkInterface", TRUE))
	{		

		if(WAL_SUCCESS != Webpa_DmlDiGetWebPACfg(ParamName,pValue))
		{

			WalError("Failed to get value for %s parameter\n", ParamName);
			return 1;
		}
	
		return 0;
		
	}
	
	if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_LastReconnectReason", TRUE))
        {
      
        	/* collect value */
		char buf[64];
		syscfg_get( NULL, "X_RDKCENTRAL-COM_LastReconnectReason", buf, sizeof(buf));

    		if( buf != NULL )
    		{
			AnscCopyString(pValue, buf);
			return 0;
		}
		
		WalError("Failed to get value for %s parameter\n", ParamName);
		return -1;
        }
	WalError(("Unsupported parameter '%s'\n", ParamName));
    return -1;
  
}

BOOL
Webpa_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
	
	{
  
	 /* check the parameter name and return the corresponding value */
	   
		char pchar[128];
		if (AnscEqualString(ParamName, "Enable", TRUE))
		{
			if(WAL_SUCCESS != Webpa_DmlDiGetWebPACfg(ParamName,pchar))
			{
				return FALSE;
			}
			if(!strncmp(pchar,"true",strlen("true")))
			{
				*pBool = TRUE;
			}
			else
			{
				*pBool = FALSE;
			}
			
			return TRUE;
		}
		
		WalError("Unsupported parameter '%s'\n", ParamName);
		return FALSE;
	
	}


BOOL
Webpa_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
	
	{	
    
		if (AnscEqualString(ParamName, "Enable", TRUE))
		{
			WalPrint("Already set, do nothing\n");
			return TRUE;
		}
		
		WalError("Unsupported parameter '%s'\n", ParamName);
		return FALSE;
	}


BOOL
Webpa_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )

	{
		/* check the parameter name and return the corresponding value */   

		char pchar[256]; 	
	 
		if( AnscEqualString(ParamName, "ServerPort", TRUE))
			{
				if(WAL_SUCCESS != Webpa_DmlDiGetWebPACfg(ParamName,pchar))
			{
				WalError("Failed to get value for %s parameter\n", ParamName);
				return FALSE;
			}
			*pInt = atoi(pchar);
			
			return TRUE;
			}
			
		if (AnscEqualString(ParamName, "RetryIntervalInSec", TRUE))
			{
				if(WAL_SUCCESS != Webpa_DmlDiGetWebPACfg(ParamName,pchar))
			{
				WalError("Failed to get value for %s parameter\n", ParamName);
				return FALSE;
			}
			*pInt = atoi(pchar);
			
			return TRUE;
			}
			
		if (AnscEqualString(ParamName, "MaxPingWaitTimeInSec", TRUE))
			{
			if(WAL_SUCCESS != Webpa_DmlDiGetWebPACfg(ParamName,pchar))
			{
				WalError("Failed to get value for %s parameter\n", ParamName);
				return FALSE;
			}
			*pInt = atoi(pchar);
			
			return TRUE;
			}	
		
		WalError(("Unsupported parameter '%s'\n", ParamName));
		return FALSE;
	
	
	}


BOOL
Webpa_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "ServerPort", TRUE))
    {
		char pchar[256];

		sprintf(pchar,"%d",iValue);

		if(WAL_SUCCESS != Webpa_DmlDiSetWebPACfg(ParamName,pchar))
		{
			WalError("set failed for %s parameter\n", ParamName);
			return FALSE;
		}
		
		return TRUE;
     }
     
    if( AnscEqualString(ParamName, "RetryIntervalInSec", TRUE))
    {
        	char pchar[256];

		sprintf(pchar,"%d",iValue);

		if(WAL_SUCCESS != Webpa_DmlDiSetWebPACfg(ParamName,pchar))
		{
			WalError("set failed for %s parameter\n", ParamName);
			return FALSE;
		}
		
		return TRUE;
    }
    
    if( AnscEqualString(ParamName, "MaxPingWaitTimeInSec", TRUE))
    {
        	char pchar[256];

		sprintf(pchar,"%d",iValue);	

		if(WAL_SUCCESS != Webpa_DmlDiSetWebPACfg(ParamName,pchar))
		{
			WalError("set failed for %s parameter\n", ParamName);
			return FALSE;
		}
		
		return TRUE;
    }
	
    WalError(("Unsupported parameter '%s'\n", ParamName));
    return FALSE;
	
}

BOOL
Webpa_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    /* check the parameter name and return the corresponding value */
    /* Required for xPC sync */
   
	if( AnscEqualString(ParamName, "X_COMCAST-COM_CMC", TRUE))
    {
        /* collect value */
		char buf[8];

		syscfg_get( NULL, "X_COMCAST-COM_CMC", buf, sizeof(buf));
    	if( buf != NULL )
    		{
    		    *puLong = atoi(buf);
				return TRUE;
    		}
		*puLong = 0;
		WalError("Failed to get value for %s parameter\n", ParamName);
        return FALSE;
    }
		
}

BOOL
Webpa_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
	
   /* check the parameter name and set the corresponding value */
		/* Required for xPC sync */
	if( AnscEqualString(ParamName, "X_COMCAST-COM_CMC", TRUE))
    {
        /* collect value */
		char buf[8];

		snprintf(buf,sizeof(buf),"%d",uValue);
		if (syscfg_set(NULL, "X_COMCAST-COM_CMC", buf) != 0) 
		{
			WalError(("syscfg_set failed\n"));
			
		}
		else 
		{
			if (syscfg_commit() != 0) 
			{
				WalError(("syscfg_commit failed\n"));
				
			}
			
			return TRUE;
		}
    }
	
    return FALSE;
}

/**
 * @brief sendUpstreamNotification function to send Upstream notification
 *
 * @param[in] UpstreamMsg Upstream notification message
 * @param[in] size Size of Upstream notification message
 */
void sendUpstreamNotification(char *msg, int size)
{
	WalPrint("Inside sendUpstreamNotification msg length %d\n", size);

	// create NotifyData struct
	UpstreamMsg *msgPtr = (UpstreamMsg *) malloc(sizeof(UpstreamMsg) * 1);
	msgPtr->msg = msg;
	msgPtr->msgLength = size;
	NotifyData *notifyDataPtr = (NotifyData *) malloc(sizeof(NotifyData) * 1);
	notifyDataPtr->type = UPSTREAM_MSG;
	Notify_Data *notify_data = (Notify_Data *) malloc(sizeof(Notify_Data) * 1);
	notify_data->msg = msgPtr;
	notifyDataPtr->data = notify_data;

	if (NULL != notifyCbFnPtr) 
	{
		(*notifyCbFnPtr)(notifyDataPtr);
	}
}

/**
 * @brief sendConnectedClientNotification function to send Connected Client notification
 * for change to Device.Hosts.Host. dynamic table
 */
void sendConnectedClientNotification(char * macId, char *status)
{
	NotifyData *notifyDataPtr = (NotifyData *) malloc(sizeof(NotifyData) * 1);
	NodeData * node = NULL;
	
	notifyDataPtr->type = CONNECTED_CLIENT_NOTIFY;
	if(macId != NULL && status != NULL)
	{
		node = (NodeData *) malloc(sizeof(NodeData) * 1);
		memset(node,sizeof(node),0);
		WalPrint("macId : %s status : %s\n",macId,status);
		node->nodeMacId = (char *)(malloc(sizeof(char) * strlen(macId) + 1));
		strncpy(node->nodeMacId, macId, strlen(macId) + 1);
				
		node->status = (char *)(malloc(sizeof(char) * strlen(status) + 1));
		strncpy(node->status, status, strlen(status) + 1);
		WalPrint("node->nodeMacId : %s node->status: %s\n",node->nodeMacId,node->status);
	}
	
	Notify_Data *notify_data = (Notify_Data *) malloc(sizeof(Notify_Data) * 1);
	notify_data->node = node;
	notifyDataPtr->data = notify_data;
	
	(*notifyCbFnPtr)(notifyDataPtr);
}


/**
 * @brief Webpa_DmlDiGetWebPACfg function to fetch Webpa config parameter values from nvram config json
 */
#define LINE_SIZE 128
#define TMP_WEBPA_CFG "/tmp/webpa_cfg.txt"
#define WEBPA_CFG     "/nvram/webpa_cfg.json"
 
WAL_STATUS
Webpa_DmlDiGetWebPACfg
    (
        char*                       ParamName,
        char*                       pValue
    )
{
    char str[128];
    char line[LINE_SIZE];
    FILE *fd;
	memset(str, 0, 128);
	fd = fopen("/nvram/webpa_cfg.json", "r");
	if(!fd)
	{	
		WalError("WEBPA_CFG not available");
		return WAL_FAILURE;
	}	
	fclose(fd);
	if (AnscEqualString(ParamName, "ServerURL", TRUE))
	{
		sprintf(str, "grep -r ServerIP %s | awk '{print $2}' | sed 's|[\"\",]||g' > %s", WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "ServerPort", TRUE))
	{
		sprintf(str, "grep -r ServerPort %s | awk '{print $2}' | sed 's|[,]||g' > %s", WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "DeviceNetworkInterface", TRUE))
	{
		sprintf(str, "grep -r DeviceNetworkInterface %s | awk '{print $2}' | sed 's|[\"\",]||g' > %s", WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "RetryIntervalInSec", TRUE))
	{
		sprintf(str, "grep -r RetryIntervalInSec %s | awk '{print $2}' | sed 's|[,]||g' > %s", WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "MaxPingWaitTimeInSec", TRUE))
	{
		sprintf(str, "grep -r MaxPingWaitTimeInSec %s | awk '{print $2}' | sed 's|[,]||g' > %s", WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "Enable", TRUE))
	{
		sprintf(str, "grep -r EnablePa %s | awk '{print $2}' | sed 's|[\"\",]||g' > %s", WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}

	fd = fopen(TMP_WEBPA_CFG, "r");
	if(fgets(line, LINE_SIZE, fd) != NULL)
	{
		strncpy(pValue, line, strlen(line));
		pValue[strlen(pValue)-1] = '\0';
		fclose(fd);
		memset(str, 0, 128);
		sprintf(str, "rm -rf %s", TMP_WEBPA_CFG);
		system(str);
		
		return WAL_SUCCESS;
	}
	return WAL_FAILURE;
		
}

/**
 * @brief Webpa_DmlDiSetWebPACfg function to set Webpa config parameter values in nvram config json
 */
WAL_STATUS
Webpa_DmlDiSetWebPACfg
    (
        char*                       ParamName,
        char*                       pValue
    )
{
    char str[128];
    char line[LINE_SIZE];
    FILE *fd;
	if ((fd = fopen(WEBPA_CFG, "r")) == NULL)
	{
		WalError("WEBPA_CFG do not exist\n");
		return WAL_FAILURE;
	}
	fclose(fd);
	memset(str, 0, 128);
	if (AnscEqualString(ParamName, "ServerURL", TRUE))
	{
		sprintf(str, "sed 's/\\(\"ServerIP\": \\).*/\\1\"%s\",/' %s > %s", pValue, WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "ServerPort", TRUE))
	{
		sprintf(str, "sed 's/\\(\"ServerPort\": \\).*/\\1%s,/' %s > %s", pValue, WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "DeviceNetworkInterface", TRUE))
	{
		sprintf(str, "sed 's/\\(\"DeviceNetworkInterface\": \\).*/\\1\"%s\",/' %s > %s", pValue, WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "RetryIntervalInSec", TRUE))
	{
		sprintf(str, "sed 's/\\(\"RetryIntervalInSec\": \\).*/\\1%s,/' %s > %s", pValue, WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	if (AnscEqualString(ParamName, "MaxPingWaitTimeInSec", TRUE))
	{
		sprintf(str, "sed 's/\\(\"MaxPingWaitTimeInSec\": \\).*/\\1%s,/' %s > %s", pValue, WEBPA_CFG, TMP_WEBPA_CFG);
		system(str);
	}
	
	memset(str, 0, 128);
	sprintf(str, "rm -rf %s", WEBPA_CFG);
	system(str);
	sprintf(str, "cp %s %s", TMP_WEBPA_CFG, WEBPA_CFG);
	system(str);
		
	memset(str, 0, 128);
	sprintf(str, "rm -rf %s", TMP_WEBPA_CFG);
	system(str);
	return WAL_SUCCESS;
}
