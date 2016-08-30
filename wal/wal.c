/**
 * @file wal.c
 *
 * @description This file describes the Webpa Abstraction Layer
 *
 * Copyright (c) 2015  Comcast
 */
#include "ssp_global.h"
#include "stdlib.h"
#include "ccsp_dm_api.h"
#include "wal.h"
#include <sys/time.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define WIFI_INDEX_MAP_SIZE                     18
#define WIFI_PARAM_MAP_SIZE			3
#define WIFI_MAX_STRING_LEN			128
#define MAX_PARAMETERNAME_LEN			512
#define MAX_PARAMETERVALUE_LEN			512
#define MAX_DBUS_INTERFACE_LEN			32
#define MAX_PATHNAME_CR_LEN			64
#define CCSP_COMPONENT_ID_WebPA			0x0000000A
#define CCSP_COMPONENT_ID_XPC			0x0000000B
#define RDKB_TR181_OBJECT_LEVEL1_COUNT	        34
#define RDKB_TR181_OBJECT_LEVEL2_COUNT	        12
#define WAL_COMPONENT_INIT_RETRY_COUNT          4
#define WAL_COMPONENT_INIT_RETRY_INTERVAL       10
#define MAX_ROW_COUNT                           128
#define NAME_VALUE_COUNT                        2 
#define CCSP_ERR_WIFI_BUSY			503	

/* WebPA Configuration for RDKB */
#define RDKB_WEBPA_COMPONENT_NAME               "com.cisco.spvtg.ccsp.webpaagent"
#define RDKB_PAM_COMPONENT_NAME		        "com.cisco.spvtg.ccsp.pam"
#define RDKB_PAM_DBUS_PATH		        "/com/cisco/spvtg/ccsp/pam"
#define RDKB_WIFI_COMPONENT_NAME	        "com.cisco.spvtg.ccsp.wifi"
#define RDKB_WIFI_DBUS_PATH		        "/com/cisco/spvtg/ccsp/wifi"
#define RDKB_WIFI_FULL_COMPONENT_NAME	        "eRT.com.cisco.spvtg.ccsp.wifi"
#define RDKB_WEBPA_CFG_FILE                     "/nvram/webpa_cfg.json"
#define RDKB_WEBPA_CFG_FILE_SRC                 "/etc/webpa_cfg.json"
#define RDKB_WEBPA_CFG_DEVICE_INTERFACE         "erouter0"
#if defined(_COSA_BCM_MIPS_)
#define RDKB_WEBPA_DEVICE_MAC                   "Device.DPoE.Mac_address"
#else
#define RDKB_WEBPA_DEVICE_MAC                   "Device.DeviceInfo.X_COMCAST-COM_CM_MAC"
#endif
#define RDKB_WEBPA_DEVICE_REBOOT_PARAM          "Device.X_CISCO_COM_DeviceControl.RebootDevice"
#define RDKB_WEBPA_DEVICE_REBOOT_VALUE          "Device"
#define RDKB_XPC_SYNC_PARAM_CID                 "Device.DeviceInfo.Webpa.X_COMCAST-COM_CID"
#define RDKB_XPC_SYNC_PARAM_CMC                 "Device.DeviceInfo.Webpa.X_COMCAST-COM_CMC"
#define RDKB_FIRMWARE_VERSION		        "Device.DeviceInfo.X_CISCO_COM_FirmwareName"
#define RDKB_DEVICE_UP_TIME		        "Device.DeviceInfo.UpTime"
#define RDKB_MANUFACTURER		        "Device.DeviceInfo.Manufacturer"
#define RDKB_MODEL_NAME			        "Device.DeviceInfo.ModelName"
#define RDKB_XPC_SYNC_PARAM_SPV                 "Device.DeviceInfo.Webpa.X_COMCAST-COM_SyncProtocolVersion"
#define ALIAS_PARAM				"Alias"
#define RDKB_PARAM_HOSTS_NAME		        "Device.Hosts.Host."
#define RDKB_PARAM_HOSTS_VERSION	        "Device.Hosts.X_RDKCENTRAL-COM_HostVersionId"
#define RDKB_PARAM_SYSTEM_TIME		        "Device.DeviceInfo.X_RDKCENTRAL-COM_SystemTime"
#define STR_NOT_DEFINED                         "Not Defined"

#define RDKB_RECONNECT_REASON		        "Device.DeviceInfo.Webpa.X_RDKCENTRAL-COM_LastReconnectReason"
#define RDKB_REBOOT_REASON		        "Device.DeviceInfo.X_RDKCENTRAL-COM_LastRebootReason"

/* RDKB Logger defines */
#define LOG_FATAL       0
#define LOG_ERROR       1
#define LOG_WARN        2
#define LOG_NOTICE      3
#define LOG_INFO        4
#define LOG_DEBUG       5

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
typedef struct
{
	ULONG WebPaInstanceNumber;
	ULONG CcspInstanceNumber;
}CpeWebpaIndexMap;

typedef struct 
{
  int comp_id;   //Unique id for the component
  int comp_size;
  char *obj_name;
  char *comp_name;
  char *dbus_path;
}ComponentVal;

typedef struct 
{
  int parameterCount;   
  char **parameterName;
  char *comp_name;
  char *dbus_path;
}ParamCompList;

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
void (*notifyCbFn)(NotifyData*) = NULL;
static ComponentVal ComponentValArray[RDKB_TR181_OBJECT_LEVEL1_COUNT] = {'\0'};
ComponentVal SubComponentValArray[RDKB_TR181_OBJECT_LEVEL2_COUNT] = {'\0'};
BOOL bRadioRestartEn = FALSE;
BOOL bRestartRadio1 = FALSE;
BOOL bRestartRadio2 = FALSE;
BOOL applySettingsFlag = FALSE;
pthread_mutex_t applySetting_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t applySetting_cond = PTHREAD_COND_INITIALIZER;
int compCacheSuccessCnt = 0, subCompCacheSuccessCnt = 0;
static char *CcspDmlName[WIFI_PARAM_MAP_SIZE] = {"Device.WiFi.Radio", "Device.WiFi.SSID", "Device.WiFi.AccessPoint"};
static char current_transaction_id[MAX_PARAMETERVALUE_LEN] = {'\0'};
static CpeWebpaIndexMap IndexMap[WIFI_INDEX_MAP_SIZE] = {
{10000, 1},
{10100, 2},
{10001, 1},
{10002, 3},
{10003, 5},
{10004, 7},
{10005, 9},
{10006, 11},
{10007, 13},
{10008, 15},
{10101, 2},
{10102, 4},
{10103, 6},
{10104, 8},
{10105, 10},
{10106, 12},
{10107, 14},
{10108, 16}
};
static char *objectList[] ={
"Device.WiFi.",
"Device.DeviceInfo.",
"Device.GatewayInfo.",
"Device.Time.",
"Device.UserInterface.",
"Device.InterfaceStack.",
"Device.Ethernet.",
"Device.MoCA.",
"Device.PPP.",
"Device.IP.",
"Device.Routing.",
"Device.DNS.",
"Device.Firewall.",
"Device.NAT.",
"Device.DHCPv4.",
"Device.DHCPv6.",
"Device.Users.",
"Device.UPnP.",
"Device.X_CISCO_COM_DDNS.",
"Device.X_CISCO_COM_Security.",
"Device.X_CISCO_COM_DeviceControl.",
"Device.Bridging.",
"Device.RouterAdvertisement.",
"Device.NeighborDiscovery.",
"Device.IPv6rd.",
"Device.X_CISCO_COM_MLD.",
#if defined(_COSA_BCM_MIPS_)
"Device.DPoE.",
#else
"Device.X_CISCO_COM_CableModem.",
#endif
"Device.X_Comcast_com_ParentalControl.",
"Device.X_CISCO_COM_Diagnostics.",
"Device.X_CISCO_COM_MultiLAN.",
"Device.X_COMCAST_COM_GRE.",
"Device.X_CISCO_COM_GRE.",
"Device.Hosts.",
"Device.ManagementServer."
};
 
static char *subObjectList[] = 
{
"Device.DeviceInfo.NetworkProperties.",
"Device.MoCA.X_CISCO_COM_WiFi_Extender.",
"Device.MoCA.Interface.",
"Device.IP.Diagnostics.",
"Device.IP.Interface.",
"Device.DNS.Diagnostics.",
"Device.DNS.Client.",
"Device.DeviceInfo.VendorConfigFile.",
"Device.DeviceInfo.MemoryStatus.",
"Device.DeviceInfo.ProcessStatus.",
"Device.DeviceInfo.Webpa.",
"Device.DeviceInfo.SupportedDataModel."
}; 

const char * notifyparameters[]={
"Device.Bridging.Bridge.1.Port.8.Enable",
"Device.Bridging.Bridge.2.Port.2.Enable",
"Device.DeviceInfo.X_COMCAST_COM_xfinitywifiEnable",
"Device.WiFi.AccessPoint.10001.Security.ModeEnabled",
"Device.WiFi.AccessPoint.10001.Security.X_COMCAST-COM_KeyPassphrase",
"Device.WiFi.AccessPoint.10001.SSIDAdvertisementEnabled",
"Device.WiFi.AccessPoint.10001.X_CISCO_COM_MACFilter.Enable",
"Device.WiFi.AccessPoint.10001.X_CISCO_COM_MACFilter.FilterAsBlackList",
"Device.WiFi.AccessPoint.10101.Security.ModeEnabled",
"Device.WiFi.AccessPoint.10101.Security.X_COMCAST-COM_KeyPassphrase",
"Device.WiFi.AccessPoint.10101.SSIDAdvertisementEnabled",
"Device.WiFi.AccessPoint.10101.X_CISCO_COM_MACFilter.Enable",
"Device.WiFi.AccessPoint.10101.X_CISCO_COM_MACFilter.FilterAsBlackList",
"Device.WiFi.Radio.10000.Enable",
"Device.WiFi.Radio.10100.Enable",
"Device.WiFi.SSID.10001.Enable",
"Device.WiFi.SSID.10001.SSID",
"Device.WiFi.SSID.10101.Enable",
"Device.WiFi.SSID.10101.SSID",
"Device.X_CISCO_COM_DeviceControl.LanManagementEntry.1.LanMode",
"Device.X_CISCO_COM_Security.Firewall.FilterAnonymousInternetRequests",
"Device.X_CISCO_COM_Security.Firewall.FilterHTTP",
"Device.X_CISCO_COM_Security.Firewall.FilterIdent",
"Device.X_CISCO_COM_Security.Firewall.FilterMulticast",
"Device.X_CISCO_COM_Security.Firewall.FilterP2P",
"Device.X_CISCO_COM_Security.Firewall.FirewallLevel",
// Commenting this parameter to avoid PSM crash
//"Device.DHCPv4.Server.Enable",
"Device.X_CISCO_COM_DeviceControl.LanManagementEntry.1.LanIPAddress",
"Device.X_CISCO_COM_DeviceControl.LanManagementEntry.1.LanSubnetMask",
"Device.DHCPv4.Server.Pool.1.MinAddress",
"Device.DHCPv4.Server.Pool.1.MaxAddress",
"Device.DHCPv4.Server.Pool.1.LeaseTime",
// Commenting as some boxes may not have this parameter
//"Device.Routing.Router.1.IPv4Forwarding.1.GatewayIPAddress",
"Device.NAT.X_CISCO_COM_DMZ.Enable",
"Device.NAT.X_CISCO_COM_DMZ.InternalIP",
"Device.NAT.X_CISCO_COM_DMZ.IPv6Host",
"Device.NAT.X_Comcast_com_EnablePortMapping",
"Device.NotifyComponent.X_RDKCENTRAL-COM_Connected-Client"
};

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void ccspSystemReadySignalCB(void* user_data);
static void waitUntilSystemReady();
static int checkIfSystemReady();
static WAL_STATUS mapStatus(int ret);
void ccspWebPaValueChangedCB(parameterSigStruct_t* val, int size,void* user_data);
static PARAMVAL_CHANGE_SOURCE mapWriteID(unsigned int writeID);
static int getParamValues(char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int getAtomicParamValues(char *pParameterName[], int paramCount, char *CompName, char *dbusPath, money_trace_spans *timeSpan,int compIndex, ParamVal ***parametervalArr, int startIndex,int *TotalParams);
static int getAtomicParamAttributes(char *parameterNames[], int paramCount, char *CompName, char *dbusPath, money_trace_spans *timeSpan, AttrVal ***attr, int startIndex);
static void free_ParamCompList(ParamCompList *ParamGroup, int compCount);
static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamValues(ParamVal paramVal[], int paramCount, const WEBPA_SET_TYPE setType, money_trace_spans *timeSpan,char * transaction_id);
static int setAtomicParamValues(ParamVal *paramVal, char *CompName, char *dbusPath, int paramCount,const WEBPA_SET_TYPE setType,char * transaction_id, uint32_t *duration);
static int setAtomicParamAttributes(const char *pParameterName[], const AttrVal **attArr,int paramCount, money_trace_spans *timeSpan);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);
static int prepare_parameterValueStruct(parameterValStruct_t* val, ParamVal *paramVal, char *paramName);
static void free_set_param_values_memory(parameterValStruct_t* val, int paramCount, char * faultParam);
static void free_paramVal_memory(ParamVal** val, int paramCount);
static void identifyRadioIndexToReset(int paramCount, parameterValStruct_t* val,BOOL *bRestartRadio1,BOOL *bRestartRadio2);
static int IndexMpa_WEBPAtoCPE(char *pParameterName);
static void IndexMpa_CPEtoWEBPA(char **ppParameterName);
static int getMatchingComponentValArrayIndex(char *objectName);
static int getMatchingSubComponentValArrayIndex(char *objectName);
static void getObjectName(char *str, char *objectName, int objectLevel);
static int getComponentInfoFromCache(char *parameterName, char *objectName, char *compName, char *dbusPath);
static void initApplyWiFiSettings();
static void *applyWiFiSettingsTask();
static int getComponentDetails(char *parameterName,char ***compName,char ***dbusPath, int * error, int *retCount);
static void prepareParamGroups(ParamCompList **ParamGroup,int paramCount,int cnt1,char *paramName,char *compName,char *dbusPath, int * compCount );
static void free_componentDetails(char **compName,char **dbusPath,int size);
extern void getCurrentTime(struct timespec *timer);
extern uint64_t getCurrentTimeInMicroSeconds(struct timespec *timer);
extern long timeValDiff(struct timespec *starttime, struct timespec *finishtime);
static int addRow(const char *object,char *compName,char *dbusPath,int *retIndex);
static int updateRow(char *objectName,TableData *list,char *compName,char *dbusPath);
static int deleteRow(const char *object);
static void getTableRows(const char *objectName,parameterValStruct_t **parameterval, int paramCount, int *numRows,char ***rowObjects);
static void contructRollbackTableData(parameterValStruct_t **parameterval,int paramCount,char ***rowList,int rowCount, int *numParam,TableData ** getList);
static int cacheTableData(const char *objectName,int paramcount,char ***rowList,int *numRows,int *params,TableData ** list);
static int addNewData(char *objectName,TableData * list,int paramcount);
static void deleteAllTableData(char **deleteList,int rowCount);
static void addCachedData(const char *objectName,TableData * addList,int rowCount);
static void getWritableParams(char *paramName, char ***writableParams, int *paramCount);
static void waitForComponentReady(char *compName, char *dbusPath);
static void checkComponentHealthStatus(char * compName, char * dbusPath, char *status, int *retStatus);
static void send_transaction_Notify();
extern ANSC_HANDLE bus_handle;

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
/**
 * @brief Registers the notification callback function.
 *
 * @param[in] cb Notification callback function.
 * @return WAL_STATUS.
 */
WAL_STATUS RegisterNotifyCB(notifyCB cb)
{
	notifyCbFn = cb;
	return WAL_SUCCESS;
}

void * getNotifyCB()
{
	WalPrint("Inside getNotifyCB\n");
	return notifyCbFn;
}

/**
 * @brief getComponentDetails Returns the compName list and dbusPath list
 *
 * @param[in] parameterName parameter Name
 * @param[out] compName component name array
 * @param[out] dbusPath dbuspath array
 * @param[out] error 
 * @param[out] retCount count of components	
 */
 
static int getComponentDetails(char *parameterName,char ***compName,char ***dbusPath, int *error, int *retCount)
{
	char objectName[MAX_PARAMETERNAME_LEN] = {'\0'};
	int index = -1,retIndex = 0,ret= -1, size = 0, i = 0;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	char **localCompName = NULL;
	char **localDbusPath = NULL;
	char tempParamName[MAX_PARAMETERNAME_LEN] = {'\0'};
	char tempCompName[MAX_PARAMETERNAME_LEN/2] = {'\0'};
	char tempDbusPath[MAX_PARAMETERNAME_LEN/2] = {'\0'};
	
	componentStruct_t ** ppComponents = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	walStrncpy(tempParamName, parameterName,sizeof(tempParamName));
	WalPrint("======= start of getComponentDetails ========\n");
	index = getComponentInfoFromCache(tempParamName, objectName, tempCompName, tempDbusPath);
	WalPrint("index : %d\n",index);
	// Cannot identify the component from cache, make DBUS call to fetch component
	if(index == -1 || ComponentValArray[index].comp_size > 2) //anything above size > 2
	{
		WalPrint("in if for size >2\n");
		// GET Component for parameter from stack
		if(index != -1)
		        WalPrint("ComponentValArray[index].comp_size : %d\n",ComponentValArray[index].comp_size);
		retIndex = IndexMpa_WEBPAtoCPE(tempParamName);
		if(retIndex == -1)
		{
			ret = CCSP_ERR_INVALID_PARAMETER_NAME;
			WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
			*error = 1;
			return ret;
		}
		WalPrint("Get component for parameterName : %s from stack\n",tempParamName);

		ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, tempParamName, l_Subsystem, &ppComponents, &size);
		WalPrint("size : %d, ret : %d\n",size,ret);

		if (ret == CCSP_SUCCESS)
		{
			localCompName = (char **) malloc (sizeof(char*) * size);
			localDbusPath = (char **) malloc (sizeof(char*) * size);
			for(i = 0; i < size; i++)
			{
				localCompName[i] = (char *) malloc (sizeof(char) * MAX_PARAMETERNAME_LEN);
				localDbusPath[i] = (char *) malloc (sizeof(char) * MAX_PARAMETERNAME_LEN);
				strcpy(localCompName[i],ppComponents[i]->componentName);
				strcpy(localDbusPath[i],ppComponents[i]->dbusPath);
				WalPrint("localCompName[%d] : %s, localDbusPath[%d] : %s\n",i,localCompName[i],i, localDbusPath[i]);
			}
			
			*retCount = size;
			free_componentStruct_t(bus_handle, size, ppComponents);
		}
		else
		{
			WalError("Parameter name %s is not supported. ret = %d\n", tempParamName, ret);
			free_componentStruct_t(bus_handle, size, ppComponents);
			*error = 1;
			return ret;
		}
	}
	else
	{
		localCompName = (char **) malloc (sizeof(char*)*1);
		localDbusPath = (char **) malloc (sizeof(char*)*1);
		localCompName[0] = (char *) malloc (sizeof(char) * MAX_PARAMETERNAME_LEN);
		localDbusPath[0] = (char *) malloc (sizeof(char) * MAX_PARAMETERNAME_LEN);
		strcpy(localCompName[0],tempCompName);
		strcpy(localDbusPath[0],tempDbusPath);
		*retCount = 1;
		size = 1;
		WalPrint("localCompName[0] : %s, localDbusPath[0] : %s\n",localCompName[0], localDbusPath[0]);
	}
	
	*compName = localCompName;
	*dbusPath = localDbusPath;
	for(i = 0; i < size; i++)
	{
		WalPrint("(*compName)[%d] : %s, (*dbusPath)[%d] : %s\n",i,(*compName)[i],i, (*dbusPath)[i]);
	}
	WalPrint("======= End of getComponentDetails ret =%d ========\n",ret);
	return CCSP_SUCCESS;
}

static void prepareParamGroups(ParamCompList **ParamGroup,int paramCount,int cnt1,char *paramName,char *compName,char *dbusPath, int * compCount )
{
        int cnt2 =0, subParamCount =0,matchFlag = 0, tempCount=0;
        tempCount =*compCount;
        ParamCompList *localParamGroup = *ParamGroup;
        WalPrint("============ start of prepareParamGroups ===========\n");
        if(*ParamGroup == NULL)
        {
                WalPrint("ParamCompList is null initializing\n");
                localParamGroup = (ParamCompList *) malloc(sizeof(ParamCompList));
           	localParamGroup[0].parameterCount = 1;
           	localParamGroup[0].comp_name = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
                walStrncpy(localParamGroup[0].comp_name, compName,MAX_PARAMETERNAME_LEN/2);
                WalPrint("localParamGroup[0].comp_name : %s\n",localParamGroup[0].comp_name);
           	localParamGroup[0].dbus_path = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
                walStrncpy(localParamGroup[0].dbus_path, dbusPath,MAX_PARAMETERNAME_LEN/2);
                WalPrint("localParamGroup[0].dbus_path :%s\n",localParamGroup[0].dbus_path);
                // max number of parameter will be equal to the remaining parameters to be iterated (i.e. paramCount - cnt1)
                localParamGroup[0].parameterName = (char **) malloc(sizeof(char *) * (paramCount - cnt1));
           	localParamGroup[0].parameterName[0] = (char *) malloc(MAX_PARAMETERNAME_LEN);
           	walStrncpy(localParamGroup[0].parameterName[0],paramName,MAX_PARAMETERNAME_LEN);
                WalPrint("localParamGroup[0].parameterName[0] : %s\n",localParamGroup[0].parameterName[0]);
           	tempCount++;
        }
        else
        {
           	WalPrint("ParamCompList exists checking if parameter belongs to existing group\n");		
                WalPrint("compName %s\n",compName);
                for(cnt2 = 0; cnt2 < tempCount; cnt2++)
                {
                       WalPrint("localParamGroup[cnt2].comp_name %s \n",localParamGroup[cnt2].comp_name);
	                if(!strcmp(localParamGroup[cnt2].comp_name,compName))
	                {
		                WalPrint("Match found to already existing component group in ParamCompList, adding parameter to it\n");
		                localParamGroup[cnt2].parameterCount = localParamGroup[cnt2].parameterCount + 1;
		                subParamCount =  localParamGroup[cnt2].parameterCount;
		                WalPrint("subParamCount :%d\n",subParamCount);

		                localParamGroup[cnt2].parameterName[subParamCount-1] = (char *) malloc(MAX_PARAMETERNAME_LEN);

		                walStrncpy(localParamGroup[cnt2].parameterName[subParamCount-1],paramName,MAX_PARAMETERNAME_LEN);
		                WalPrint("localParamGroup[%d].parameterName :%s\n",cnt2,localParamGroup[cnt2].parameterName[subParamCount-1]);

		                matchFlag=1;
		                break;
	                }
                }
            	if(matchFlag != 1)
            	{
	                WalPrint("Parameter does not belong to existing component group, creating new group \n");

                      	localParamGroup =  (ParamCompList *) realloc(localParamGroup,sizeof(ParamCompList) * (tempCount + 1));
                      	localParamGroup[tempCount].parameterCount = 1;
                      	localParamGroup[tempCount].comp_name = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
	                walStrncpy(localParamGroup[tempCount].comp_name, compName,MAX_PARAMETERNAME_LEN/2);
                      	localParamGroup[tempCount].dbus_path = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
	                walStrncpy(localParamGroup[tempCount].dbus_path, dbusPath,MAX_PARAMETERNAME_LEN/2);

	                // max number of parameter will be equal to the remaining parameters to be iterated (i.e. paramCount - cnt1)
                      	localParamGroup[tempCount].parameterName = (char **) malloc(sizeof(char *) * (paramCount - cnt1));
	                localParamGroup[tempCount].parameterName[0] = (char *) malloc(MAX_PARAMETERNAME_LEN);
                      	walStrncpy(localParamGroup[tempCount].parameterName[0],paramName,MAX_PARAMETERNAME_LEN);

                       	WalPrint("localParamGroup[%d].comp_name :%s\n",tempCount,localParamGroup[tempCount].comp_name);
                      	WalPrint("localParamGroup[%d].parameterName :%s\n",tempCount,localParamGroup[tempCount].parameterName[0]);
	                tempCount++;
            	}
        }
        *compCount = tempCount;
        *ParamGroup = localParamGroup; 
        WalPrint("============ End of prepareParamGroups compCount =%d===========\n",*compCount);
 	
 }
/**
 * @brief getValues Returns the parameter Names from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] paramValArr parameter value Array
 * @param[out] retValCount Number of parameters returned from stack
 * @param[out] retStatus Returns parameter Value from the stack
 */
void getValues(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, ParamVal ***paramValArr,
		int *retValCount, WAL_STATUS *retStatus)
{
	int cnt1=0,cnt2=0, ret = -1, startIndex = 0, error = 0, compCount=0, count = 0, i = 0,retCount = 0, totalParams = 0;
	char parameterName[MAX_PARAMETERNAME_LEN] = {'\0'};
	ParamCompList *ParamGroup = NULL;
	char **compName = NULL;
	char **dbusPath = NULL;
	
	for(cnt1 = 0; cnt1 < paramCount; cnt1++)
	{
		// Get the matching component index from cache
		walStrncpy(parameterName,paramName[cnt1],sizeof(parameterName));
		// To get list of component name and dbuspath
		ret = getComponentDetails(parameterName,&compName,&dbusPath,&error,&count);
		if(error == 1)
		{
			break;
		}
		WalPrint("parameterName: %s count: %d\n",parameterName,count);
		for(i = 0; i < count; i++)
		{
			WalPrint("compName[%d] : %s, dbusPath[%d] : %s\n", i,compName[i],i, dbusPath[i]);
		  	prepareParamGroups(&ParamGroup,paramCount,cnt1,parameterName,compName[i],dbusPath[i],&compCount);
	  	}
                free_componentDetails(compName,dbusPath,count);
	}//End of for loop
	   
	WalPrint("Number of parameter groups : %d\n",compCount);
	
	if(error != 1)
	{
		if(timeSpan)
		{
			timeSpan->spans = (money_trace_span *) malloc(sizeof(money_trace_span)* compCount);
			memset(timeSpan->spans,0,(sizeof(money_trace_span)* compCount));
			timeSpan->count = compCount;
		}
		for(cnt1 = 0; cnt1 < compCount; cnt1++)
		{
			WalPrint("********** Parameter group ****************\n");
		  	WalPrint("ParamGroup[%d].comp_name :%s, ParamGroup[%d].dbus_path :%s, ParamGroup[%d].parameterCount :%d\n",cnt1,ParamGroup[cnt1].comp_name, cnt1,ParamGroup[cnt1].dbus_path, cnt1,ParamGroup[cnt1].parameterCount);
		  	
		  	for(cnt2 = 0; cnt2 < ParamGroup[cnt1].parameterCount; cnt2++)
		  	{
			 	WalPrint("ParamGroup[%d].parameterName :%s\n",cnt1,ParamGroup[cnt1].parameterName[cnt2]);
		  	}
			if(!strcmp(ParamGroup[cnt1].comp_name,RDKB_WIFI_FULL_COMPONENT_NAME) && applySettingsFlag == TRUE) 
			{
				ret = CCSP_ERR_WIFI_BUSY;
				if(timeSpan)
				{
					timeSpan->count = 0;
					WAL_FREE(timeSpan->spans);					
				}
				break;
			}
		  	// GET atomic value call
			WalPrint("startIndex %d\n",startIndex);
		  	ret = getAtomicParamValues(ParamGroup[cnt1].parameterName, ParamGroup[cnt1].parameterCount, ParamGroup[cnt1].comp_name, ParamGroup[cnt1].dbus_path, timeSpan, cnt1, paramValArr, startIndex,&retCount);
			WalPrint("After getAtomic ParamValues :retCount = %d\n",retCount);
		  	if(ret != CCSP_SUCCESS)
		  	{
				WalError("Get Atomic Values call failed for ParamGroup[%d]->comp_name :%s ret: %d\n",cnt1,ParamGroup[cnt1].comp_name,ret);
				break;
		  	}
			startIndex = startIndex + retCount;
			totalParams = totalParams + retCount;
			WalPrint("totalParams : %d\n",totalParams);
		}
		if(timeSpan)
		{
		        WalPrint("timeSpan->count : %d\n",timeSpan->count);
		}
	}
	retValCount[0] = totalParams;
	for (cnt1 = 0; cnt1 < paramCount; cnt1++)
	{
		retStatus[cnt1] = mapStatus(ret);	
	}
	free_ParamCompList(ParamGroup, compCount);	
}

/**
 * @brief To free allocated memory for ParamCompList
 *
 * @param[in] ParamGroup ParamCompList formed during GET request to group parameters based on components
 * @param[in] compCount number of components  
 */
static void free_ParamCompList(ParamCompList *ParamGroup, int compCount)
{
	int cnt1 = 0, cnt2 = 0;
	for(cnt1 = 0; cnt1 < compCount; cnt1++)
	{
	  	for(cnt2 = 0; cnt2 < ParamGroup[cnt1].parameterCount; cnt2++)
	  	{
	     	        WAL_FREE(ParamGroup[cnt1].parameterName[cnt2]);
	  	}
		WAL_FREE(ParamGroup[cnt1].parameterName);
		WAL_FREE(ParamGroup[cnt1].comp_name);
		WAL_FREE(ParamGroup[cnt1].dbus_path);
	}
	WAL_FREE(ParamGroup);
}

/**
 * @brief To free allocated memory to get component details
 *
 * @param[in] compName component name list 
 * @param[in] dbusPath dbuspath list 
 * @param[in] size number of components  
 */
static void free_componentDetails(char **compName,char **dbusPath,int size)
{
	int i;
	for(i = 0; i < size; i++)
	{
		WAL_FREE(compName[i]);
		WAL_FREE(dbusPath[i]);	
	}

	WAL_FREE(compName);
	WAL_FREE(dbusPath);
}
static int getComponentInfoFromCache(char *parameterName, char *objectName, char *compName, char *dbusPath)
{   
	int index = -1;	
	
	getObjectName(parameterName, objectName, 1);
	index = getMatchingComponentValArrayIndex(objectName);
	WalPrint("objectLevel: 1, parameterName: %s, objectName: %s, matching index: %d\n",parameterName,objectName,index);
		 
	if((index != -1) && (ComponentValArray[index].comp_size == 1))
	{
		strcpy(compName,ComponentValArray[index].comp_name);
		strcpy(dbusPath,ComponentValArray[index].dbus_path);
	}
	else if((index != -1) && (ComponentValArray[index].comp_size == 2))
	{
		getObjectName(parameterName, objectName, 2);
		index = getMatchingSubComponentValArrayIndex(objectName);
		WalPrint("objectLevel: 2, parameterName: %s, objectName: %s, matching index=%d\n",parameterName,objectName,index);
		if(index != -1 )
		{
			strcpy(compName,SubComponentValArray[index].comp_name);
			strcpy(dbusPath,SubComponentValArray[index].dbus_path); 
		}
    	}
	return index;	
}

/**
 * @brief getAttributes Returns the parameter Value Attributes from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[out] timeSpan timing_values for each component.
 * @param[in] paramValArr parameter value Array
 * @param[out] retValCount Number of parameters returned from stack
 * @param[out] retStatus Returns parameter Value from the stack
 */
void getAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
{
	int cnt1=0,cnt2=0, ret = -1, startIndex = 0,error = 0, compCount=0, count =0, i= 0;
	char parameterName[MAX_PARAMETERNAME_LEN] = {'\0'};
	ParamCompList *ParamGroup = NULL;
	char **compName = NULL;
	char **dbusPath = NULL;
	
	for(cnt1 = 0; cnt1 < paramCount; cnt1++)
	{
		// Get the matching component index from cache
		walStrncpy(parameterName,paramName[cnt1],sizeof(parameterName));
		// To get list of component name and dbuspath
		ret = getComponentDetails(parameterName,&compName,&dbusPath,&error,&count);
		if(error == 1)
		{
			break;
		}
		WalPrint("parameterName: %s count : %d\n",parameterName,count);
		for(i = 0; i < count; i++)
		{
			WalPrint("compName[%d] : %s, dbusPath[%d] : %s\n", i,compName[i],i, dbusPath[i]);
		  	prepareParamGroups(&ParamGroup,paramCount,cnt1,parameterName,compName[i],dbusPath[i],&compCount);
		}
        	free_componentDetails(compName,dbusPath,count);
	}//End of for loop
	   
	WalPrint("Number of parameter groups : %d\n",compCount);
	if(error != 1)
	{
		if(timeSpan)
		{
			timeSpan->spans = (money_trace_span *) malloc(sizeof(money_trace_span)* compCount);
			memset(timeSpan->spans,0,(sizeof(money_trace_span)* compCount));
		}
		for(cnt1 = 0; cnt1 < compCount; cnt1++)
		{
			WalPrint("********** Parameter group ****************\n");
		  	WalInfo("ParamGroup[%d].comp_name :%s, ParamGroup[%d].dbus_path :%s, ParamGroup[%d].parameterCount :%d\n",cnt1,ParamGroup[cnt1].comp_name, cnt1,ParamGroup[cnt1].dbus_path, cnt1,ParamGroup[cnt1].parameterCount);
		  	
		  	for(cnt2 = 0; cnt2 < ParamGroup[cnt1].parameterCount; cnt2++)
		  	{
		 		WalPrint("ParamGroup[%d].parameterName :%s\n",cnt1,ParamGroup[cnt1].parameterName[cnt2]);
		  	}
			if(!strcmp(ParamGroup[cnt1].comp_name,RDKB_WIFI_FULL_COMPONENT_NAME)&& applySettingsFlag == TRUE) 
			{
				ret = CCSP_ERR_WIFI_BUSY;
				if(timeSpan)
				{
					timeSpan->count = 0;					
					WAL_FREE(timeSpan->spans);
				}	
				break;
			}
		  	// GET atomic value call
			WalPrint("startIndex %d\n",startIndex);
		  	ret = getAtomicParamAttributes(ParamGroup[cnt1].parameterName, ParamGroup[cnt1].parameterCount, ParamGroup[cnt1].comp_name, ParamGroup[cnt1].dbus_path, timeSpan, attr, startIndex);
		  	if(ret != CCSP_SUCCESS)
		  	{
				WalError("Get Atomic Values call failed for ParamGroup[%d]->comp_name :%s ret: %d\n",cnt1,ParamGroup[cnt1].comp_name,ret);
				break;
		  	}
		        if(timeSpan)
			{
			        timeSpan->count++;
			}
			startIndex = startIndex + ParamGroup[cnt1].parameterCount;
		}
		if(timeSpan)
		{
		        WalPrint("timeSpan->count : %d\n",timeSpan->count);
		}
	}
	
	for (cnt1 = 0; cnt1 < paramCount; cnt1++)
	{
		retStatus[cnt1] = mapStatus(ret);
		retAttrCount[cnt1] = 1;	
	}
	
	free_ParamCompList(ParamGroup, compCount);
}

/**
 * @brief setValues Sets the parameter value
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] setType Set operation type
 * @param[out] timeSpan timing_values for each component.
 * @param[out] retStatus Returns parameter Value from the stack
 */

void setValues(const ParamVal paramVal[], const unsigned int paramCount, const WEBPA_SET_TYPE setType, money_trace_spans *timeSpan, WAL_STATUS *retStatus,char * transaction_id)
{
        int cnt = 0, ret = 0, cnt1 =0, i = 0, count = 0, error = 0, compCount = 0, cnt2= 0, j = 0;
        int startIndex = 0,retCount = 0,checkSetstatus = 0,totalParams = 0,rev=0,indexWifi= -1,getFlag=0;
        char parameterName[MAX_PARAMETERNAME_LEN] = {'\0'};
        ParamCompList *ParamGroup = NULL;
        char **compName = NULL;
        char **dbusPath = NULL;
        ParamVal **val = NULL;
        ParamVal **rollbackVal = NULL;
        uint32_t duration = 0;
        
        ParamVal ***storeGetValue = NULL;// To store param values before failure occurs

        WalPrint("=============== Start of setValues =============\n");
        for(cnt1 = 0; cnt1 < paramCount; cnt1++)
        {
                walStrncpy(parameterName,paramVal[cnt1].name,sizeof(parameterName));
                // To get list of component name and dbuspath
                ret = getComponentDetails(parameterName,&compName,&dbusPath,&error,&count);
                if(error == 1)
                {
                        break;
                }
                WalPrint("parameterName: %s count: %d\n",parameterName,count);
                for(i = 0; i < count; i++)
                {
                        WalPrint("compName[%d] : %s, dbusPath[%d] : %s\n", i,compName[i],i, dbusPath[i]);
                        prepareParamGroups(&ParamGroup,paramCount,cnt1,parameterName,compName[i],dbusPath[i],&compCount);
                }
                free_componentDetails(compName,dbusPath,count);
        }

        if(error != 1)
        {
                WalInfo("Number of parameter groups : %d\n",compCount);

                val = (ParamVal **) malloc(sizeof(ParamVal *) * compCount);
                memset(val,0,(sizeof(ParamVal *) * compCount));

                rollbackVal = (ParamVal **) malloc(sizeof(ParamVal *) * compCount);
                memset(rollbackVal,0,(sizeof(ParamVal *) * compCount));

                storeGetValue = (ParamVal ***)malloc(sizeof(ParamVal **) * compCount);
                memset(storeGetValue,0,(sizeof(ParamVal **) * compCount));
                
                if(timeSpan)
                {
                        timeSpan->spans = (money_trace_span *) malloc(sizeof(money_trace_span)* compCount);
                        memset(timeSpan->spans,0,(sizeof(money_trace_span)* compCount));
                        timeSpan->count = compCount;
                }
		
                for(j = 0; j < compCount ;j++)
                {
                        WalInfo("ParamGroup[%d].comp_name :%s, ParamGroup[%d].dbus_path :%s, ParamGroup[%d].parameterCount :%d\n",j,ParamGroup[j].comp_name, j,ParamGroup[j].dbus_path, j,ParamGroup[j].parameterCount);

                        val[j] = (ParamVal *) malloc(sizeof(ParamVal) * ParamGroup[j].parameterCount);
                        rollbackVal[j] = (ParamVal *) malloc(sizeof(ParamVal) * ParamGroup[j].parameterCount);

                        storeGetValue[j] = (ParamVal **)malloc(sizeof(ParamVal *) * ParamGroup[j].parameterCount);
                }

                WalPrint("--------- Start of SET Atomic caching -------\n");
                for (i = 0; i < compCount; i++)
                {
                        //GET values for rollback purpose
                        ret = getAtomicParamValues(ParamGroup[i].parameterName, ParamGroup[i].parameterCount, ParamGroup[i].comp_name, ParamGroup[i].dbus_path, timeSpan, i, &storeGetValue[i], startIndex,&retCount);

                        WalPrint("After GAPV ret = %d :retCount = %d\n",ret,retCount);
                        if(ret != CCSP_SUCCESS)
                        {
                                WalError("Get Atomic Values call failed for ParamGroup[%d]->comp_name :%s ret: %d\n",i,ParamGroup[i].comp_name,ret);
                                getFlag = 1;

                                WalPrint("------ Free for storeGetValue ------\n");

                                for(cnt1=i-1;cnt1>=0;cnt1--)
                                {
                                        WalPrint("cnt1 inside free for loop is %d\n",cnt1);
                                        for(j=0;j<ParamGroup[cnt1].parameterCount;j++)
                                        {
                                                WAL_FREE(storeGetValue[cnt1][j]->name);
                                                WAL_FREE(storeGetValue[cnt1][j]->value);
                                                WalPrint("Freeing storeGetValue[cnt1][j] in failure case\n");
                                                WAL_FREE(storeGetValue[cnt1][j]);
                                        }
                                        WAL_FREE(storeGetValue[cnt1]);
                                }

                                break;
                        }
                        else
                        {		 
                                for(j = 0; j < retCount ; j++)
                                {
                                        WalPrint("storeGetValue[%d][%d]->name : %s, storeGetValue[%d][%d]->value : %s, storeGetValue[%d][%d]->type : %d\n",i,j,storeGetValue[i][j]->name,i,j,storeGetValue[i][j]->value,i,j,storeGetValue[i][j]->type);

                                        rollbackVal[i][j].name=storeGetValue[i][j]->name;
                                        rollbackVal[i][j].value=storeGetValue[i][j]->value;
                                        rollbackVal[i][j].type=storeGetValue[i][j]->type;

                                        WalPrint("rollbackVal[%d][%d].name : %s, rollbackVal[%d][%d].value : %s, rollbackVal[%d][%d].type : %d\n",i,j,rollbackVal[i][j].name,i,j,rollbackVal[i][j].value,i,j,rollbackVal[i][j].type);
                                }		  	    		  	    		  	    
                        }
                }
                WalPrint("--------- End of SET Atomic caching -------\n");
                if(getFlag !=1)
                {		    
                        WalPrint("---- Start of preparing val struct ------\n");
                        for(cnt1 = 0; cnt1 < paramCount; cnt1++)
                        {
                                for(j = 0; j < compCount ;j++)
                                {
                                        for(cnt2 = 0; cnt2 < ParamGroup[j].parameterCount; cnt2++)
                                        {
                                                WalPrint("ParamGroup[%d].parameterName[%d] :%s\n",j,cnt2,ParamGroup[j].parameterName[cnt2]);
                                                WalPrint("paramVal[%d].name : %s\n",cnt1,paramVal[cnt1].name);
                                                if(strcmp(paramVal[cnt1].name,ParamGroup[j].parameterName[cnt2]) == 0)
                                                {
                                                        WalPrint("paramVal[%d].name : %s \n",cnt1,paramVal[cnt1].name);
                                                        val[j][cnt2].name = paramVal[cnt1].name;
                                                        WalPrint("val[%d][%d].name : %s \n",j,cnt2,val[j][cnt2].name);
                                                        WalPrint("paramVal[%d].value : %s\n",cnt1,paramVal[cnt1].value);
                                                        val[j][cnt2].value = paramVal[cnt1].value;
                                                        WalPrint("val[%d][%d].value : %s \n",j,cnt2,val[j][cnt2].value);
                                                        val[j][cnt2].type = paramVal[cnt1].type;
                                                }
                                        }
                                }
                        }//End of for loop
                        WalPrint("---- End of preparing val struct ------\n");

                        for (i = 0; i < compCount; i++)
                        {
                                if(!strcmp(ParamGroup[i].comp_name,RDKB_WIFI_FULL_COMPONENT_NAME) && applySettingsFlag == TRUE)
                                {
                                        ret = CCSP_ERR_WIFI_BUSY;
                                        break;
                                }			

                                // Skip and do SET for Wifi component at the end 
                                if(!strcmp(ParamGroup[i].comp_name,RDKB_WIFI_FULL_COMPONENT_NAME))
                                {
                                        WalPrint("skip wifi set and get the index %d\n",i);
                                        indexWifi = i;
                                }
                                else
                                {
                                        WalPrint("ParamGroup[%d].comp_name : %s\n",i,ParamGroup[i].comp_name);
                                        ret = setAtomicParamValues(val[i], ParamGroup[i].comp_name,ParamGroup[i].dbus_path,ParamGroup[i].parameterCount, setType,transaction_id,&duration);	
                                        WalPrint("ret : %d\n",ret);
                                        if(timeSpan)
                                        {
                                                WalPrint("timeSpan->spans[%d].name : %s\n",i,timeSpan->spans[i].name);
                                                WalPrint("duration : %lu\n",duration);
                                                timeSpan->spans[i].duration = timeSpan->spans[i].duration + duration;
                                                WalPrint("timeSpan->spans[%d].duration : %lu\n",i,timeSpan->spans[i].duration);
                                        }
                                        if(ret != CCSP_SUCCESS)
                                        {
                                                WalError("Failed to do atomic set hence rollbacking the changes. ret :%d\n",ret);
                                                WalPrint("------ Start of rollback ------\n");
                                                // Rollback data in failure case
                                                for(rev =i-1;rev>=0;rev--)
                                                {
                                                        WalPrint("rev value inside for loop is  %d\n",rev);
                                                        //skip for wifi rollback
                                                        if(indexWifi != rev)
                                                        {
                                                                WalPrint("ParamGroup[%d].comp_name : %s\n",rev,ParamGroup[rev].comp_name);
                                                                checkSetstatus = setAtomicParamValues(rollbackVal[rev],ParamGroup[rev].comp_name,ParamGroup[rev].dbus_path, ParamGroup[rev].parameterCount, setType,transaction_id,&duration);
                                                                WalPrint("checkSetstatus is : %d\n",checkSetstatus);
                                                                if(timeSpan)
                                                                {
                                                                        WalPrint("timeSpan->spans[%d].name : %s\n",rev,timeSpan->spans[rev].name);
                                                                        WalPrint("duration : %lu\n",duration);
                                                                        timeSpan->spans[rev].duration = timeSpan->spans[rev].duration + duration;
                                                                        WalPrint("timeSpan->spans[%d].duration : %lu\n",rev,timeSpan->spans[rev].duration);
                                                                }
                                                                if(checkSetstatus != CCSP_SUCCESS)
                                                                {
                                                                        WalError("While rollback Failed to do atomic set. checkSetstatus :%d\n",checkSetstatus);
                                                                }
                                                        }
                                                        else
                                                        {
                                                                WalPrint("Skip rollback for WiFi\n");
                                                                indexWifi = -1;
                                                        }
                                                }
                                                WalPrint("------ End of rollback ------\n");				
                                                break;
                                        }
                                }
                        }
                        //Got wifi index and do SET
                        if(indexWifi !=-1)
                        {
                                WalPrint("Wifi SET at end\n");
                                WalPrint("ParamGroup[%d].comp_name : %s\n",indexWifi,ParamGroup[indexWifi].comp_name);
                                ret = setAtomicParamValues(val[indexWifi], ParamGroup[indexWifi].comp_name,ParamGroup[indexWifi].dbus_path, ParamGroup[indexWifi].parameterCount, setType,transaction_id,&duration);
                                if(timeSpan)
                                {
                                        WalPrint("timeSpan->spans[%d].name : %s\n",indexWifi,timeSpan->spans[indexWifi].name);
                                        WalPrint("duration : %lu\n",duration);
                                        timeSpan->spans[indexWifi].duration = timeSpan->spans[indexWifi].duration + duration;
                                        WalPrint("timeSpan->spans[%d].duration : %lu\n",indexWifi,timeSpan->spans[indexWifi].duration);
                                }	
                                if(ret != CCSP_SUCCESS)
                                {
                                        WalError("Failed atomic set for WIFI hence rollbacking the changes. ret :%d and i is %d\n",ret,i);

                                        // Rollback data in failure case
                                        for(rev =i-1;rev>=0;rev--)
                                        {
                                                WalPrint("rev value inside for loop is  %d\n",rev);
                                                //skip for wifi rollback
                                                if(indexWifi != rev)
                                                {
                                                        WalPrint("ParamGroup[%d].comp_name : %s\n",rev,ParamGroup[rev].comp_name);
                                                        checkSetstatus = setAtomicParamValues(rollbackVal[rev], ParamGroup[rev].comp_name,ParamGroup[rev].dbus_path, ParamGroup[rev].parameterCount, setType,transaction_id,&duration);	
                                                        WalPrint("checkSetstatus is: %d\n",checkSetstatus);
                                                        if(timeSpan)
                                                        {
                                                                WalPrint("timeSpan->spans[%d].name : %s\n",rev,timeSpan->spans[rev].name);
                                                                WalPrint("duration : %lu\n",duration);
                                                                timeSpan->spans[rev].duration = timeSpan->spans[rev].duration + duration;
                                                                WalPrint("timeSpan->spans[%d].duration : %lu\n",rev,timeSpan->spans[rev].duration);
                                                        }
                                                        if(checkSetstatus != CCSP_SUCCESS)
                                                        {
                                                                WalError("While rollback Failed to do atomic set. checkSetstatus :%d\n",checkSetstatus);
                                                        }
                                                }
                                                else
                                                {
                                                        WalPrint("Skip rollback for WiFi\n");
                                                }	

                                        }
                                        WalPrint("------ End of rollback ------\n");
                                }
                        }

                        WalPrint("------ Free for storeGetValue ------\n");
                        for(i=0;i<compCount;i++)
                        {
                                for(j=0;j<ParamGroup[i].parameterCount;j++)
                                {
                                        WAL_FREE(storeGetValue[i][j]->name);
                                        WAL_FREE(storeGetValue[i][j]->value);
                                        WalPrint("Freeing storeGetValue[cnt1][j] in success case\n");
                                        WAL_FREE(storeGetValue[i][j]);
                                }
                                WAL_FREE(storeGetValue[i]);
                        }
                }
                WalPrint("------ Free for val ------\n");
                free_paramVal_memory(val,compCount);
                WalPrint("------ Free for rollbackVal ------\n");
                free_paramVal_memory(rollbackVal,compCount);
                WAL_FREE(storeGetValue);
        }
        else
        {
                WalError("Failed to get Component details\n");
        }

        WalPrint("------ Free for ParamGroup ------\n");
        free_ParamCompList(ParamGroup, compCount);
        //To set status
        for (cnt = 0; cnt < paramCount; cnt++) 
        {
                retStatus[cnt] = mapStatus(ret);
        }

        WalPrint("=============== End of setValues =============\n");
}

/**
 * @brief setAttributes Returns the status of parameter from stack for SET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[out] timeSpan timing_values for each component.
 * @param[in] attArr parameter value Array
 * @param[out] retStatus Returns parameter Value from the stack
 */
void setAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan,
		const AttrVal *attArr[], WAL_STATUS *retStatus)
{
	int cnt = 0, ret = 0;
	ret = setAtomicParamAttributes(paramName, attArr,paramCount, timeSpan);
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		retStatus[cnt] = mapStatus(ret);
	}

}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/*
* @brief WAL_STATUS mapStatus Defines WAL status values from corresponding ccsp values
* @param[in] ret ccsp status values from stack
*/
static WAL_STATUS mapStatus(int ret)
{
	switch (ret) 
	{
		case CCSP_SUCCESS:
			return WAL_SUCCESS;
		case CCSP_FAILURE:
			return WAL_FAILURE;
		case CCSP_ERR_TIMEOUT:
			return WAL_ERR_TIMEOUT;
		case CCSP_ERR_NOT_EXIST:
			return WAL_ERR_NOT_EXIST;
		case CCSP_ERR_INVALID_PARAMETER_NAME:
			return WAL_ERR_INVALID_PARAMETER_NAME;
		case CCSP_ERR_INVALID_PARAMETER_TYPE:
			return WAL_ERR_INVALID_PARAMETER_TYPE;
		case CCSP_ERR_INVALID_PARAMETER_VALUE:
			return WAL_ERR_INVALID_PARAMETER_VALUE;
		case CCSP_ERR_NOT_WRITABLE:
			return WAL_ERR_NOT_WRITABLE;
		case CCSP_ERR_SETATTRIBUTE_REJECTED:
			return WAL_ERR_SETATTRIBUTE_REJECTED;
		case CCSP_CR_ERR_NAMESPACE_OVERLAP:
			return WAL_ERR_NAMESPACE_OVERLAP;
		case CCSP_CR_ERR_UNKNOWN_COMPONENT:
			return WAL_ERR_UNKNOWN_COMPONENT;
		case CCSP_CR_ERR_NAMESPACE_MISMATCH:
			return WAL_ERR_NAMESPACE_MISMATCH;
		case CCSP_CR_ERR_UNSUPPORTED_NAMESPACE:
			return WAL_ERR_UNSUPPORTED_NAMESPACE;
		case CCSP_CR_ERR_DP_COMPONENT_VERSION_MISMATCH:
			return WAL_ERR_DP_COMPONENT_VERSION_MISMATCH;
		case CCSP_CR_ERR_INVALID_PARAM:
			return WAL_ERR_INVALID_PARAM;
		case CCSP_CR_ERR_UNSUPPORTED_DATATYPE:
			return WAL_ERR_UNSUPPORTED_DATATYPE;
		case CCSP_ERR_WIFI_BUSY:
			return WAL_ERR_WIFI_BUSY;
		default:
			return WAL_FAILURE;
	}
}
/**
 * @brief ccspWebPaValueChangedCB callback function for set notification
 *
 * @param[in] val parameterSigStruct_t notification struct got from stack
 * @param[in] size 
 * @param[in] user_data
 */
void ccspWebPaValueChangedCB(parameterSigStruct_t* val, int size, void* user_data)
{
	WalPrint("Inside CcspWebpaValueChangedCB\n");

	ParamNotify *paramNotify;

	if (NULL == notifyCbFn) 
	{
		WalError("Fatal: ccspWebPaValueChangedCB() notifyCbFn is NULL\n");
		return;
	}

	paramNotify= (ParamNotify *) malloc(sizeof(ParamNotify));
	paramNotify->paramName = val->parameterName;
	paramNotify->oldValue= val->oldValue;
	paramNotify->newValue = val->newValue;
	paramNotify->type = val->type;
	paramNotify->changeSource = mapWriteID(val->writeID);

	NotifyData *notifyDataPtr = (NotifyData *) malloc(sizeof(NotifyData) * 1);
	notifyDataPtr->type = PARAM_NOTIFY;
	Notify_Data *notify_data = (Notify_Data *) malloc(sizeof(Notify_Data) * 1);
	notify_data->notify = paramNotify;
	notifyDataPtr->data = notify_data;

	WalInfo("Notification Event from stack: Parameter Name: %s, Old Value: %s, New Value: %s, Data Type: %d, Change Source: %d\n",
			paramNotify->paramName, paramNotify->oldValue, paramNotify->newValue, paramNotify->type, paramNotify->changeSource);

	(*notifyCbFn)(notifyDataPtr);
}

/**
 * @brief sendIoTNotification function to send IoT notification
 *
 * @param[in] iotMsg IoT notification message
 * @param[in] size Size of IoT notification message
 */
void sendIoTNotification(void* iotMsg, int size)
{
	char* str = NULL;

	ParamNotify *paramNotify;

	if (NULL == notifyCbFn) 
	{
		WalError("Fatal: sendIoTNotification() notifyCbFn is NULL\n");
		return;
	}

	paramNotify = (ParamNotify *) malloc(sizeof(ParamNotify));
	paramNotify->paramName = (char *) malloc(8);
	walStrncpy(paramNotify->paramName, "IOT", 8);
	paramNotify->oldValue = NULL;

	if((str = (char*) malloc(size+1)) == NULL)
	{
		WalError("Error allocating memory in sendIoTNotification fn\n");
		WAL_FREE(iotMsg);
		return;
	}
	sprintf(str, "%.*s", size, (char *)iotMsg);
	//WAL_FREE(iotMsg); //TODO: Free memory?
	paramNotify->newValue = str;
	paramNotify->type = WAL_STRING;
	paramNotify->changeSource = CHANGED_BY_UNKNOWN;
	
	NotifyData *notifyDataPtr = (NotifyData *) malloc(sizeof(NotifyData) * 1);
	notifyDataPtr->type = PARAM_NOTIFY;
	Notify_Data *notify_data = (Notify_Data *) malloc(sizeof(Notify_Data) * 1);
	notify_data->notify = paramNotify;
	notifyDataPtr->data = notify_data;

	WalInfo("Notification Event from IoT: Parameter Name: %s, New Value: %s, Data Type: %d, Change Source: %d\n",
			paramNotify->paramName, paramNotify->newValue, paramNotify->type, paramNotify->changeSource);

	(*notifyCbFn)(notifyDataPtr);
}

static PARAMVAL_CHANGE_SOURCE mapWriteID(unsigned int writeID)
{
	PARAMVAL_CHANGE_SOURCE source;
	WalPrint("Inside mapWriteID\n");
	WalPrint("WRITE ID is %d\n", writeID);

	switch(writeID)
	{
		case CCSP_COMPONENT_ID_ACS:
			source = CHANGED_BY_ACS;
			break;
		case CCSP_COMPONENT_ID_WebPA:
			source = CHANGED_BY_WEBPA;
			break;
		case CCSP_COMPONENT_ID_XPC:
			source = CHANGED_BY_XPC;
			break;
		case DSLH_MPA_ACCESS_CONTROL_CLIENTTOOL:
			source = CHANGED_BY_CLI;
			break;
		case CCSP_COMPONENT_ID_SNMP:
			source = CHANGED_BY_SNMP;
			break;
		case CCSP_COMPONENT_ID_WebUI:
			source = CHANGED_BY_WEBUI;
			break;
		default:
			source = CHANGED_BY_UNKNOWN;
			break;
	}

	WalPrint("CMC/component_writeID is: %d\n", source);
	return source;
}

/**
 * @brief getParamValues Returns the parameter Values from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramValArr parameter value Array
 * @param[out] TotalParams Number of parameters returned from stack
 */
static int getParamValues(char *pParameterName, ParamVal ***parametervalArr, int *TotalParams)
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret = 0, i = 0, size = 0, val_size = 0;
	char *parameterNames[1];
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char *p = &paramName;
	
	componentStruct_t ** ppComponents = NULL;
	parameterValStruct_t **parameterval = NULL;
	parameterValStruct_t *parametervalError = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	walStrncpy(paramName, pParameterName,sizeof(paramName));
	snprintf(dst_pathname_cr,sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1)
	{
		parameterNames[0] = p;
		ret = CcspBaseIf_getParameterValues(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				parameterNames,
				1, &val_size, &parameterval);

		if (ret != CCSP_SUCCESS)
		{
			parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal *) * 1);
			parametervalError = (parameterValStruct_t *) malloc(sizeof(parameterValStruct_t));
			parametervalError->parameterValue = NULL;
			parametervalError->parameterName = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN);
			walStrncpy(parametervalError->parameterName,pParameterName,MAX_PARAMETERNAME_LEN);
			parametervalError->type = ccsp_string;
			parametervalArr[0][0] = parametervalError;
			*TotalParams = 1;
			WalError("Failed to GetValue for param: %s ret: %d\n", paramName, ret);

		}
		else
		{
			*TotalParams = val_size;
			parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*) * val_size);
			for (i = 0; i < val_size; i++)
			{
				IndexMpa_CPEtoWEBPA(&parameterval[i]->parameterName);

				IndexMpa_CPEtoWEBPA(&parameterval[i]->parameterValue);

				parametervalArr[0][i] = parameterval[i];
				WalPrint("success: %s %s %d \n",parametervalArr[0][i]->name,parametervalArr[0][i]->value,parametervalArr[0][i]->type);
			}
			WAL_FREE(parameterval);
		}
	}
	else
	{
		parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal *) * 1);
		parametervalError = (parameterValStruct_t *) malloc(sizeof(parameterValStruct_t));
		parametervalError->parameterValue = NULL;
		parametervalError->parameterName = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN);
		walStrncpy(parametervalError->parameterName,pParameterName,MAX_PARAMETERNAME_LEN);
		parametervalError->type = ccsp_string;
		parametervalArr[0][0] = parametervalError;
		*TotalParams = 1;
		WalError("Parameter name is not supported.ret : %d\n", ret);
	}
	free_componentStruct_t(bus_handle, size, ppComponents);
	return ret;
}


/**
 * @brief getAtomicParamValues Returns the parameter Values from stack for GET request in atomic way.
 * This is optimized as it fetches component from pre-populated ComponentValArray and does bulk GET 
 * for parameters belonging to same component
 *
 * @param[in] parameterNames parameter Names List
 * @param[in] paramCount number of parameters
 * @param[in] CompName Component Name of parameters
 * @param[in] dbusPath Dbus Path of component
 * @param[out] timeSpan timing_values for each component.
 * @param[out] paramValArr parameter value Array
 * @param[in] startIndex starting array index to fill the output paramValArr
 */
static int getAtomicParamValues(char *parameterNames[], int paramCount, char *CompName, char *dbusPath, money_trace_spans *timeSpan, int compIndex, ParamVal ***parametervalArr, int startIndex,int *TotalParams)
{
	int ret = 0, val_size = 0, cnt=0, retIndex=0, error=0;
	char **parameterNamesLocal = NULL;
	parameterValStruct_t **parameterval = NULL;
	uint64_t startTime = 0, endTime = 0;
	struct timespec start, end;
	uint32_t timeDuration = 0;
	WalPrint(" ------ Start of getAtomicParamValues ----\n");
	parameterNamesLocal = (char **) malloc(sizeof(char *) * paramCount);
	memset(parameterNamesLocal,0,(sizeof(char *) * paramCount));

	// Initialize names array with converted index	
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		WalPrint("Before parameterNames[%d] : %s\n",cnt,parameterNames[cnt]);
	
		parameterNamesLocal[cnt] = (char *) malloc(sizeof(char) * (strlen(parameterNames[cnt]) + 1));
		strcpy(parameterNamesLocal[cnt],parameterNames[cnt]);

		retIndex=IndexMpa_WEBPAtoCPE(parameterNamesLocal[cnt]);
		if(retIndex == -1)
		{
		 	ret = CCSP_ERR_INVALID_PARAMETER_NAME;
		 	WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
			error = 1;
			if(timeSpan)
			{
				timeSpan->count = 0;
				WAL_FREE(timeSpan->spans);
			}
			break;
		}

		WalPrint("After parameterNamesLocal[%d] : %s\n",cnt,parameterNamesLocal[cnt]);
	}
	
	if(error != 1)
	{
		WalPrint("CompName = %s, dbusPath : %s, paramCount = %d\n", CompName, dbusPath, paramCount);
		if(timeSpan)
		{
			startTime = getCurrentTimeInMicroSeconds(&start);
			WalPrint("component start_time : %llu\n",startTime);
		}
		ret = CcspBaseIf_getParameterValues(bus_handle,CompName,dbusPath,parameterNamesLocal,paramCount, &val_size, &parameterval);
		if(timeSpan)
		{
			endTime = getCurrentTimeInMicroSeconds(&end);	
			timeDuration = endTime - startTime;
			timeSpan->spans[compIndex].name = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN/2);
			strcpy(timeSpan->spans[compIndex].name,CompName);
			WalPrint("timeSpan->spans[%d].name : %s\n",compIndex,timeSpan->spans[compIndex].name);
			WalPrint("start_time : %llu\n",startTime);
			timeSpan->spans[compIndex].start = startTime;
			WalPrint("timeSpan->spans[%d].start : %llu\n",compIndex,timeSpan->spans[compIndex].start);
			WalPrint("timeDuration : %lu\n",timeDuration);
			timeSpan->spans[compIndex].duration = timeDuration;
			WalPrint("timeSpan->spans[%d].duration : %lu\n",compIndex,timeSpan->spans[compIndex].duration);
			
		}
		WalPrint("----- After GPA ret = %d------\n",ret);
		if (ret != CCSP_SUCCESS)
		{
			WalError("Error:Failed to GetValue for parameters ret: %d\n", ret);
		}
		else
		{
			WalPrint("val_size : %d\n",val_size);
			if (val_size > 0)
			{
				if(startIndex == 0)
				{
					parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*) * val_size);
				}
				else
				{
					parametervalArr[0] = (ParamVal **) realloc(parametervalArr[0],sizeof(ParamVal*) * (startIndex + val_size));
				}
				for (cnt = 0; cnt < val_size; cnt++)
				{
					WalPrint("cnt+startIndex : %d\n",cnt+startIndex);
					IndexMpa_CPEtoWEBPA(&parameterval[cnt]->parameterName);

					IndexMpa_CPEtoWEBPA(&parameterval[cnt]->parameterValue);

					parametervalArr[0][cnt+startIndex] = parameterval[cnt];
					WalPrint("success: %s %s %d \n",parametervalArr[0][cnt+startIndex]->name,parametervalArr[0][cnt+startIndex]->value,parametervalArr[0][cnt+startIndex]->type);
				}
				*TotalParams = val_size;
				WAL_FREE(parameterval);
			}
			else if(val_size == 0 && ret == CCSP_SUCCESS)
			{
				WalError("No child elements found\n");
				*TotalParams = val_size;
				WAL_FREE(parameterval);				
			}
		}	
	}
		
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		WAL_FREE(parameterNamesLocal[cnt]);
	}
	WAL_FREE(parameterNamesLocal);
	return ret;
}


/**
 * @brief getParamAttributes Returns the parameter Values from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] attr parameter value Array
 * @param[out] TotalParams Number of parameters returned from stack
 */
 
static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams)
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int size = 0, ret = 0, sizeAttrArr = 0, x = 0;
	char *parameterNames[1];
	componentStruct_t ** ppComponents = NULL;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char *p = &paramName;
	
	parameterAttributeStruct_t** ppAttrArray = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	walStrncpy(paramName, pParameterName,sizeof(paramName));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem,  //prefix
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1)
	{
		parameterNames[0] = p;

		ret = CcspBaseIf_getParameterAttributes(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				parameterNames, 1, &sizeAttrArr, &ppAttrArray);

		if (CCSP_SUCCESS != ret)
		{
			attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * 1);
			attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
			attr[0][0]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
			attr[0][0]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);
			sprintf(attr[0][0]->value, "%d", -1);
			walStrncpy(attr[0][0]->name, pParameterName,MAX_PARAMETERNAME_LEN);
			attr[0][0]->type = WAL_INT;
			*TotalParams = 1;
			WalError("Failed to GetValue for GetParamAttr ret : %d \n", ret);
		}
		else
		{
			*TotalParams = sizeAttrArr;
			WalPrint("sizeAttrArr: %d\n",sizeAttrArr);
			attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * sizeAttrArr);
			for (x = 0; x < sizeAttrArr; x++)
			{
				attr[0][x] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
				attr[0][x]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
				attr[0][x]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);

				IndexMpa_CPEtoWEBPA(&ppAttrArray[x]->parameterName);
				walStrncpy(attr[0][x]->name, ppAttrArray[x]->parameterName,MAX_PARAMETERNAME_LEN);
				sprintf(attr[0][x]->value, "%d", ppAttrArray[x]->notification);
				attr[0][x]->type = WAL_INT;
			}
		}
		free_parameterAttributeStruct_t(bus_handle, sizeAttrArr, ppAttrArray);
		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * 1);
		attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
		attr[0][0]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
		attr[0][0]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);
		sprintf(attr[0][0]->value, "%d", -1);
		walStrncpy(attr[0][0]->name, pParameterName,MAX_PARAMETERNAME_LEN);
		attr[0][0]->type = WAL_INT;
		*TotalParams = 1;
		WalError("Parameter name is not supported.ret : %d\n", ret);
	}
	return ret;
}

static int getAtomicParamAttributes(char *parameterNames[], int paramCount, char *CompName, char *dbusPath, money_trace_spans *timeSpan, AttrVal ***attr, int startIndex)
{
	int ret = 0, sizeAttrArr = 0, cnt=0, retIndex=0, error=0;
	char **parameterNamesLocal = NULL;
	parameterAttributeStruct_t** ppAttrArray = NULL;
	uint64_t startTime = 0, endTime = 0;
	struct timespec start, end;
	uint32_t timeDuration = 0;
	WalPrint(" ------ Start of getAtomicParamAttributes ----\n");
	parameterNamesLocal = (char **) malloc(sizeof(char *) * paramCount);
	memset(parameterNamesLocal,0,(sizeof(char *) * paramCount));

	// Initialize names array with converted index	
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		WalPrint("Before parameterNames[%d] : %s\n",cnt,parameterNames[cnt]);
	
		parameterNamesLocal[cnt] = (char *) malloc(sizeof(char) * (strlen(parameterNames[cnt]) + 1));
		strcpy(parameterNamesLocal[cnt],parameterNames[cnt]);

		retIndex=IndexMpa_WEBPAtoCPE(parameterNamesLocal[cnt]);
		if(retIndex == -1)
		{
		 	ret = CCSP_ERR_INVALID_PARAMETER_NAME;
		 	WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
			error = 1;
			if(timeSpan)
			{
				timeSpan->count = 0;
				WAL_FREE(timeSpan->spans);
			}
			break;
		}

		WalPrint("After parameterNamesLocal[%d] : %s\n",cnt,parameterNamesLocal[cnt]);
	}
	
	if(error != 1)
	{
		WalPrint("CompName = %s, dbusPath : %s, paramCount = %d\n", CompName, dbusPath, paramCount);
		if(timeSpan)
		{
			startTime = getCurrentTimeInMicroSeconds(&start);
			WalPrint("component start_time : %llu\n",startTime);
		}
		ret = CcspBaseIf_getParameterAttributes(bus_handle,CompName,dbusPath,parameterNamesLocal,paramCount, &sizeAttrArr, &ppAttrArray);
		if(timeSpan)
		{
			endTime = getCurrentTimeInMicroSeconds(&end);	
			timeDuration = endTime - startTime;
			
			timeSpan->spans[timeSpan->count].name = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN/2);
			strcpy(timeSpan->spans[timeSpan->count].name,CompName);
			WalPrint("timeSpan->spans[%d].name : %s\n",timeSpan->count,timeSpan->spans[timeSpan->count].name);
			WalPrint("start_time : %llu\n",startTime);
			timeSpan->spans[timeSpan->count].start = startTime;
			WalPrint("timeSpan->spans[%d].start : %llu\n",timeSpan->count,timeSpan->spans[timeSpan->count].start);
			WalPrint("timeDuration : %lu\n",timeDuration);
			timeSpan->spans[timeSpan->count].duration = timeDuration;
			WalPrint("timeSpan->spans[%d].duration : %lu\n",timeSpan->count,timeSpan->spans[timeSpan->count].duration);
		}
		WalPrint("----- After GPA ret = %d------\n",ret);
		if (ret != CCSP_SUCCESS)
		{
			WalError("Error:Failed to GetValue for parameters ret: %d\n", ret);
		}
		else
		{
			WalPrint("sizeAttrArr : %d\n",sizeAttrArr);
			if(startIndex == 0)
			{
				attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * sizeAttrArr);
			}
			else
			{
				attr[0] = (AttrVal **) realloc(attr[0],sizeof(AttrVal*) * (startIndex + sizeAttrArr));
			}
			for (cnt = 0; cnt < sizeAttrArr; cnt++)
			{
				WalPrint("cnt+startIndex : %d\n",cnt+startIndex);				
				attr[0][cnt+startIndex] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
				attr[0][cnt+startIndex]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
				attr[0][cnt+startIndex]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);

				IndexMpa_CPEtoWEBPA(&ppAttrArray[cnt]->parameterName);
				WalPrint("ppAttrArray[cnt]->parameterName : %s\n",ppAttrArray[cnt]->parameterName);
				walStrncpy(attr[0][cnt+startIndex]->name, ppAttrArray[cnt]->parameterName,MAX_PARAMETERNAME_LEN);
				sprintf(attr[0][cnt+startIndex]->value, "%d", ppAttrArray[cnt]->notification);
				attr[0][cnt+startIndex]->type = WAL_INT;
				WalPrint("success: %s %s %d \n",attr[0][cnt+startIndex]->name,attr[0][cnt+startIndex]->value,attr[0][cnt+startIndex]->type);
			}

			free_parameterAttributeStruct_t(bus_handle, sizeAttrArr, ppAttrArray);
		}	
	}
		
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		WAL_FREE(parameterNamesLocal[cnt]);
	}
	WAL_FREE(parameterNamesLocal);
	return ret;
}

static int setAtomicParamValues(ParamVal *paramVal, char *CompName, char *dbusPath, int paramCount,const WEBPA_SET_TYPE setType,char * transaction_id, uint32_t *duration)
{
        char* faultParam = NULL;
        int ret=0, size = 0, cnt = 0, cnt1=0, retIndex=0, index = -1;
        char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
        char objectName[MAX_PARAMETERNAME_LEN] = { 0 };
        unsigned int writeID = CCSP_COMPONENT_ID_WebPA;
        uint64_t startTime = 0, endTime = 0;
        struct timespec start, end;
        uint32_t timeDuration = 0;

        WalPrint("------------------ start of setAtomicParamValues ----------------\n");
        parameterValStruct_t* val = (parameterValStruct_t*) malloc(sizeof(parameterValStruct_t) * paramCount);
        memset(val,0,(sizeof(parameterValStruct_t) * paramCount));

        for (cnt = 0; cnt < paramCount; cnt++)
        {
                retIndex=0;
                walStrncpy(paramName, paramVal[cnt].name,sizeof(paramName));
                WalPrint("Inside setAtomicParamValues paramName is %s \n",paramName);
                retIndex = IndexMpa_WEBPAtoCPE(paramName);
                if(retIndex == -1)
                {
                        ret = CCSP_ERR_INVALID_PARAMETER_NAME;
                        WalError("Parameter name %s is not supported.ret : %d\n", paramName, ret);
                        free_set_param_values_memory(val,paramCount,faultParam);
                        return ret;
                }
                WalPrint("B4 prepare_parameterValueStruct\n");
                ret = prepare_parameterValueStruct(&val[cnt], &paramVal[cnt], paramName);
                if(ret)
                {
                        WalError("Preparing parameter value struct is Failed \n");
                        free_set_param_values_memory(val,paramCount,faultParam);
                        return ret;
                }
        }

        writeID = (setType == WEBPA_ATOMIC_SET_XPC)? CCSP_COMPONENT_ID_XPC: CCSP_COMPONENT_ID_WebPA;

        if(!strcmp(CompName,RDKB_WIFI_FULL_COMPONENT_NAME))
        {
                identifyRadioIndexToReset(paramCount,val,&bRestartRadio1,&bRestartRadio2);
                bRadioRestartEn = TRUE;
        }

        startTime = getCurrentTimeInMicroSeconds(&start);

        ret = CcspBaseIf_setParameterValues(bus_handle, CompName, dbusPath, 0, writeID, val, paramCount, TRUE, &faultParam);

        endTime = getCurrentTimeInMicroSeconds(&end);	
        timeDuration = endTime - startTime;
        WalPrint("timeDuration : %lu\n",timeDuration);
        *duration = timeDuration;
        WalPrint("*duration : %lu\n",*duration);

        if(!strcmp(CompName,RDKB_WIFI_FULL_COMPONENT_NAME))
        {
                if(ret == CCSP_SUCCESS) //signal apply settings thread only when set is success
                {
                        if(transaction_id != NULL)
                        {
                                WalPrint("Copy transaction_id to current_transaction_id \n");
                                walStrncpy(current_transaction_id, transaction_id,sizeof(current_transaction_id));
                                WalPrint("In wal, current_transaction_id %s, transaction_id %s\n",current_transaction_id,transaction_id);    
                        }
                        else
                        {
                                WalError("transaction_id in request is NULL\n");
                                memset(current_transaction_id,0,sizeof(current_transaction_id));
                        }

                        pthread_cond_signal(&applySetting_cond);
                        WalPrint("condition signalling in setAtomicParamValues\n");
                }
        }
        
        if (ret != CCSP_SUCCESS && faultParam)
        {
                WalError("Failed to SetAtomicValue for param  '%s' ret : %d \n", faultParam, ret);
                free_set_param_values_memory(val,paramCount,faultParam);
                return ret;
        }
        
        free_set_param_values_memory(val,paramCount,faultParam);
        WalPrint("------------------ End of setAtomicParamValues ----------------\n");
        return ret;
}
/**
 * @brief setParamValues Returns the status from stack for SET request
 *
 * @param[in] paramVal parameter value Array
 * @param[in] paramCount Number of parameters
 * @param[in] setType set for atomic set
 * @param[out] timeSpan timing_values for each component.
 */

static int setParamValues(ParamVal paramVal[], int paramCount, const WEBPA_SET_TYPE setType, money_trace_spans *timeSpan,char * transaction_id)
{
	char* faultParam = NULL;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret=0, size = 0, cnt = 0, cnt1=0, retIndex=0, index = -1;
	componentStruct_t ** ppComponents = NULL;
	char CompName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char tempCompName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dbusPath[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char objectName[MAX_PARAMETERNAME_LEN] = { 0 };
	unsigned int writeID = CCSP_COMPONENT_ID_WebPA;
	uint64_t startTime = 0, endTime = 0;
	struct timespec start, end;
	uint64_t start_time = 0;
	uint32_t timeDuration = 0;
	
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	parameterValStruct_t* val = (parameterValStruct_t*) malloc(sizeof(parameterValStruct_t) * paramCount);
	memset(val,0,(sizeof(parameterValStruct_t) * paramCount));
	
	walStrncpy(paramName, paramVal[0].name,sizeof(paramName));
	retIndex = IndexMpa_WEBPAtoCPE(paramName);
	if(retIndex== -1)
	{
		ret = CCSP_ERR_INVALID_PARAMETER_NAME;
		WalError("Parameter name %s is not supported.ret : %d\n", paramName, ret);
		WAL_FREE(val);
		return ret;
	}
	
	index = getComponentInfoFromCache(paramName,objectName,CompName,dbusPath);
	
	// Cannot identify the component from cache, make DBUS call to fetch component
	if(index == -1 || ComponentValArray[index].comp_size > 2) //anything above size > 2
	{
		WalPrint("in if for size >2\n");
		// GET Component for parameter from stack
		if(index != -1)
		        WalPrint("ComponentValArray[index].comp_size : %d\n",ComponentValArray[index].comp_size);
		walStrncpy(paramName,paramVal[0].name,sizeof(paramName));
		retIndex = IndexMpa_WEBPAtoCPE(paramName);
		if(retIndex == -1)
		{
			ret = CCSP_ERR_INVALID_PARAMETER_NAME;
			WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
			WAL_FREE(val);
			return ret;
		}
		WalPrint("Get component for paramName : %s from stack\n",paramName);

		ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
		WalPrint("size : %d, ret : %d\n",size,ret);
		
		if (ret == CCSP_SUCCESS && size == 1)
		{	
			walStrncpy(CompName,ppComponents[0]->componentName,sizeof(CompName));
			walStrncpy(dbusPath,ppComponents[0]->dbusPath,sizeof(dbusPath));
			free_componentStruct_t(bus_handle, size, ppComponents);
		}
		else
		{
			WalError("Parameter name %s is not supported. ret = %d\n", paramName, ret);
			free_componentStruct_t(bus_handle, size, ppComponents);
			WAL_FREE(val);
			//to avoid wildcard SET 
			if(size > 1)
				return CCSP_FAILURE;
			return ret;
		}
	}
	
	WalPrint("parameterName: %s, CompName : %s, dbusPath : %s\n", paramName, CompName, dbusPath);

	for (cnt = 0; cnt < paramCount; cnt++) 
	{
	    	retIndex=0;
		walStrncpy(paramName, paramVal[cnt].name,sizeof(paramName));
		retIndex = IndexMpa_WEBPAtoCPE(paramName);
		if(retIndex == -1)
		{
			ret = CCSP_ERR_INVALID_PARAMETER_NAME;
			WalError("Parameter name %s is not supported.ret : %d\n", paramName, ret);
			free_set_param_values_memory(val,paramCount,faultParam);
			return ret;
		}
		
		WalPrint("-------Starting parameter component comparison-----------\n");
		index = getComponentInfoFromCache(paramName,objectName,tempCompName,dbusPath);
				
		// Cannot identify the component from cache, make DBUS call to fetch component
		if(index == -1 || ComponentValArray[index].comp_size > 2) //anything above size > 2
		{
			WalPrint("in if for size >2\n");
			// GET Component for parameter from stack
			WalPrint("ComponentValArray[index].comp_size : %d\n",ComponentValArray[index].comp_size);
			walStrncpy(paramName,paramVal[cnt].name,sizeof(paramName));
			retIndex = IndexMpa_WEBPAtoCPE(paramName);
			if(retIndex == -1)
			{
				ret = CCSP_ERR_INVALID_PARAMETER_NAME;
				WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
				WAL_FREE(val);
				return ret;
			}
			WalPrint("Get component for paramName : %s from stack\n",paramName);

			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
		        dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
			WalPrint("size : %d, ret : %d\n",size,ret);
			
			if (ret == CCSP_SUCCESS && size == 1)
			{	
				walStrncpy(tempCompName,ppComponents[0]->componentName,sizeof(tempCompName));
				free_componentStruct_t(bus_handle, size, ppComponents);
			}
			else
			{
				WalError("Parameter name %s is not supported. ret = %d\n", paramName, ret);
				free_componentStruct_t(bus_handle, size, ppComponents);
				WAL_FREE(val);
				//to avoid wildcard SET 
				if(size > 1)
					return CCSP_FAILURE;
				return ret;
			}
		}
		
		WalPrint("parameterName: %s, tempCompName : %s\n", paramName, tempCompName);
		if (strcmp(CompName, tempCompName) != 0)
		{
			WalError("Error: Parameters does not belong to the same component\n");
			WAL_FREE(val);
			return CCSP_FAILURE;
		}
		WalPrint("--------- End of parameter component comparison------\n");
				
		ret = prepare_parameterValueStruct(&val[cnt], &paramVal[cnt], paramName);
		if(ret)
		{
			WalError("Preparing parameter value struct is Failed \n");
			free_set_param_values_memory(val,paramCount,faultParam);
			return ret;
		}
	}
		
	writeID = (setType == WEBPA_ATOMIC_SET_XPC)? CCSP_COMPONENT_ID_XPC: CCSP_COMPONENT_ID_WebPA;
	
	if(!strcmp(CompName,RDKB_WIFI_FULL_COMPONENT_NAME)) 
	{		
		if(applySettingsFlag == TRUE)
		{
			free_set_param_values_memory(val,paramCount,faultParam);
			return CCSP_ERR_WIFI_BUSY;
		}
		identifyRadioIndexToReset(paramCount,val,&bRestartRadio1,&bRestartRadio2);
		bRadioRestartEn = TRUE;		
	}
	if(timeSpan)
	{
		timeSpan->spans = (money_trace_span *) malloc(sizeof(money_trace_span));
		memset(timeSpan->spans,0,(sizeof(money_trace_span)));
		timeSpan->count = 1;
		WalPrint("timeSpan->count : %d\n",timeSpan->count);
		startTime = getCurrentTimeInMicroSeconds(&start);	
		WalPrint("component start_time : %llu\n",startTime);
	}
		
	ret = CcspBaseIf_setParameterValues(bus_handle, CompName, dbusPath, 0, writeID, val, paramCount, TRUE, &faultParam);
	if(timeSpan)
	{
		endTime = getCurrentTimeInMicroSeconds(&end);	
		timeDuration += endTime - startTime;
		timeSpan->spans[0].name = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN/2);
		strcpy(timeSpan->spans[0].name,CompName);
		WalPrint("timeSpan->spans[0].name : %s\n",timeSpan->spans[0].name);
		WalPrint("start_time : %llu\n",startTime);
		timeSpan->spans[0].start = startTime;
		WalPrint("timeSpan->spans[0].start : %llu\n",timeSpan->spans[0].start);
		WalPrint("timeDuration : %lu\n",timeDuration);
		timeSpan->spans[0].duration = timeDuration;
		WalPrint("timeSpan->spans[0].duration : %lu\n",timeSpan->spans[0].duration);
	}
	if(!strcmp(CompName,RDKB_WIFI_FULL_COMPONENT_NAME)) 
	{
		if(ret == CCSP_SUCCESS) //signal apply settings thread only when set is success
		{
			if(transaction_id != NULL)
			{
				WalPrint("Copy transaction_id to current_transaction_id \n");
				walStrncpy(current_transaction_id, transaction_id,sizeof(current_transaction_id));
				WalPrint("In wal, current_transaction_id %s, transaction_id %s\n",current_transaction_id,transaction_id);    
			}
			else
			{
				WalError("transaction_id in request is NULL\n");
				memset(current_transaction_id,0,sizeof(current_transaction_id));
			}
		    
			pthread_cond_signal(&applySetting_cond);
			WalPrint("condition signalling in setParamValues\n");
		}
		
	}	
	if (ret != CCSP_SUCCESS && faultParam) 
	{
		WalError("Failed to SetAtomicValue for param  '%s' ret : %d \n", faultParam, ret);
		free_set_param_values_memory(val,paramCount,faultParam);
		return ret;
	}
	free_set_param_values_memory(val,paramCount,faultParam);	
	return ret;
}

static void initApplyWiFiSettings()
{
	int err = 0;
	pthread_t applySettingsThreadId;
	WalPrint("============ initApplySettings ==============\n");
	err = pthread_create(&applySettingsThreadId, NULL, applyWiFiSettingsTask, NULL);
	if (err != 0) 
	{
		WalError("Error creating messages thread :[%s]\n", strerror(err));
	}
	else
	{
		WalPrint("applyWiFiSettings thread created Successfully\n");
	}
}

static void *applyWiFiSettingsTask()
{
	char CompName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dbusPath[MAX_PARAMETERNAME_LEN/2] = { 0 };
	parameterValStruct_t *RadApplyParam = NULL;
	char* faultParam = NULL;
	unsigned int writeID = CCSP_COMPONENT_ID_WebPA;
	struct timespec start,end,*startPtr,*endPtr;
	startPtr = &start;
	endPtr = &end;
	int nreq = 0,ret=0;
	WalPrint("================= applyWiFiSettings ==========\n");
	parameterValStruct_t val_set[4] = { 
					{"Device.WiFi.Radio.1.X_CISCO_COM_ApplySettingSSID","1", ccsp_int},
					{"Device.WiFi.Radio.1.X_CISCO_COM_ApplySetting", "true", ccsp_boolean},
					{"Device.WiFi.Radio.2.X_CISCO_COM_ApplySettingSSID","2", ccsp_int},
					{"Device.WiFi.Radio.2.X_CISCO_COM_ApplySetting", "true", ccsp_boolean} };
	
	// Component cache index 0 maps to "Device.WiFi."
	if(ComponentValArray[0].comp_name != NULL && ComponentValArray[0].dbus_path != NULL)
	{				
		walStrncpy(CompName,ComponentValArray[0].comp_name,sizeof(CompName));
		walStrncpy(dbusPath,ComponentValArray[0].dbus_path,sizeof(dbusPath));
		WalPrint("CompName : %s dbusPath : %s\n",CompName,dbusPath);
	

		//Identify the radio and apply settings
		while(1)
		{
			WalPrint("Before cond wait in applyWiFiSettings\n");
			pthread_cond_wait(&applySetting_cond, &applySetting_mutex);
			applySettingsFlag = TRUE;
			WalPrint("applySettingsFlag is set to TRUE\n");
			getCurrentTime(startPtr);
			WalPrint("After cond wait in applyWiFiSettings\n");
			if(bRadioRestartEn)
			{
				bRadioRestartEn = FALSE;
			
				if((bRestartRadio1 == TRUE) && (bRestartRadio2 == TRUE)) 
				{
					WalPrint("Need to restart both the Radios\n");
					RadApplyParam = val_set;
					nreq = 4;
				}

				else if(bRestartRadio1) 
				{
					WalPrint("Need to restart Radio 1\n");
					RadApplyParam = val_set;
					nreq = 2;
				}
				else if(bRestartRadio2) 
				{
					WalPrint("Need to restart Radio 2\n");
					RadApplyParam = &val_set[2];
					nreq = 2;
				}
			
				// Reset radio flags
				bRestartRadio1 = FALSE;
				bRestartRadio2 = FALSE;
			
				WalPrint("nreq : %d writeID : %d\n",nreq,writeID);
				ret = CcspBaseIf_setParameterValues(bus_handle, CompName, dbusPath, 0, writeID, RadApplyParam, nreq, TRUE,&faultParam);
				WalInfo("After SPV in applyWiFiSettings ret = %d\n",ret);
				if (ret != CCSP_SUCCESS && faultParam) 
				{
					WalError("Failed to Set Apply Settings\n");
					WAL_FREE(faultParam);
				}	
				
				// Send transcation event notify to server
				send_transaction_Notify();
				
				applySettingsFlag = FALSE;
				WalPrint("applySettingsFlag is set to FALSE\n");
			}

			getCurrentTime(endPtr);
			WalInfo("Elapsed time for apply setting : %ld ms\n", timeValDiff(startPtr, endPtr));
		}
	}
	else
	{
		WalError("Failed to get WiFi component info from cache to initialize apply settings\n");
	}
	WalPrint("============ End =============\n");
}

static void send_transaction_Notify()
{
	// Send notify event to server
	WalPrint("Send trans status event to server \n");
	if (NULL == notifyCbFn) 
	{
		WalError("Fatal: notifyCbFn is NULL\n");
		return;
	}
	else if(strlen(current_transaction_id) == 0)
	{
		WalError("current_transaction_id is empty, hence not sending transaction status event\n");
	}	
	else
	{
		WalPrint("Allocate memory to NotifyData \n");
		TransData *msgPtr = (TransData *) malloc(sizeof(TransData) * 1);
		msgPtr->transId = (char *)malloc(sizeof(char) * sizeof(current_transaction_id));
		walStrncpy(msgPtr->transId, current_transaction_id,sizeof(current_transaction_id));
		WalPrint("current_transaction_id %s, transId %s\n", current_transaction_id, msgPtr->transId);

		NotifyData *notifyDataPtr = (NotifyData *) malloc(sizeof(NotifyData) * 1);
		notifyDataPtr->type = TRANS_STATUS;

		Notify_Data *notify_data = (Notify_Data *) malloc(sizeof(Notify_Data) * 1);
		notify_data->status = msgPtr;

		notifyDataPtr->data = notify_data;

		(*notifyCbFn)(notifyDataPtr);
	}
}

/**
 * @brief free_set_param_values_memory to free memory allocated in setParamValues function
 *
 * @param[in] val parameter value Array
 * @param[in] paramCount parameter count
 * @param[in] faultParam fault Param
 */
static void free_set_param_values_memory(parameterValStruct_t* val, int paramCount, char * faultParam)
{
	WalPrint("Inside free_set_param_values_memory\n");	
	int cnt1 = 0;
	WAL_FREE(faultParam);

	for (cnt1 = 0; cnt1 < paramCount; cnt1++) 
	{
		WAL_FREE(val[cnt1].parameterName);
	}
	WAL_FREE(val);
}

static void free_paramVal_memory(ParamVal ** val, int paramCount)
{
	WalPrint("Inside free_paramVal_memory\n");
	int cnt1 = 0;
	for (cnt1 = 0; cnt1 < paramCount; cnt1++)
	{
		WAL_FREE(val[cnt1]);
	}
	WAL_FREE(val);
}

/**
 * @brief prepare_parameterValueStruct returns parameter values
 *
 * @param[in] val parameter value Array
 * @param[in] paramVal parameter value Array
 * @param[in] paramName parameter name
 */
 
static int prepare_parameterValueStruct(parameterValStruct_t* val, ParamVal *paramVal, char *paramName)
{
	val->parameterName = malloc( sizeof(char) * MAX_PARAMETERNAME_LEN);

	if(val->parameterName == NULL)
	{
		return WAL_FAILURE;
	}
	strcpy(val->parameterName,paramName);

	val->parameterValue = paramVal->value;
		
	switch(paramVal->type)
	{ 
		case 0:
				val->type = ccsp_string;
				break;
		case 1:
				val->type = ccsp_int;
				break;
		case 2:
				val->type = ccsp_unsignedInt;
				break;
		case 3:
				val->type = ccsp_boolean;
				break;
		case 4:
				val->type = ccsp_dateTime;
				break;
		case 5:
				val->type = ccsp_base64;
				break;
		case 6:
				val->type = ccsp_long;
				break;
		case 7:
				val->type = ccsp_unsignedLong;
				break;
		case 8:
				val->type = ccsp_float;
				break;
		case 9:
				val->type = ccsp_double;
				break;
		case 10:
				val->type = ccsp_byte;
				break;
		default:
				val->type = ccsp_none;
				break;
	}
	return WAL_SUCCESS;
}

/**
 * @brief identifyRadioIndexToReset identifies which radio to restart 
 *
 * @param[in] paramCount count of parameters
 * @param[in] paramVal parameter value Array
 * @param[out] bRestartRadio1
 * @param[out] bRestartRadio2
 */
static void identifyRadioIndexToReset(int paramCount, parameterValStruct_t* val,BOOL *bRestartRadio1,BOOL *bRestartRadio2) 
{
	int x =0 ,index =0, SSID =0,apply_rf =0;
	for (x = 0; x < paramCount; x++)
	{
		WalPrint("val[%d].parameterName : %s\n",x,val[x].parameterName);
		if (!strncmp(val[x].parameterName, "Device.WiFi.Radio.1.", 20))
		{
			*bRestartRadio1 = TRUE;
		}
		else if (!strncmp(val[x].parameterName, "Device.WiFi.Radio.2.", 20))
		{
			*bRestartRadio2 = TRUE;
		}
		else
		{
			if ((!strncmp(val[x].parameterName, "Device.WiFi.SSID.", 17)))
			{
				sscanf(val[x].parameterName, "Device.WiFi.SSID.%d", &index);
				WalPrint("SSID index = %d\n", index);
				SSID = (1 << ((index) - 1));
				apply_rf = (2 - ((index) % 2));
				WalPrint("apply_rf = %d\n", apply_rf);

				if (apply_rf == 1)
				{
					*bRestartRadio1 = TRUE;
				}
				else if (apply_rf == 2)
				{
					*bRestartRadio2 = TRUE;
				}
			}
			else if (!strncmp(val[x].parameterName, "Device.WiFi.AccessPoint.",24))
			{
				sscanf(val[x].parameterName, "Device.WiFi.AccessPoint.%d", &index);
				WalPrint("AccessPoint index = %d\n", index);
				SSID = (1 << ((index) - 1));
				apply_rf = (2 - ((index) % 2));
				WalPrint("apply_rf = %d\n", apply_rf);

				if (apply_rf == 1)
				{
					*bRestartRadio1 = TRUE;
				}
				else if (apply_rf == 2)
				{
					*bRestartRadio2 = TRUE;
				}
			}
		}
	}
}

/**
 * @brief setParamAttributes Sets the parameter attribute value
 * @param[in] pParameterName parameter name
 * @param[in] attArr attribute value Array
 */
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr)
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret = 0, size = 0, notificationType = 0;

	componentStruct_t ** ppComponents = NULL;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	parameterAttributeStruct_t attriStruct;

	attriStruct.parameterName = NULL;
	attriStruct.notificationChanged = 1;
	attriStruct.accessControlChanged = 0;

	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	walStrncpy(paramName, pParameterName,sizeof(paramName));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, /* prefix */
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1)
	{
#ifndef USE_NOTIFY_COMPONENT
		ret = CcspBaseIf_Register_Event(bus_handle, ppComponents[0]->componentName, "parameterValueChangeSignal");

		if (CCSP_SUCCESS != ret)
		{
			WalError("WebPa: CcspBaseIf_Register_Event failed!!!\n");
		}

		CcspBaseIf_SetCallback2(bus_handle, "parameterValueChangeSignal", ccspWebPaValueChangedCB, NULL);
#endif

		attriStruct.parameterName = paramName;
		notificationType = atoi(attArr->value);
		attriStruct.notification = notificationType;

		ret = CcspBaseIf_setParameterAttributes(bus_handle,	ppComponents[0]->componentName, ppComponents[0]->dbusPath, 0,
				&attriStruct, 1);

		if (CCSP_SUCCESS != ret)
		{
			WalError("Failed to SetValue for SetParamAttr ret : %d \n", ret);
		}

		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		WalError("Component name is not supported ret : %d\n", ret);
	}

	return ret;
}

static int setAtomicParamAttributes(const char *pParameterName[], const AttrVal **attArr,int paramCount, money_trace_spans *timeSpan)
{
	int ret = 0, cnt = 0, size = 0, notificationType = 0, error = 0,retIndex = 0, count = 0, i = 0, count1;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char **compName = NULL;
	char **tempCompName = NULL;
	char **dbusPath = NULL;
	char **tempDbusPath = NULL;
	uint64_t startTime = 0, endTime = 0;
	struct timespec start, end;
	uint64_t start_time = 0;
	uint32_t timeDuration = 0;
	
	parameterAttributeStruct_t *attriStruct =(parameterAttributeStruct_t*) malloc(sizeof(parameterAttributeStruct_t) * paramCount);
	memset(attriStruct,0,(sizeof(parameterAttributeStruct_t) * paramCount));
	WalPrint("==========setAtomicParamAttributes ========\n ");
	
	walStrncpy(paramName,pParameterName[0],sizeof(paramName));
	// To get list of component name and dbuspath
	ret = getComponentDetails(paramName,&compName,&dbusPath,&error,&count);
	if(error == 1)
	{
		WalError("Component name is not supported ret : %d\n", ret);
		WAL_FREE(attriStruct);
		return ret;
	}
	WalPrint("paramName: %s count: %d\n",paramName,count);
	for(i = 0; i < count; i++)
	{
		WalPrint("compName[%d] : %s, dbusPath[%d] : %s\n", i,compName[i],i, dbusPath[i]);
	}
	
	for (cnt = 0; cnt < paramCount; cnt++) 
	{
		retIndex = 0;
		walStrncpy(paramName,pParameterName[cnt],sizeof(paramName));
		// To get list of component name and dbuspath	
		ret = getComponentDetails(paramName,&tempCompName,&tempDbusPath,&error,&count1);
		if(error == 1)
		{
			WalError("Component name is not supported ret : %d\n", ret);
			WAL_FREE(attriStruct);
			return ret;
		}			
		WalPrint("paramName: %s count: %d\n",paramName,count);
		for(i = 0; i < count1; i++)
		{
			WalPrint("tempCompName[%d] : %s, tempDbusPath[%d] : %s\n", i,tempCompName[i],i, tempDbusPath[i]);
		}
		if (strcmp(compName[0], tempCompName[0]) != 0)
		{
			WalError("Error: Parameters does not belong to the same component\n");
			WAL_FREE(attriStruct);
			return CCSP_FAILURE;
		}		
		retIndex = IndexMpa_WEBPAtoCPE(paramName);
		if(retIndex == -1)
		{
			ret = CCSP_ERR_INVALID_PARAMETER_NAME;
			WalError("Parameter name %s is not supported.ret : %d\n", paramName, ret);
			WAL_FREE(attriStruct);	
			return ret;
		}

		attriStruct[cnt].parameterName = NULL;
		attriStruct[cnt].notificationChanged = 1;
		attriStruct[cnt].accessControlChanged = 0;	
		notificationType = atoi(attArr[cnt]->value);
		WalPrint("notificationType : %d\n",notificationType);
		if(notificationType == 1)
		{
#ifndef USE_NOTIFY_COMPONENT
			if(timeSpan)
			{
				startTime = getCurrentTimeInMicroSeconds(&start);
			}
			ret = CcspBaseIf_Register_Event(bus_handle, compName[0], "parameterValueChangeSignal");
			if(timeSpan)
			{
				endTime = getCurrentTimeInMicroSeconds(&end);	
				timeDuration += endTime - startTime;
			}
			if (CCSP_SUCCESS != ret)
			{
				WalError("WebPa: CcspBaseIf_Register_Event failed!!!\n");
			}
			if(timeSpan)
			{
				startTime = getCurrentTimeInMicroSeconds(&start);
			}		
			CcspBaseIf_SetCallback2(bus_handle, "parameterValueChangeSignal", ccspWebPaValueChangedCB, NULL);
			if(timeSpan)
			{
				endTime = getCurrentTimeInMicroSeconds(&end);	
				timeDuration += endTime - startTime;
			}
#endif
		}
		attriStruct[cnt].parameterName = malloc( sizeof(char) * MAX_PARAMETERNAME_LEN);
		walStrncpy(attriStruct[cnt].parameterName,paramName,MAX_PARAMETERNAME_LEN);
		WalPrint("attriStruct[%d].parameterName : %s\n",cnt,attriStruct[cnt].parameterName);
		
		attriStruct[cnt].notification = notificationType;
		WalPrint("attriStruct[%d].notification : %d\n",cnt,attriStruct[cnt].notification );	
		free_componentDetails(tempCompName,tempDbusPath,count1);
	}
	
	if(error != 1)
	{
		if(timeSpan)
		{
			timeSpan->spans = (money_trace_span *) malloc(sizeof(money_trace_span));
			memset(timeSpan->spans,0,(sizeof(money_trace_span)));
			timeSpan->count = 1;
			WalPrint("timeSpan->count : %d\n",timeSpan->count);
			startTime = getCurrentTimeInMicroSeconds(&start);
			start_time = startTime;
			WalPrint("component start_time: %llu\n",start_time);
		}
		ret = CcspBaseIf_setParameterAttributes(bus_handle,compName[0], dbusPath[0], 0,
			attriStruct, paramCount);
		if(timeSpan)
		{
			endTime = getCurrentTimeInMicroSeconds(&end);	
			timeDuration += endTime - startTime;
			
			timeSpan->spans[0].name = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN/2);
			strcpy(timeSpan->spans[0].name,compName[0]);
			WalPrint("timeSpan->spans[0].name : %s\n",timeSpan->spans[0].name);
			WalPrint("start_time : %llu\n",start_time);
			timeSpan->spans[0].start = start_time;
			WalPrint("timeSpan->spans[0].start : %llu\n",timeSpan->spans[0].start);
			WalPrint("timeDuration : %lu\n",timeDuration);
			timeSpan->spans[0].duration = timeDuration;
			WalPrint("timeSpan->spans[0].duration : %lu\n",timeSpan->spans[0].duration);
		}
		WalPrint("=== After SPA == ret = %d\n",ret);
		if (CCSP_SUCCESS != ret)
		{
			WalError("Failed to set attributes for SetParamAttr ret : %d \n", ret);
		}
		for (cnt = 0; cnt < paramCount; cnt++) 
		{
			WAL_FREE(attriStruct[cnt].parameterName);
		}
	}
	free_componentDetails(compName,dbusPath,count);
	WAL_FREE(attriStruct);
	return ret;
}
/**
 * @brief IndexMpa_WEBPAtoCPE maps to CPE index
 * @param[in] pParameterName parameter name
 */
 
static int IndexMpa_WEBPAtoCPE(char *pParameterName)
{
	int i = 0, j = 0, dmlNameLen = 0, instNum = 0, len = 0, matchFlag = -1;
	char pDmIntString[WIFI_MAX_STRING_LEN];
	char* instNumStart = NULL;
	char restDmlString[WIFI_MAX_STRING_LEN] = {'\0'};
	for (i = 0; i < WIFI_PARAM_MAP_SIZE; i++)
	{
		dmlNameLen = strlen(CcspDmlName[i]);
		if (strncmp(pParameterName, CcspDmlName[i], dmlNameLen) == 0)
		{
			instNumStart = pParameterName + dmlNameLen;
			//To match complete wildcard, including . 
			if (strlen(pParameterName) <= dmlNameLen + 1)
			{
				// Found match on table, but there is no instance number
				break;
			}
			else
			{
				if (instNumStart[0] == '.')
				{
					instNumStart++;
				}
				else
				{ 
				  WalPrint("No matching index as instNumStart[0] : %c\n",instNumStart[0]);
				  break;
				}
				sscanf(instNumStart, "%d%s", &instNum, restDmlString);
				WalPrint("instNum : %d restDmlString : %s\n",instNum,restDmlString);

				// Find instance match and translate
				if (i == 0)
				{
					// For Device.WiFI.Radio.
					j = 0;
					len=2;
				}
				else
				{
					// For other than Device.WiFI.Radio.
					j = 2;
					len =WIFI_INDEX_MAP_SIZE;
				}
				for (j; j < len; j++)
				{
					if (IndexMap[j].WebPaInstanceNumber == instNum)
					{
						snprintf(pDmIntString, sizeof(pDmIntString),"%s.%d%s", CcspDmlName[i], IndexMap[j].CcspInstanceNumber, restDmlString);
						strcpy(pParameterName, pDmIntString);
						matchFlag = 1;
						break;
					}
				}
				WalPrint("matchFlag %d\n",matchFlag);
				if(matchFlag == -1)
				{
					WalError("Invalid index for : %s\n",pParameterName);
					return matchFlag;
				}
			}
			break;
		}
	}
	return 0;
}

/**
 * @brief IndexMpa_CPEtoWEBPA maps to WEBPA index
 * @param[in] pParameterName parameter name
 */
 
static void IndexMpa_CPEtoWEBPA(char **ppParameterName)
{
	int i = 0, j = 0, dmlNameLen = 0, instNum =0;
	char *pDmIntString = NULL;
	char* instNumStart = NULL;
	char restDmlString[WIFI_MAX_STRING_LEN]= {'\0'};
	char *pParameterName = *ppParameterName;

	for (i = 0; i < WIFI_PARAM_MAP_SIZE; i++) 
	{
		dmlNameLen = strlen(CcspDmlName[i]);
		if (strncmp(pParameterName, CcspDmlName[i], dmlNameLen) == 0)
		{
			instNumStart = pParameterName + dmlNameLen;
			if (strlen(pParameterName) < dmlNameLen + 1)
			{
				// Found match on table, but there is no instance number
				break;
			}
			else
			{
				if (instNumStart[0] == '.')
				{
					instNumStart++;
				}
				sscanf(instNumStart, "%d%s", &instNum, restDmlString);
				// Find instance match and translate
				if (i == 0)
				{
					// For Device.WiFI.Radio.
					j = 0;
				} else {
					// For other than Device.WiFI.Radio.
					j = 2;
				}
				for (j; j < WIFI_INDEX_MAP_SIZE; j++)
				{
					if (IndexMap[j].CcspInstanceNumber == instNum)
					{
						pDmIntString = (char *) malloc(
								sizeof(char) * (dmlNameLen + MAX_PARAMETERNAME_LEN));
						if (pDmIntString)
						{
							snprintf(pDmIntString, dmlNameLen + MAX_PARAMETERNAME_LEN ,"%s.%d%s", CcspDmlName[i],
									IndexMap[j].WebPaInstanceNumber,
									restDmlString);
							WAL_FREE(pParameterName);
							WalPrint("pDmIntString : %s\n",pDmIntString);
							*ppParameterName = pDmIntString;
							return;
						}

						break;
					}
				}
			}
			break;
		}
	}
	return;
}

/**
 * @brief getMatchingComponentValArrayIndex Compare objectName with the pre-populated ComponentValArray and return matching index
 *
 * param[in] objectName 
 * @return matching ComponentValArray index
 */
static int getMatchingComponentValArrayIndex(char *objectName)
{
	int i =0,index = -1;

	for(i = 0; i < compCacheSuccessCnt ; i++)
	{
		if(ComponentValArray[i].obj_name != NULL && !strcmp(objectName,ComponentValArray[i].obj_name))
		{
	      		index = ComponentValArray[i].comp_id;
			WalPrint("Matching Component Val Array index for object %s : %d\n",objectName, index);
			break;
		}	    
	}
	return index;
}


/**
 * @brief getMatchingSubComponentValArrayIndex Compare objectName with the pre-populated SubComponentValArray and return matching index
 *
 * param[in] objectName 
 * @return matching ComponentValArray index
 */
static int getMatchingSubComponentValArrayIndex(char *objectName)
{
	int i =0,index = -1;
	
	for(i = 0; i < subCompCacheSuccessCnt ; i++)
	{
		if(SubComponentValArray[i].obj_name != NULL && !strcmp(objectName,SubComponentValArray[i].obj_name))
		{
		      	index = SubComponentValArray[i].comp_id;
			WalPrint("Matching Sub-Component Val Array index for object %s : %d\n",objectName, index);
			break;
		}	    
	}	
	return index;
}

/**
 * @brief getObjectName Get object name from parameter name. Example WiFi from "Device.WiFi.SSID."
 *
 * @param[in] str Parameter Name
 * param[out] objectName Set with the object name
 * param[in] objectLevel Level of object 1, 2. Example 1 for WiFi and 2 for SSID
 */
static void getObjectName(char *str, char *objectName, int objectLevel)
{
	char *tmpStr;
	char localStr[MAX_PARAMETERNAME_LEN]={'\0'};
	walStrncpy(localStr,str,sizeof(localStr));
	int count = 1;
	
	if(localStr)
	{	
		tmpStr = strtok(localStr,".");
		
		while (tmpStr != NULL)
		{
			tmpStr = strtok (NULL, ".");
			if(tmpStr && count >= objectLevel)
			{
				strcpy(objectName,tmpStr);
				WalPrint("_________ objectName %s__________ \n",objectName);
	    		        break;
			}
			count++;
	  	}
	}
}


/**
 * @brief LOGInit Initialize RDK Logger
 */
void LOGInit()
{
	#ifdef FEATURE_SUPPORT_RDKLOG
		rdk_logger_init("/fss/gw/lib/debug.ini");    /* RDK logger initialization*/
	#endif
}


/**
 * @brief _WEBPA_LOG WEBPA RDK Logger API
 *
 * @param[in] level LOG Level
 * @param[in] msg Message to be logged 
 */
void _WEBPA_LOG(unsigned int level, const char *msg, ...)
{
	va_list arg;
	char *pTempChar = NULL;
	int ret = 0;
	unsigned int rdkLogLevel = LOG_DEBUG;

	switch(level)
	{
		case WEBPA_LOG_ERROR:
			rdkLogLevel = LOG_ERROR;
			break;

		case WEBPA_LOG_INFO:
			rdkLogLevel = LOG_INFO;
			break;

		case WEBPA_LOG_PRINT:
			rdkLogLevel = LOG_DEBUG;
			break;
	}

	if( rdkLogLevel <= LOG_INFO )
	{
		pTempChar = (char *)malloc(4096);
		if(pTempChar)
		{
			va_start(arg, msg);
			ret = vsnprintf(pTempChar, 4096, msg,arg);
			if(ret < 0)
			{
				perror(pTempChar);
			}
			va_end(arg);
			RDK_LOG(rdkLogLevel, "LOG.RDK.WEBPA", "%s", pTempChar);
			WAL_FREE(pTempChar);
		}
	}
}

/**
 * @brief getWebPAConfig interface returns the WebPA config data.
 *
 * @param[in] param WebPA config param name.
 * @return const char* WebPA config param value.
 */
const char* getWebPAConfig(WCFG_PARAM_NAME param)
{
	const char *ret = NULL;
	
	switch(param)
	{
		case WCFG_COMPONENT_NAME:
			ret = RDKB_WEBPA_COMPONENT_NAME;
			break;

		case WCFG_CFG_FILE:
			ret = RDKB_WEBPA_CFG_FILE;
			break;

		case WCFG_CFG_FILE_SRC:
			ret = RDKB_WEBPA_CFG_FILE_SRC;
			break;

		case WCFG_DEVICE_INTERFACE:
			ret = RDKB_WEBPA_CFG_DEVICE_INTERFACE;
			break;

		case WCFG_DEVICE_MAC:
			ret = RDKB_WEBPA_DEVICE_MAC;
			break;

		case WCFG_DEVICE_REBOOT_PARAM:
			ret = RDKB_WEBPA_DEVICE_REBOOT_PARAM;
			break;

		case WCFG_DEVICE_REBOOT_VALUE:
			ret = RDKB_WEBPA_DEVICE_REBOOT_VALUE;
			break;

		case WCFG_XPC_SYNC_PARAM_CID:
			ret = RDKB_XPC_SYNC_PARAM_CID;
			break;

		case WCFG_XPC_SYNC_PARAM_CMC:
			ret = RDKB_XPC_SYNC_PARAM_CMC;
			break;

		case WCFG_FIRMWARE_VERSION:
			ret = RDKB_FIRMWARE_VERSION;
			break;
		
		case WCFG_DEVICE_UP_TIME:
			ret = RDKB_DEVICE_UP_TIME;
			break;

		case WCFG_MANUFACTURER:
			ret = RDKB_MANUFACTURER;
			break;

		case WCFG_MODEL_NAME:
			ret = RDKB_MODEL_NAME;
			break;
	
		case WCFG_XPC_SYNC_PARAM_SPV:
			ret = RDKB_XPC_SYNC_PARAM_SPV;
			break;

		case WCFG_PARAM_HOSTS_NAME:
			ret = RDKB_PARAM_HOSTS_NAME;
			break;

		case WCFG_PARAM_HOSTS_VERSION:
			ret = RDKB_PARAM_HOSTS_VERSION;
			break;
		
		case WCFG_PARAM_SYSTEM_TIME:
			ret = RDKB_PARAM_SYSTEM_TIME;
			break;
			
		case WCFG_RECONNECT_REASON:
			ret = RDKB_RECONNECT_REASON;
			break;
		
		case WCFG_REBOOT_REASON:
			ret = RDKB_REBOOT_REASON;
			break;
		
		default:
			ret = STR_NOT_DEFINED;
	}

	return ret;
}

/**
 * @brief WALInit Initialize WAL
 */
void WALInit()
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret = 0, i = 0, size = 0, len = 0, cnt = 0, cnt1 = 0, retryCount = 0;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	componentStruct_t ** ppComponents = NULL;

	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	WalPrint("-------- Start of populateComponentValArray -------\n");
	len = sizeof(objectList)/sizeof(objectList[0]);
	WalPrint("Length of object list : %d\n",len);
	for(i = 0; i < len ; i++)
	{
		walStrncpy(paramName,objectList[i],sizeof(paramName));
		do
		{
			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
					dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
			
			if (ret == CCSP_SUCCESS)
			{	    
				retryCount = 0;
				// Allocate memory for ComponentVal obj_name, comp_name, dbus_path
				ComponentValArray[cnt].obj_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				ComponentValArray[cnt].comp_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				ComponentValArray[cnt].dbus_path = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));

				ComponentValArray[cnt].comp_id = cnt;
				ComponentValArray[cnt].comp_size = size;
				getObjectName(paramName,ComponentValArray[cnt].obj_name,1);
				walStrncpy(ComponentValArray[cnt].comp_name,ppComponents[0]->componentName,MAX_PARAMETERNAME_LEN/2);
				walStrncpy(ComponentValArray[cnt].dbus_path,ppComponents[0]->dbusPath,MAX_PARAMETERNAME_LEN/2);
					   
				WalInfo("ComponentValArray[%d].comp_id = %d,ComponentValArray[cnt].comp_size = %d, ComponentValArray[%d].obj_name = %s, ComponentValArray[%d].comp_name = %s, ComponentValArray[%d].dbus_path = %s\n", cnt, ComponentValArray[cnt].comp_id,ComponentValArray[cnt].comp_size, cnt, ComponentValArray[cnt].obj_name, cnt, ComponentValArray[cnt].comp_name, cnt, ComponentValArray[cnt].dbus_path);  
				cnt++;
			}
			else
			{
				retryCount++;
				WalError("------------Failed to get component info for object %s----------: ret = %d, size = %d, retrying .... %d ...\n", objectList[i], ret, size, retryCount);
				if(retryCount == WAL_COMPONENT_INIT_RETRY_COUNT)
				{
					WalError("Unable to get component for object %s\n", objectList[i]);
				}
				else
				{
					sleep(WAL_COMPONENT_INIT_RETRY_INTERVAL);
				}
			}
			free_componentStruct_t(bus_handle, size, ppComponents);

		}while((retryCount >= 1) && (retryCount <= 3));		
	}
	
	compCacheSuccessCnt = cnt;
	WalPrint("compCacheSuccessCnt : %d\n", compCacheSuccessCnt);
	
	WalPrint("Initializing sub component list\n");
	len = sizeof(subObjectList)/sizeof(subObjectList[0]);
	WalPrint("Length of sub object list : %d\n",len);

	retryCount = 0;
	for(i = 0; i < len; i++)
	{
		walStrncpy(paramName,subObjectList[i],sizeof(paramName));
		do
		{
			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
					dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
			
			if (ret == CCSP_SUCCESS)
			{	       
				retryCount = 0;
				// Allocate memory for ComponentVal obj_name, comp_name, dbus_path
				SubComponentValArray[cnt1].obj_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				SubComponentValArray[cnt1].comp_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				SubComponentValArray[cnt1].dbus_path = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));

				SubComponentValArray[cnt1].comp_id = cnt1;
				SubComponentValArray[cnt1].comp_size = size;
				getObjectName(paramName,SubComponentValArray[cnt1].obj_name,2);
				WalPrint("in WALInit() SubComponentValArray[cnt].obj_name is %s",SubComponentValArray[cnt1].obj_name);
				walStrncpy(SubComponentValArray[cnt1].comp_name,ppComponents[0]->componentName,MAX_PARAMETERNAME_LEN/2);
				walStrncpy(SubComponentValArray[cnt1].dbus_path,ppComponents[0]->dbusPath,MAX_PARAMETERNAME_LEN/2);
					   
				WalInfo("SubComponentValArray[%d].comp_id = %d,SubComponentValArray[i].comp_size = %d, SubComponentValArray[%d].obj_name = %s, SubComponentValArray[%d].comp_name = %s, SubComponentValArray[%d].dbus_path = %s\n", cnt1, SubComponentValArray[cnt1].comp_id,SubComponentValArray[cnt1].comp_size, cnt1, SubComponentValArray[cnt1].obj_name, cnt1, SubComponentValArray[cnt1].comp_name, cnt1, SubComponentValArray[cnt1].dbus_path);  
				cnt1++;
			}
			else
			{
				retryCount++;
				WalError("------------Failed to get component info for object %s----------: ret = %d, size = %d, retrying.... %d ....\n", subObjectList[i], ret, size, retryCount);
				if(retryCount == WAL_COMPONENT_INIT_RETRY_COUNT)
				{
					WalError("Unable to get component for object %s\n", subObjectList[i]);
				}
				else
				{
					sleep(WAL_COMPONENT_INIT_RETRY_INTERVAL);
				}
			}
			free_componentStruct_t(bus_handle, size, ppComponents);

		}while((retryCount >= 1) && (retryCount <= (WAL_COMPONENT_INIT_RETRY_COUNT - 1)));
	}

	subCompCacheSuccessCnt = cnt1;
	WalPrint("subCompCacheSuccessCnt : %d\n", subCompCacheSuccessCnt);
	WalPrint("-------- End of populateComponentValArray -------\n");

	// Initialize Apply WiFi Settings handler
	initApplyWiFiSettings();
}

/**
 * @brief ccspSystemReadySignalCB Call back function to be executed once we receive system ready signal from CR.
 * This is to make sure that Web PA will SET attributes only when system is completely UP 
 */
static void ccspSystemReadySignalCB(void* user_data)
{
	// Touch a file to indicate that Web PA can proceed with 
	// SET/GET any parameter. 
	system("touch /var/tmp/webpaready");
	WalInfo("Received system ready signal, created /var/tmp/webpaready file\n");
}

/**
 * @brief waitUntilSystemReady Function to wait until the system ready signal from CR is received.
 * This is to delay WebPA start up until other components on stack are ready.
 */
static void waitUntilSystemReady()
{
	CcspBaseIf_Register_Event(bus_handle, NULL, "systemReadySignal");

        CcspBaseIf_SetCallback2
	(
		bus_handle,
		"systemReadySignal",
		ccspSystemReadySignalCB,
		NULL
	);

	FILE *file;
	int wait_time = 0;
          
	// Wait till Call back touches the indicator to proceed further
	while((file = fopen("/var/tmp/webpaready", "r")) == NULL)
	{
		WalInfo("Waiting for system ready signal\n");
		// After waiting for 24 * 5 = 120s (2mins) send dbus message to CR to query for system ready
		if(wait_time == 24)
		{
			wait_time = 0;
			if(checkIfSystemReady())
			{
				WalInfo("Checked CR - System is ready, proceed with Webpa start up\n");
				system("touch /var/tmp/webpaready");
				break;
				//Break out, System ready signal already delivered
			}
			else
			{
				WalInfo("Queried CR for system ready after waiting for 2 mins, it is still not ready\n");
			}
		}
		sleep(5);
		wait_time++;
	};
	// In case of Web PA restart, we should be having webpaready already touched.
	// In normal boot up we will reach here only when system ready is received.
	if(file != NULL)
	{
		WalInfo("/var/tmp/webpaready file exists, hence can proceed with webpa start up\n");
		fclose(file);
	}	
}

/**
 * @brief checkIfSystemReady Function to query CR and check if system is ready.
 * This is just in case webpa registers for the systemReadySignal event late.
 * If SystemReadySignal is already sent then this will return 1 indicating system is ready.
 */
static int checkIfSystemReady()
{
	char str[MAX_PARAMETERNAME_LEN/2];
	int val, ret;
	snprintf(str, sizeof(str), "eRT.%s", CCSP_DBUS_INTERFACE_CR);
	// Query CR for system ready
	ret = CcspBaseIf_isSystemReady(bus_handle, str, &val);
	WalInfo("checkIfSystemReady(): ret %d, val %d\n", ret, val);
	return val;
}

void addRowTable(const char *objectName, TableData *list,char **retObject, WAL_STATUS *retStatus)
{
        int ret = 0, index =0, status =0, retUpdate = 0, retDel = 0;
        char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
        char compName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dbusPath[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char tempParamName[MAX_PARAMETERNAME_LEN] = { 0 };
	
	WalPrint("objectName : %s\n",objectName);
	walStrncpy(paramName,objectName,sizeof(paramName));
        WalPrint("paramName before mapping : %s\n",paramName);
        status=IndexMpa_WEBPAtoCPE(paramName);
	if(status == -1)
	{
	 	ret = CCSP_ERR_INVALID_PARAMETER_NAME;
	 	WalError("paramName %s is not supported, invalid index. ret = %d\n", paramName,ret); 	
	}
	else
	{
		WalPrint("paramName after mapping : %s\n",paramName);
		ret = addRow(paramName,compName,dbusPath,&index);
		WalPrint("ret = %d index :%d\n",ret,index);
		WalPrint("parameterName: %s, CompName : %s, dbusPath : %s\n", paramName, compName, dbusPath);
		if(ret == CCSP_SUCCESS)
		{
			WalPrint("paramName : %s index : %d\n",paramName,index);
			snprintf(tempParamName,MAX_PARAMETERNAME_LEN,"%s%d.", paramName, index);
			WalPrint("tempParamName : %s\n",tempParamName);
		        retUpdate = updateRow(tempParamName,list,compName,dbusPath);
		        if(retUpdate == CCSP_SUCCESS)
		        {
				strcpy(*retObject, tempParamName);
				WalPrint("retObject : %s\n",*retObject);
		                WalPrint("Table is updated successfully\n");
				WalPrint("retObject before mapping :%s\n",*retObject);
				IndexMpa_CPEtoWEBPA(retObject);
				WalPrint("retObject after mapping :%s\n",*retObject);
		        }
		        else
			{
				ret = retUpdate;
				WalError("Failed to update row hence deleting the added row %s\n",tempParamName);
				retDel = deleteRow(tempParamName);
				if(retDel == CCSP_SUCCESS)
				{
					WalInfo("Reverted the add row changes.\n");
				}
				else
				{
					WalError("Failed to revert the add row changes\n");
				}
			}
		}
		else
		{
		        WalError("Failed to add table\n");
		}
		
        }
        WalPrint("ret : %d\n",ret);
        *retStatus = mapStatus(ret);
	WalPrint("retStatus : %d\n",*retStatus);
        	        
	
}
static int addRow(const char *object,char *compName,char *dbusPath,int *retIndex)
{
        int ret = 0, size = 0, index = 0,i=0;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };	
	componentStruct_t ** ppComponents = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);	
	
	WalPrint("<==========start of addRow ========>\n ");
	
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, object, l_Subsystem, &ppComponents, &size);
			
	WalPrint("size : %d, ret : %d\n",size,ret);

	if (ret == CCSP_SUCCESS && size == 1)
	{
		strcpy(compName,ppComponents[0]->componentName);
		strcpy(dbusPath,ppComponents[0]->dbusPath);
		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		WalError("Parameter name %s is not supported. ret = %d\n", object, ret);
		free_componentStruct_t(bus_handle, size, ppComponents);
		return ret;
	}
	WalInfo("parameterName: %s, CompName : %s, dbusPath : %s\n", object, compName, dbusPath);
	ret = CcspBaseIf_AddTblRow(
                bus_handle,
                compName,
                dbusPath,
                0,
                object,
                &index
            );
        WalPrint("ret = %d index : %d\n",ret,index);    
        if ( ret == CCSP_SUCCESS )
        {
                WalPrint("Execution succeed.\n");
                WalInfo("%s%d. is added.\n", object, index);               
                *retIndex = index;
                WalPrint("retIndex : %d\n",*retIndex);               
        }
        else
        {
                WalError("Execution fail ret :%d\n", ret);
        }
	WalPrint("<==========End of addRow ========>\n ");
	return ret;
}
static int updateRow(char *objectName,TableData *list,char *compName,char *dbusPath)
{
        int i=0, ret = -1,numParam =0, val_size = 0, retGet = -1;
        char **parameterNamesLocal = NULL; 
        char *faultParam = NULL;
	unsigned int writeID = CCSP_COMPONENT_ID_WebPA;	
	parameterValStruct_t *val= NULL;
	parameterValStruct_t **parameterval = NULL;
	
	WalPrint("<==========Start of updateRow ========>\n ");
  	numParam = list->parameterCount;
  	WalPrint("numParam : %d\n",numParam);
        parameterNamesLocal = (char **) malloc(sizeof(char *) * numParam);
        memset(parameterNamesLocal,0,(sizeof(char *) * numParam));        
        val = (parameterValStruct_t*) malloc(sizeof(parameterValStruct_t) * numParam);
	memset(val,0,(sizeof(parameterValStruct_t) * numParam));
        for(i =0; i<numParam; i++)
        {
        	parameterNamesLocal[i] = (char *) malloc(sizeof(char ) * MAX_PARAMETERNAME_LEN);
        	WalPrint("list->parameterNames[%d] : %s\n",i,list->parameterNames[i]);
                snprintf(parameterNamesLocal[i],MAX_PARAMETERNAME_LEN,"%s%s", objectName,list->parameterNames[i]);
                WalPrint("parameterNamesLocal[%d] : %s\n",i,parameterNamesLocal[i]);
        }
       
	WalInfo("parameterName: %s, CompName : %s, dbusPath : %s\n", parameterNamesLocal[0], compName, dbusPath);

	// To get dataType of parameter do bulk GET for all the input parameters in the requests
	retGet = CcspBaseIf_getParameterValues(bus_handle,
				compName, dbusPath,
				parameterNamesLocal,
				numParam, &val_size, &parameterval);
	WalPrint("After GPV ret: %d, val_size: %d\n",retGet,val_size);
	if(retGet == CCSP_SUCCESS && val_size > 0)
	{
		WalPrint("val_size : %d, numParam %d\n",val_size, numParam);

		for(i =0; i<numParam; i++)
		{
			WalPrint("parameterval[i]->parameterName %s, parameterval[i]->parameterValue %s, parameterval[i]->type %d\n",parameterval[i]->parameterName, parameterval[i]->parameterValue, parameterval[i]->type);
		        val[i].parameterName = parameterNamesLocal[i];
		        WalPrint("list->parameterValues[%d] : %s\n",i,list->parameterValues[i]);
		        val[i].parameterValue = list->parameterValues[i];
		        val[i].type = parameterval[i]->type;	
		}
		free_parameterValStruct_t (bus_handle, numParam, parameterval);		

		ret = CcspBaseIf_setParameterValues(bus_handle, compName, dbusPath, 0, writeID, val, numParam, TRUE, &faultParam);
		WalPrint("ret : %d\n",ret);
	}
	else
	{
		ret = retGet;
	}
        if(ret != CCSP_SUCCESS)
        {
                WalError("Failed to update row %d\n",ret);
		WAL_FREE(faultParam);
        }
               
        for(i =0; i<numParam; i++)
        {
        	WAL_FREE(parameterNamesLocal[i]);
        }
        WAL_FREE(parameterNamesLocal);
        WAL_FREE(val);
        WalPrint("<==========End of updateRow ========>\n ");
        return ret;
         
}
void deleteRowTable(const char *object,WAL_STATUS *retStatus)
{
        int ret = 0,status = 0, error =0;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	
	WalPrint("object : %s\n",object);
	walStrncpy(paramName,object,sizeof(paramName));
        WalPrint("paramName before mapping : %s\n",paramName);
	status=IndexMpa_WEBPAtoCPE(paramName);
	if(status == -1)
	{
	 	ret = CCSP_ERR_INVALID_PARAMETER_NAME;
	 	error = 1;
	 	WalError("Parameter %s is not supported, invalid index. ret = %d\n", paramName,ret); 	
	}
	else
	{
		WalPrint("paramName after mapping : %s\n",paramName);
		ret = deleteRow(paramName);
		if(ret == CCSP_SUCCESS)
		{
			WalPrint("%s is deleted Successfully.\n", paramName);
		
		}
		else
		{
			WalError("%s could not be deleted ret %d\n", paramName, ret);
		}
	}

	*retStatus = mapStatus(ret);
}


static int deleteRow(const char *object)
{
        int ret = 0, size =0;
	char compName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dbusPath[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	componentStruct_t ** ppComponents = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	
	WalPrint("<==========Start of deleteRow ========>\n ");
	
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, object, l_Subsystem, &ppComponents, &size);
	WalPrint("size : %d, ret : %d\n",size,ret);

	if (ret == CCSP_SUCCESS && size == 1)
	{
		strcpy(compName,ppComponents[0]->componentName);
		strcpy(dbusPath,ppComponents[0]->dbusPath);
		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		WalError("Parameter name %s is not supported. ret = %d\n", object, ret);
		free_componentStruct_t(bus_handle, size, ppComponents);
		return ret;
	}
	WalInfo("parameterName: %s, CompName : %s, dbusPath : %s\n", object, compName, dbusPath);
	ret = CcspBaseIf_DeleteTblRow(
                bus_handle,
                compName,
                dbusPath,
                0,
                object
            );
        WalPrint("ret = %d\n",ret);    
        if ( ret == CCSP_SUCCESS )
        {
                WalPrint("Execution succeed.\n");
                WalInfo("%s is deleted.\n", object);
        }
        else
        {
                WalError("Execution fail ret :%d\n", ret);
        }
	WalPrint("<==========End of deleteRow ========>\n ");
	return ret;
	
}

/**
 * @brief replaceTable replaces table data with the provided list of rows
 *
 * param[in] objectName table name
 * param[in] list provided list of rows
 * param[in] paramcount no.of rows 
 * param[out] retStatus Returns status
 */
void replaceTable(const char *objectName,TableData * list,int paramcount,WAL_STATUS *retStatus)
{
	int cnt = 0, ret = 0,numParams = 0,retIndex =0,addRet =0, isWalStatus = 0, cnt1 =0, rowCount = 0;
	char **deleteList = NULL;
	TableData * addList = NULL;
	char paramName[MAX_PARAMETERNAME_LEN] = {'\0'};
	WalPrint("<==========Start of replaceTable ========>\n ");
	walStrncpy(paramName,objectName,sizeof(paramName));
	WalPrint("paramName before Mapping : %s\n",paramName);
	// Index mapping 
	retIndex=IndexMpa_WEBPAtoCPE(paramName);
	if(retIndex == -1)
	{
	 	ret = CCSP_ERR_INVALID_PARAMETER_NAME;
	 	WalError("Parameter %s is not supported, invalid index. ret = %d\n", paramName,ret); 	
	}
	else
	{
		WalPrint("paramName after mapping : %s\n",paramName);
		ret = cacheTableData(paramName,paramcount,&deleteList,&rowCount,&numParams,&addList);
		WalPrint("ret : %d rowCount %d numParams: %d\n",ret,rowCount,numParams);
		if(ret == CCSP_SUCCESS)
		{
			WalInfo("Table (%s) has %d rows",paramName,rowCount);
			if(rowCount > 0)
			{
				WalPrint("-------- Printing table data ----------\n");
				for(cnt =0; cnt < rowCount; cnt++)
				{	
					WalPrint("deleteList[%d] : %s\n",cnt,deleteList[cnt]);
					if(paramcount != 0)
					{
					WalPrint("addList[%d].parameterCount : %d\n",cnt,addList[cnt].parameterCount);
						for (cnt1 = 0; cnt1 < addList[cnt].parameterCount; cnt1++)
						{
							WalPrint("addList[%d].parameterNames[%d] : %s,addList[%d].parameterValues[%d] : %s\n ",cnt,cnt1,addList[cnt].parameterNames[cnt1],cnt,cnt1,addList[cnt].parameterValues[cnt1]);
						}
					}
				}
				WalPrint("-------- Printed %d rows----------\n",rowCount);
				deleteAllTableData(deleteList,rowCount);
			}
			if(paramcount != 0)
			{
				addRet = addNewData(paramName,list,paramcount);
				ret = addRet;
				isWalStatus = 1;
				if(addRet != WAL_SUCCESS && rowCount > 0)
				{
					WalError("Failed to replace table, hence reverting the changes\n");
					addCachedData(paramName,addList,rowCount);
				}	
				for ( cnt = 0 ; cnt < rowCount ; cnt++)
				{
					for(cnt1 = 0; cnt1 < numParams; cnt1++)
					{
						WAL_FREE(addList[cnt].parameterNames[cnt1]);
						WAL_FREE(addList[cnt].parameterValues[cnt1]);
					}
					WAL_FREE(addList[cnt].parameterNames);
					WAL_FREE(addList[cnt].parameterValues);
				}
				WAL_FREE(addList);
			}	
		}
	}
	if(isWalStatus == 1)
	{
		*retStatus = ret;
	}
	else
	{
		*retStatus = mapStatus(ret);
	}
	WalPrint("Finally ----> ret: %d retStatus : %d\n",ret,*retStatus);
    WalPrint("<==========End of replaceTable ========>\n ");	
}

/**
 * @brief cacheTableData stores current data in the table into array of TableData
 *
 * param[in] objectName table name
 * param[in] paramcount no.of rows in the given data
 * param[out] rowList list of rowObjects in the array  
 * param[out] numRows return no.of rows cached
 * param[out] params return no.of params in each row 
 * param[out] list return rows with data
 */
static int cacheTableData(const char *objectName,int paramcount,char ***rowList,int *numRows,int *params,TableData ** list)
{
	int cnt =0,val_size = 0,ret = 0,size =0,rowCount = 0, cnt1=0;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = {'\0'};
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = {'\0'};
	componentStruct_t ** ppComponents = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	char *parameterNames[1];
	char **rows = NULL;
	char paramName[MAX_PARAMETERNAME_LEN] = {'\0'};
	char *p = NULL;
	p = &paramName;
	TableData * getList = NULL;
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	parameterValStruct_t **parameterval = NULL;
	WalPrint("<================ Start of cacheTableData =============>\n ");
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,dst_pathname_cr, objectName, l_Subsystem, &ppComponents, &size);
	WalPrint("size : %d, ret : %d\n",size,ret);
	if (ret == CCSP_SUCCESS && size == 1)
	{
		WalInfo("parameterName: %s, CompName : %s, dbusPath : %s\n", objectName, ppComponents[0]->componentName, ppComponents[0]->dbusPath);
		walStrncpy(paramName, objectName, sizeof(paramName));
		parameterNames[0] = p;
		ret = CcspBaseIf_getParameterValues(bus_handle,	ppComponents[0]->componentName, ppComponents[0]->dbusPath,  parameterNames,	1, &val_size, &parameterval);
		WalPrint("ret = %d val_size = %d\n",ret,val_size);
		if(ret == CCSP_SUCCESS && val_size > 0)
		{
			for (cnt = 0; cnt < val_size; cnt++)
			{
				WalPrint("parameterval[%d]->parameterName : %s,parameterval[%d]->parameterValue : %s\n ",cnt,parameterval[cnt]->parameterName,cnt,parameterval[cnt]->parameterValue);    
			}
			getTableRows(objectName,parameterval,val_size,&rowCount,&rows);
			WalPrint("rowCount : %d\n",rowCount);
			*rowList = rows;
			*numRows = rowCount;
			for(cnt = 0; cnt < rowCount; cnt++)
			{
				WalPrint("(*rowList)[%d] %s\n",cnt,(*rowList)[cnt]);
			}
			if(rowCount > 0 && paramcount > 0)
			{
				contructRollbackTableData(parameterval,val_size,rowList,rowCount,params,&getList);
				*list = getList;
				for(cnt =0; cnt < rowCount; cnt++)
				{	
					WalPrint("(*list)[%d].parameterCount : %d\n",cnt,(*list)[cnt].parameterCount);
					for (cnt1 = 0; cnt1 < (*list)[cnt].parameterCount; cnt1++)
					{
						WalPrint("(*list)[%d].parameterNames[%d] : %s,(*list)[%d].parameterValues[%d] : %s\n ",cnt,cnt1,(*list)[cnt].parameterNames[cnt1],cnt,cnt1,(*list)[cnt].parameterValues[cnt1]);
					}
				}
			}	
		}
		else
		{
			if(val_size == 0)
			{
				WalInfo("Table %s is EMPTY\n",objectName);
				*numRows = 0;
				*params = 0;
				rowList = NULL;
				list = NULL;
			}
		}
		free_componentStruct_t(bus_handle, size, ppComponents);
		free_parameterValStruct_t (bus_handle,val_size,parameterval);
	}
	else
	{
		WalError("Parameter name %s is not supported. ret = %d\n", objectName, ret);
		free_componentStruct_t(bus_handle, size, ppComponents);
		return ret;
	}
	WalPrint("<================ End of cacheTableData =============>\n ");
	return ret;
}

/**
 * @brief getTableRows dynamically gets list of rowObjects and row count
 *
 * param[in] objectName table name
 * param[in] parameterval arry of parameterValStruct 
 * param[in] paramCount total parameters in table  
 * param[out] numRows return no.of rows in table
 * param[out] rowObjects return array of rowObjects
 */
static void getTableRows(const char *objectName,parameterValStruct_t **parameterval, int paramCount, int *numRows,char ***rowObjects)
{
	int rows[MAX_ROW_COUNT] = {'\0'};
	int len = 0, cnt = 0, cnt1=0, objLen = 0, index = 0, rowCount = 0;
	char subStr[MAX_PARAMETERNAME_LEN] = {'\0'};
	char tempName[MAX_PARAMETERNAME_LEN] = {'\0'};
	WalPrint("---------- Start of getTableRows -----------\n");
	objLen = strlen(objectName);
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		len = strlen(parameterval[cnt]->parameterName);
		strncpy (subStr, parameterval[cnt]->parameterName + objLen, len-objLen);
		subStr[len-objLen] = '\0';
		sscanf(subStr,"%d.%s", &index, tempName);
		WalPrint("index : %d tempName : %s\n",index,tempName);
		if(cnt == 0)
		{
			rows[cnt1] = index;
		}
		else if(rows[cnt1] != index)
		{
			cnt1++;
			rows[cnt1] = index;
		} 		
	}
	rowCount = cnt1+1;
	WalPrint("rowCount : %d\n",rowCount);
	if(rowCount > 0)
	{
		*numRows = rowCount;
		*rowObjects = (char **)malloc(sizeof(char *) * rowCount);
		for(cnt = 0; cnt < rowCount; cnt++)
		{
			(*rowObjects)[cnt] = (char *)malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
			WalPrint("rows[%d] %d\n",cnt,rows[cnt]);
			snprintf((*rowObjects)[cnt],MAX_PARAMETERNAME_LEN,"%s%d.", objectName, rows[cnt]);
			WalPrint("(*rowObjects)[%d] %s\n",cnt,(*rowObjects)[cnt]);
		}
	}
	else
	{
		*numRows = 0;
		rowObjects = NULL;
	}
	WalPrint("---------- End of getTableRows -----------\n");
}

/**
 * @brief contructRollbackTableData constructs table data with name and value for roll back
 *
 * param[in] parameterval array of parameter values in table 
 * param[in] paramCount total parameters in table
 * param[in] rowList list of rows
 * param[in] rowCount no.of rows 
 * param[out] numParam return no.of paramters for each row
 * param[out] getList return list of table data with name and value
 */
static void contructRollbackTableData(parameterValStruct_t **parameterval,int paramCount,char ***rowList,int rowCount, int *numParam,TableData ** getList)
{
	int writableParamCount = 0, cnt = 0, cnt1 = 0, i = 0, params = 0;
	char **writableList = NULL;
	char subStr[MAX_PARAMETERNAME_LEN] = {'\0'};
	WalPrint("---------- Start of contructRollbackTableData -----------\n");
	getWritableParams((*rowList[0]), &writableList, &writableParamCount);
	WalInfo("writableParamCount : %d\n",writableParamCount);
	*numParam = writableParamCount;
	*getList = (TableData *) malloc(sizeof(TableData) * rowCount);
	params = paramCount / rowCount;
	for(cnt = 0; cnt < rowCount; cnt++)
	{
		(*getList)[cnt].parameterCount = writableParamCount;
		(*getList)[cnt].parameterNames = (char **)malloc(sizeof(char *) * writableParamCount);
		(*getList)[cnt].parameterValues = (char **)malloc(sizeof(char *) * writableParamCount);
		i = 0;
		cnt1 = cnt * params;
		for(; cnt1 < paramCount; cnt1++)
		{
			if(strstr(parameterval[cnt1]->parameterName,(*rowList)[cnt]) != NULL &&
					strstr(parameterval[cnt1]->parameterName,writableList[i]) != NULL)
			{
				WalPrint("parameterval[%d]->parameterName : %s,parameterval[%d]->parameterValue : %s\n ",cnt1,parameterval[cnt1]->parameterName,cnt1,parameterval[cnt1]->parameterValue);
				(*getList)[cnt].parameterNames[i] = (char *)malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
				(*getList)[cnt].parameterValues[i] = (char *)malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
				strcpy((*getList)[cnt].parameterNames[i],writableList[i]);
				WalPrint("(*getList)[%d].parameterNames[%d] : %s\n",cnt, i, (*getList)[cnt].parameterNames[i]);
				strcpy((*getList)[cnt].parameterValues[i],parameterval[cnt1]->parameterValue);
				WalPrint("(*getList)[%d].parameterValues[%d] : %s\n",cnt, i, (*getList)[cnt].parameterValues[i]);
				i++;
			}
		}
	}
	
	for(cnt = 0; cnt < writableParamCount; cnt++)
	{
		WAL_FREE(writableList[cnt]);
	}
	WAL_FREE(writableList);	
	WalPrint("---------- End of contructRollbackTableData -----------\n");
}

/**
 * @brief deleteAllTableData deletes all table data 
 *
 * param[in] deleteList array of rows from cached data
 * param[in] rowCount no.of rows to delete
 */
static void deleteAllTableData(char **deleteList,int rowCount)
{
	int cnt =0, delRet = 0; 
	WalPrint("---------- Start of deleteAllTableData -----------\n");
	for(cnt =0; cnt < rowCount; cnt++)
	{	
		delRet = deleteRow(deleteList[cnt]);
		WalPrint("delRet: %d\n",delRet);
		if(delRet != CCSP_SUCCESS)
		{
			WalError("deleteList[%d] :%s failed to delete\n",cnt,deleteList[cnt]);
		}
		WAL_FREE(deleteList[cnt]);
	}
	WAL_FREE(deleteList); 
	WalPrint("---------- End of deleteAllTableData -----------\n");
}

/**
 * @brief addNewData adds new data to the table 
 * param[in] objectName table name 
 * param[in] list table data to add
 * param[in] paramcount no.of rows to add
 */
static int addNewData(char *objectName,TableData * list,int paramcount)
{
	int cnt = 0,addRet = 0, i =0, delRet = 0;
	char paramName[MAX_PARAMETERNAME_LEN] = {'\0'};
	char **retObject = NULL;
	retObject = (char **)malloc(sizeof(char*) * paramcount);
	memset(retObject,0,(sizeof(char *) * paramcount));
	WalPrint("---------- Start of addNewData -----------\n");
	for(cnt =0; cnt < paramcount; cnt++)
	{				
		walStrncpy(paramName,objectName,sizeof(paramName));
		retObject[cnt] = (char *)malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
		addRowTable(paramName,&list[cnt],&retObject[cnt],&addRet);
		WalInfo("retObject[%d] : %s addRet : %d\n",cnt,retObject[cnt],addRet);
		if(addRet != WAL_SUCCESS)
		{
			WalError("Failed to add/update row retObject[%d] : %s, addRet : %d, hence deleting the already added rows\n", cnt, retObject[cnt], addRet);
			for(i= cnt-1; i >= 0; i--)
			{
				walStrncpy(paramName,retObject[i],sizeof(paramName));
				deleteRowTable(paramName, &delRet);
				WalPrint("delRet : %d\n",delRet);
				if(delRet != WAL_SUCCESS)
				{
					WalError("retObject[%d] :%s failed to delete, delRet %d\n",i,retObject[i], delRet);
					break;
				}		   
			}
			break;
		}						
	}
	for(cnt =0; cnt < paramcount; cnt++)
	{
		WAL_FREE(retObject[cnt]);
	}
	WAL_FREE(retObject);
	WalPrint("---------- End of addNewData -----------\n");
	return addRet;
}

/**
 * @brief addCachedData adds stored data to the table on roll back
 * param[in] objectName table name
 * param[in] addList list of table data to add
 * param[in] rowCount no.of rows to add
 */
static void addCachedData(const char *objectName,TableData * addList,int rowCount)
{
	int cnt = 0, addRet = 0;
	char paramName[MAX_PARAMETERNAME_LEN] = {'\0'};
	char **retRows = NULL;
	retRows = (char **)malloc(sizeof(char*) * rowCount);
	memset(retRows,0,(sizeof(char *) * rowCount));
	WalPrint("---------- Start of addCachedData -----------\n");
	for(cnt =0; cnt < rowCount; cnt++)
	{
		retRows[cnt] = (char *)malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
		walStrncpy(paramName,objectName,sizeof(paramName));
		addRowTable(paramName,&addList[cnt],&retRows[cnt],&addRet);
		WalInfo("retRows[%d] : %s addRet : %d\n",cnt,retRows[cnt],addRet);
		if(addRet == WAL_SUCCESS)
		{
			WalPrint("%s row is successfully added\n",retRows[cnt]);
		}
		WAL_FREE(retRows[cnt]);
	}					
	WAL_FREE(retRows);
	WalPrint("---------- End of addCachedData -----------\n");
}

/**
 * @brief getWritableParams gets writable parameters from stack
 * param[in] paramName row object
 * param[out] writableParams return list of writable params
 * param[out] paramCount count of writable params
 */
static void getWritableParams(char *paramName, char ***writableParams, int *paramCount)
{
	int cnt =0,val_size = 0,ret = 0, size = 0, cnt1 = 0, writableCount = 0, len = 0;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	componentStruct_t ** ppComponents = NULL;
	char *tempStr = NULL;
	char temp[MAX_PARAMETERNAME_LEN] = { 0 };
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	parameterInfoStruct_t **parameterInfo = NULL;
	WalPrint("==================== Start of getWritableParams ==================\n");
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
	WalPrint("size : %d, ret : %d\n",size,ret);
	if (ret == CCSP_SUCCESS && size == 1)
	{
		WalPrint("parameterName: %s, CompName : %s, dbusPath : %s\n", paramName, ppComponents[0]->componentName, ppComponents[0]->dbusPath);
		ret = CcspBaseIf_getParameterNames(bus_handle,ppComponents[0]->componentName, ppComponents[0]->dbusPath, paramName,1,&val_size,&parameterInfo);
		WalInfo("val_size : %d, ret : %d\n",val_size,ret);
		if(ret == CCSP_SUCCESS && val_size > 0)
		{
			*writableParams = (char **)malloc(sizeof(char *) * val_size);
			cnt1 = 0;
			for(cnt = 0; cnt < val_size; cnt++)
			{
				
				len = strlen(paramName);
				if(parameterInfo[cnt]->writable == 1)
				{
					strcpy(temp, parameterInfo[cnt]->parameterName);
					tempStr =temp + len;
					WalPrint("tempStr : %s\n",tempStr);
					if(strcmp(tempStr ,ALIAS_PARAM) == 0)
					{
						WalInfo("Skipping Alias parameter \n");
					}
					else
					{
						(*writableParams)[cnt1] = (char *)malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
						strcpy((*writableParams)[cnt1],tempStr);	
						cnt1++;
					}	
				}
			}
			*paramCount = writableCount = cnt1;
			WalPrint("writableCount %d\n",writableCount);
		}
		else
		{
			if(val_size == 0)
			{
				WalInfo("Table %s is EMPTY\n",paramName);
				*paramCount = 0;
				writableParams = NULL;
			}
		}
		free_componentStruct_t(bus_handle, size, ppComponents);
		free_parameterInfoStruct_t (bus_handle, val_size, parameterInfo);
	}
	else
	{
		WalError("Parameter name %s is not supported. ret = %d\n", paramName, ret);
		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	WalPrint("==================== End of getWritableParams ==================\n");
}

/**
 * @brief waitForConnectReadyCondition wait till PAM component is ready, its health is green
 * Wait for PAM as need to get CM_MAC, CMC, CID from it before connecting to server
 */
void waitForConnectReadyCondition()
{
	waitForComponentReady(RDKB_PAM_COMPONENT_NAME,RDKB_PAM_DBUS_PATH);
}

/**
 * @brief waitForOperationalReadyCondition wait till PSM and WiFi components are ready, health is green
 * Wifi is ready means the major operational feature is ready
 */
void waitForOperationalReadyCondition()
{
	// Wait till PSM, WiFi components are ready on the stack.
	waitForComponentReady(CCSP_DBUS_PSM,CCSP_DBUS_PATH_PSM);
	waitForComponentReady(RDKB_WIFI_COMPONENT_NAME,RDKB_WIFI_DBUS_PATH);
}

/**
 * @brief waitForComponentReady Wait till the given component is ready, its health is green
 * @param[in] compName RDKB Component Name
 * @param[in] dbusPath RDKB Dbus Path name
 */
static void waitForComponentReady(char *compName, char *dbusPath)
{
	char status[32] = {'\0'};
	int ret = -1;
	int count = 0;
	while(1)
	{
		checkComponentHealthStatus(compName, dbusPath, status,&ret);
		if(ret == CCSP_SUCCESS && (strcmp(status, "Green") == 0))
		{
			break;
		}
		else
		{
			if(count > 5)
			{
				count = 0;
				WalError("%s component Health, ret:%d, waiting\n", compName, ret);
			}
			sleep(5);
			count++;
		}
		
	} 
	WalInfo("%s component health is %s, continue\n", compName, status);
}

/**
 * @brief checkComponentHealthStatus Query the health of the given component
 * @param[in] compName RDKB Component Name
 * @param[in] dbusPath RDKB Dbus Path name
 * @param[out] status describes component health Red/Green
 * @param[out] retStatus error or status code returned by stack call
 */
static void checkComponentHealthStatus(char * compName, char * dbusPath, char *status, int *retStatus)
{
	int ret = 0, val_size = 0;
	parameterValStruct_t **parameterval = NULL;
	char *parameterNames[1] = {};
	char tmp[MAX_PARAMETERNAME_LEN];
	char str[MAX_PARAMETERNAME_LEN/2];     
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	
	sprintf(tmp,"%s.%s",compName, "Health");
	parameterNames[0] = tmp;
	      
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(str, sizeof(str), "%s%s", l_Subsystem, compName);
	WalPrint("str is:%s\n", str);
		
	ret = CcspBaseIf_getParameterValues(bus_handle, str, dbusPath,  parameterNames, 1, &val_size, &parameterval);
	WalPrint("ret = %d val_size = %d\n",ret,val_size);
	if(ret == CCSP_SUCCESS)
	{
		WalPrint("parameterval[0]->parameterName : %s parameterval[0]->parameterValue : %s\n",parameterval[0]->parameterName,parameterval[0]->parameterValue);
		strcpy(status, parameterval[0]->parameterValue);
		WalPrint("status of component:%s\n", status);
	}
	free_parameterValStruct_t (bus_handle, val_size, parameterval);
	
	*retStatus = ret;
}


/**
 * @brief getNotifyParamList Get notification parameters from intial NotifList
 * returns notif parameter names and size of list
 * @param[inout] paramList Initial Notif parameters list
 * @param[inout] size Notif List array size
 */
void getNotifyParamList(const char ***paramList, int *size)
{
	*size = sizeof(notifyparameters)/sizeof(notifyparameters[0]);
	WalPrint("Notify param list size :%d\n", *size);
	
      	*paramList = notifyparameters;

}
