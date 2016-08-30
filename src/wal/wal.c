#include "wal.h"
#include "waldb/waldb.h"
#include "libIBus.h"
#include "libIARM.h"
typedef unsigned int bool;
#include "hostIf_tr69ReqHandler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "rdk_debug.h"

#define MAX_NUM_PARAMETERS 2048
#define MAX_DATATYPE_LENGTH 48
#define MAX_PARAM_LENGTH TR69HOSTIFMGR_MAX_PARAM_LEN

/* WebPA Configuration for RDKV */
#define RDKV_WEBPA_COMPONENT_NAME            "webpaagent"
#define RDKV_WEBPA_CFG_FILE                  "/etc/webpa_cfg.json"
#define RDKV_WEBPA_CFG_FILE_OVERRIDE         "/opt/webpa_cfg.json"
#define RDKV_WEBPA_CFG_FILE_SRC              "/etc/webpa_cfg.json"
#define RDKV_WEBPA_CFG_FILE_SRC_OVERRIDE     "/opt/webpa_cfg.json"
#define RDKV_WEBPA_CFG_DEVICE_INTERFACE      "eth1"
#define RDKV_WEBPA_DEVICE_MAC                "Device.DeviceInfo.X_COMCAST-COM_STB_MAC"
#define RDKV_XPC_SYNC_PARAM_CID              "not.defined"
#define RDKV_XPC_SYNC_PARAM_CMC              "not.defined"
#define RDKV_XPC_SYNC_PARAM_SPV              "not.defined"
#define RDKV_FIRMWARE_VERSION                "Device.DeviceInfo.X_COMCAST-COM_FirmwareFilename"
#define RDKV_DEVICE_UP_TIME                  "Device.DeviceInfo.UpTime"
#define STR_NOT_DEFINED                      "not.defined"
#define LOG_MOD_WEBPA                        "LOG.RDK.WEBPAVIDEO"

static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t *param);
static int GetParamInfo(const char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int set_ParamValues_tr69hostIf (HOSTIF_MsgData_t param);
static int SetParamInfo(ParamVal paramVal);
static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);

static void converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType);
static void converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* walType);
static char *get_NetworkIfName(void);
static int g_dbhandle = 0;

static void converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType)
{
	if(!strcmp(ParamDataType,"string"))
		*pParamType = hostIf_StringType;
	else if(!strcmp(ParamDataType,"unsignedInt"))
		*pParamType = hostIf_UnsignedIntType;
	else if(!strcmp(ParamDataType,"int"))
		*pParamType = hostIf_IntegerType;
	else if(!strcmp(ParamDataType,"unsignedLong"))
		*pParamType = hostIf_UnsignedLongType;
	else if(!strcmp(ParamDataType,"boolean"))
		*pParamType = hostIf_BooleanType;
	else if(!strcmp(ParamDataType,"hexBinary"))
		*pParamType = hostIf_StringType;
	else
		*pParamType = hostIf_StringType;
}

static void converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* pwalType)
{
	switch(paramType)
	{
	case hostIf_StringType:
		*pwalType = WAL_STRING;
		break;
	case hostIf_UnsignedIntType:
		*pwalType = WAL_UINT;
		break;
	case hostIf_IntegerType:
		*pwalType = WAL_INT;
		break;
	case hostIf_BooleanType:
		*pwalType = WAL_BOOLEAN;
		break;
	case hostIf_UnsignedLongType:
		*pwalType = WAL_ULONG;
		break;
	case hostIf_DateTimeType:
		*pwalType = WAL_DATETIME;
		break;
	default:
		*pwalType = WAL_STRING;
		break;
	}
}


/**
 * @brief getValues interface returns the parameter values.
 *
 * getValues supports an option to pass wildward parameters. This can be achieved by passing an object name followed by '.'
 * instead of parameter name.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[out] paramValArr Two dimentional array of parameter name/value pairs.
 * @param[out] retValCount List of "number of parameters" for each input paramName. Usually retValCount will be 1 except
 * for wildcards request where it represents the number of param/value pairs retrieved for the particular wildcard parameter.
 * @param[out] retStatus List of Return status.
 */
void getValues(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, ParamVal ***paramValArr, int *retValCount, WAL_STATUS *retStatus)
//void getValues(const char *paramName[], int paramCount, ParamVal ***paramValArr, int *retValCount, WAL_STATUS *retStatus)
{
	int cnt = 0;
	int numParams = 0;
	for (cnt = 0; cnt < paramCount; cnt++) {
		retStatus[cnt] = GetParamInfo(paramName[cnt], &paramValArr[cnt],&numParams);
		retValCount[cnt]=numParams;
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"Parameter Name: %s return: %d\n",paramName[cnt],retStatus[cnt]);
	}
}

/**
 * @brief Initializes the Message Bus and registers WebPA component with the stack.
 *
 * @param[in] name WebPA Component Name.
 * @return WAL_STATUS.
 */

WAL_STATUS msgBusInit(const char *name)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
	DB_STATUS dbRet = DB_FAILURE;
	ret = IARM_Bus_Init(name);
	if(ret != IARM_RESULT_SUCCESS)
	{
		/*TODO: Log Error */
		return WAL_FAILURE;
	}

	ret = IARM_Bus_Connect();
	if(ret != IARM_RESULT_SUCCESS)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error connecting to IARM Bus for '%s'\r\n", name);
		return WAL_FAILURE;
	}

	// Load Document model
	dbRet = loaddb("/etc/data-model.xml",(void *)&g_dbhandle);

	if(dbRet != DB_SUCCESS)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error loading database\n");
		return WAL_FAILURE;
	}
}

/**
 * @brief Registers the notification callback function.
 *
 * @param[in] cb Notification callback function.
 * @return WAL_STATUS.
 */
WAL_STATUS RegisterNotifyCB(notifyCB cb)
{
	//TODO
	return WAL_SUCCESS;
}

/**
 * generic Api for get HostIf parameters through IARM_TR69Bus
**/
static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t *ptrParam)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;

	ptrParam->reqType = HOSTIF_GET;

	ret = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)ptrParam, sizeof(HOSTIF_MsgData_t));


	if(ret != IARM_RESULT_SUCCESS) {
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"[%s:%s:%d] Failed in IARM_Bus_Call(), with return value: %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
		return WAL_ERR_INVALID_PARAM;
	}
	else
	{
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"[%s:%s:%d] The value for param: %s is %s paramLen : %d\n", __FILE__, __FUNCTION__, __LINE__, ptrParam->paramName,ptrParam->paramValue, ptrParam->paramLen);
	}

	return WAL_SUCCESS;
}


static int GetParamInfo(const char *pParameterName, ParamVal ***parametervalArr,int *TotalParams)
{
	//Check if pParameterName is in the tree and convert to a list if a wildcard/branch
	const char wcard = '*'; // TODO: Currently support only wildcard character *
	int i = 0;
	int ret = WAL_FAILURE;
	DB_STATUS dbRet = DB_FAILURE;
	HOSTIF_MsgData_t Param = {0};

	if(g_dbhandle)
	{
		if(strchr(pParameterName,wcard))
		{
			char **getParamList = NULL;
			char **ParamDataTypeList = NULL;
			int paramCount = 0;

			/* Translate wildcard to list of parameters */
			getParamList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			if(getParamList!=NULL)
			{
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
				ret = WAL_FAILURE;
				goto exit0;
			}
			ParamDataTypeList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			if(ParamDataTypeList==NULL)
			{
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
				ret = WAL_FAILURE;
				goto exit0;
			}

			dbRet = getParameterList((void *)g_dbhandle,pParameterName,getParamList,ParamDataTypeList,&paramCount);
			*TotalParams = paramCount;
			parametervalArr[0] = (ParamVal **) malloc(paramCount * sizeof(ParamVal*));
			if(parametervalArr[0] == NULL)
			{
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
				ret = WAL_FAILURE;
				goto exit0;
			}
				
			memset(parametervalArr[0],0,paramCount * sizeof(ParamVal*));

			for(i = 0; i < paramCount; i++)
			{
				RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"%s:%s\n",getParamList[i],ParamDataTypeList[i]);
				strncpy(Param.paramName,getParamList[i],MAX_PARAM_LENGTH-1);
				Param.paramName[MAX_PARAM_LENGTH-1]='\0';

				// Convert ParamDataType to hostIf datatype
				converttohostIfType(ParamDataTypeList[i],&(Param.paramtype));
				Param.instanceNum = 0;
				parametervalArr[0][i]=malloc(sizeof(ParamVal));
				if(parametervalArr[0][i] == NULL)
				{
					RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
					ret = WAL_FAILURE;
					goto exit0;
				}
				memset(parametervalArr[0][i],0,sizeof(ParamVal));

				// Convert Param.paramtype to ParamVal.type
				converttoWalType(Param.paramtype,&(parametervalArr[0][i]->type));

				ret = get_ParamValues_tr69hostIf(&Param);

				if(ret == WAL_SUCCESS)
				{
					parametervalArr[0][i]->name=malloc(MAX_PARAM_LENGTH);
					char *ptrtoname = parametervalArr[0][i]->name;
					if(ptrtoname == NULL)
					{
						RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
						ret = WAL_FAILURE;
						goto exit0;
					}
					parametervalArr[0][i]->value=malloc(MAX_PARAM_LENGTH);
					char *ptrtovalue = parametervalArr[0][i]->value;
					if(ptrtovalue == NULL)
					{
						RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
						ret = WAL_FAILURE;
						goto exit0;
					}
					strncpy(ptrtoname,Param.paramName, MAX_PARAM_LENGTH-1);
					ptrtoname[MAX_PARAM_LENGTH-1] = '\0';
					strncpy(ptrtovalue,Param.paramValue, MAX_PARAM_LENGTH-1);
					ptrtovalue[MAX_PARAM_LENGTH-1] = '\0';
				}
				else
				{
					ret = WAL_FAILURE;
				}
				free(getParamList[i]);
				free(ParamDataTypeList[i]);
			}
exit0:
			if(ret != WAL_SUCCESS) // For success case generic layer would free up parametervalArr after consuming data
			{
				int j;
				for(j=0;j<i;j++)
				{
					if(parametervalArr[0][j]->name != NULL)
					{
						free(parametervalArr[0][j]->name);
						parametervalArr[0][j]->name = NULL;
					}
					if(parametervalArr[0][j]->value != NULL)
					{
						free(parametervalArr[0][j]->value);
						parametervalArr[0][j]->value = NULL;
					}	
					if(parametervalArr[0][j] != NULL)
					{
						free(parametervalArr[0][j]);
						parametervalArr[0][j] = NULL;
					}
				}
                		if(parametervalArr[0] != NULL)
					free(parametervalArr[0]);
			}
			if(getParamList != NULL)
				free(getParamList);
			if(ParamDataTypeList!=NULL)
				free(ParamDataTypeList);
			return ret;
		}
		else /* No wildcard, check whether given parameter is valid */
		{
			char *dataType = malloc(sizeof(char) * MAX_DATATYPE_LENGTH);
			if(dataType == NULL)
			{
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
				ret = WAL_FAILURE;
				goto exit1;
			}
			parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*));
			if(parametervalArr[0] == NULL)
			{
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
				ret = WAL_FAILURE;
				goto exit1;
			}
			memset(parametervalArr[0],0,sizeof(ParamVal*));
			parametervalArr[0][0]=malloc(sizeof(ParamVal));
			if(parametervalArr[0][0] == NULL)
			{
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
				ret = WAL_FAILURE;
				goto exit1;
			}
			parametervalArr[0][0]->name = NULL;
			parametervalArr[0][0]->value = NULL;
			parametervalArr[0][0]->type = 0;

			if(isParameterValid((void *)g_dbhandle,pParameterName,dataType))
			{
				*TotalParams = 1;
				strncpy(Param.paramName,pParameterName,MAX_PARAM_LENGTH-1);
				Param.paramName[MAX_PARAM_LENGTH-1]='\0';
				converttohostIfType(dataType,&(Param.paramtype));
				Param.instanceNum = 0;
				// Convert Param.paramtype to ParamVal.type
				RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"CMCSA:: GetParamInfo Param.paramtype is %d\n",Param.paramtype);
				converttoWalType(Param.paramtype,&(parametervalArr[0][0]->type));

				ret = get_ParamValues_tr69hostIf(&Param);
				if(ret == WAL_SUCCESS)
				{
					parametervalArr[0][0]->name=malloc(sizeof(char)*MAX_PARAM_LENGTH);
					if(parametervalArr[0][0]->name == NULL)
					{
						RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
						ret = WAL_FAILURE;
						goto exit1;
					}
					memset(parametervalArr[0][0]->name,0,sizeof(char)*MAX_PARAM_LENGTH);
					parametervalArr[0][0]->value=malloc(sizeof(char)*MAX_PARAM_LENGTH);
					if(parametervalArr[0][0]->value == NULL)
					{
						RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
						ret = WAL_FAILURE;
						goto exit1;
					}
					memset(parametervalArr[0][0]->value,0,sizeof(char)*MAX_PARAM_LENGTH);
					switch(Param.paramtype)
					{
						case hostIf_IntegerType:
						case hostIf_BooleanType:
							{
							char *ptrtovalue = parametervalArr[0][0]->value;
							snprintf(ptrtovalue,MAX_PARAM_LENGTH-1,"%d",*((int *)Param.paramValue));
							ptrtovalue[MAX_PARAM_LENGTH-1] = '\0';
							RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"CMCSA:: GetParamInfo value is %s\n",parametervalArr[0][0]->value);
							break;
							}
						case hostIf_UnsignedIntType:
                                                        snprintf(parametervalArr[0][0]->value,MAX_PARAM_LENGTH-1,"%u",*((unsigned int *)Param.paramValue));
                                                        parametervalArr[0][0]->value[MAX_PARAM_LENGTH-1] = '\0';
                                                        RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"CMCSA:: GetParamInfo value is %s\n",parametervalArr[0][0]->value);
                                                        break;
						case hostIf_UnsignedLongType:
                                                        snprintf(parametervalArr[0][0]->value,MAX_PARAM_LENGTH-1,"%u",*((unsigned long *)Param.paramValue));
                                                        parametervalArr[0][0]->value[MAX_PARAM_LENGTH-1] = '\0';
                                                        RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"CMCSA:: GetParamInfo value is %s\n",parametervalArr[0][0]->value);
                                                        break;
						case hostIf_StringType:
							{
							char *pValue = parametervalArr[0][0]->value;
							strncpy(pValue,Param.paramValue,MAX_PARAM_LENGTH-1 );
							pValue[MAX_PARAM_LENGTH-1] = '\0';
							RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"CMCSA:: GetParamInfo value is %s ptrtovalue %s\n",parametervalArr[0][0]->value,pValue);
							break;
							}
						default: // handle as string
							{
							char *ptrtodefvalue = parametervalArr[0][0]->value;
							strncpy(ptrtodefvalue,Param.paramValue, MAX_PARAM_LENGTH-1);
							ptrtodefvalue[MAX_PARAM_LENGTH-1] = '\0';
							RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"CMCSA:: GetParamInfo value is %s\n",parametervalArr[0][0]->value);
							break;
							}
					}
					char *ptrtoname = parametervalArr[0][0]->name;
					strncpy(ptrtoname,Param.paramName, MAX_PARAM_LENGTH-1);
					ptrtoname[MAX_PARAM_LENGTH-1] = '\0';
				}
				else
				{
					RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"get_ParamValues_tr69hostIf failed:ret is %d\n",ret);
					ret = WAL_FAILURE;
				}
			}
			else
			{
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA," Invalid Parameter name %s\n",pParameterName);
				return WAL_ERR_INVALID_PARAMETER_NAME;
			}
exit1:
			if(ret != WAL_SUCCESS) // For success case generic layer would free up parametervalArr after consuming data
			{
				if(parametervalArr[0][0]->value != NULL)
				{
					free(parametervalArr[0][0]->value);
					parametervalArr[0][0]->value = NULL;
				}
				if(parametervalArr[0][0]->name != NULL)
				{
					free(parametervalArr[0][0]->name);
					parametervalArr[0][0]->name = NULL;
				}
				if(parametervalArr[0][0] != NULL)
				{
					free(parametervalArr[0][0]);
					parametervalArr[0][0] = NULL;
				}
				if(parametervalArr[0])
				{
					free(parametervalArr[0]);
					parametervalArr[0] = NULL;
				}
			}
			if(dataType != NULL)
			{
				free(dataType);
				dataType = NULL;
			}
			return ret;
		}
	}
	return ret;
}

/**
 * @brief setValues interface sets the parameter value.
 *
 * @param[in] paramVal List of Parameter name/value pairs.
 * @param[in] paramCount Number of parameters.
 * @param[out] retStatus List of Return status.
 */
void setValues(const ParamVal paramVal[], const unsigned int paramCount, const unsigned int isAtomic, money_trace_spans *timeSpan, WAL_STATUS *retStatus,char * transaction_id)
//void setValues(const ParamVal paramVal[], int paramCount, WAL_STATUS *retStatus)
{
	int cnt=0;
	for(cnt = 0; cnt < paramCount; cnt++)
	{
		retStatus[cnt] = SetParamInfo(paramVal[cnt]);
	}
}

/**
 * generic Api for get HostIf parameters through IARM_TR69Bus
**/
static int set_ParamValues_tr69hostIf (HOSTIF_MsgData_t param)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
	param.reqType = HOSTIF_SET;
	ret = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME,
	                    IARM_BUS_TR69HOSTIFMGR_API_SetParams,
	                    (void *)&param,
	                    sizeof(param));
	if(ret != IARM_RESULT_SUCCESS) {
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"[%s:%s:%d] Failed in IARM_Bus_Call(), with return value: %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
		return WAL_ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"[%s:%s:%d] Set Successful for value : %s\n", __FILE__, __FUNCTION__, __LINE__, (char *)param.paramValue);
	}
	return WAL_SUCCESS;
}

static int SetParamInfo(ParamVal paramVal)
{

	int ret = WAL_SUCCESS;
	HOSTIF_MsgData_t Param = {0};
	memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));

	char *pdataType = NULL;
	pdataType = malloc(sizeof(char) * MAX_DATATYPE_LENGTH);
	if(pdataType == NULL)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
		return WAL_FAILURE;
	}

	if(isParameterValid((void *)g_dbhandle,paramVal.name,pdataType))
	{
		DATA_TYPE walType;
		converttohostIfType(pdataType,&(Param.paramtype));
		// Convert Param.paramtype to ParamVal.type
		converttoWalType(Param.paramtype,&walType);

		if(walType != paramVal.type)
		{
			RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA," Invalid Parameter type for %s\n",paramVal.name);
			return WAL_ERR_INVALID_PARAMETER_TYPE;
		}

		strncpy(Param.paramName, paramVal.name,MAX_PARAM_LENGTH-1);
		Param.paramName[MAX_PARAM_LENGTH-1]='\0';
		strncpy(Param.paramValue, paramVal.value,MAX_PARAM_LENGTH-1);
		Param.paramValue[MAX_PARAM_LENGTH-1]='\0';

		ret = set_ParamValues_tr69hostIf(Param);
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"set_ParamValues_tr69hostIf %d\n",ret);
	}
	else
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA," Invalid Parameter name %s\n",paramVal.name);
		return WAL_ERR_INVALID_PARAMETER_NAME;
	}
	return ret;
}

/**
 *
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[out] attr Two dimentional array of attribute name/value pairs.
 * @param[out] retAttrCount List of "number of attributes" for each input paramName.
 * @param[out] retStatus List of Return status.
 */
void getAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
//void getAttributes(const char *paramName[], int paramCount, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
{
	int cnt=0;
	for(cnt=0; cnt<paramCount; cnt++)
	{
		retStatus[cnt]=getParamAttributes(paramName[cnt], &attr[cnt], &retAttrCount[cnt]);
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"Parameter Name: %s, Parameter Attributes return: %d\n",paramName[cnt],retStatus[cnt]);
	}
}

static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams)
{
	// TODO:Implement Attributes
	return 0;
}

/**
 * @brief setAttributes interface sets the attribute values.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[in] attr List of attribute name/value pairs.
 * @param[out] retStatus List of Return status.
 */
void setAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, const AttrVal *attr[], WAL_STATUS *retStatus)
//void setAttributes(const char *paramName[], int paramCount, const AttrVal *attr[], WAL_STATUS *retStatus)
{
	int cnt=0;
	for(cnt=0; cnt<paramCount; cnt++)
	{
		retStatus[cnt] = setParamAttributes(paramName[cnt],attr[cnt]);
	}
}

static int setParamAttributes(const char *pParameterName, const AttrVal *attArr)
{
	// TODO:Implement Attributes
	return 0;
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
	int ret = 0;
	unsigned int rdkLogLevel = RDK_LOG_DEBUG;
	char *pTempChar = NULL;

	switch(level)
	{
		case WEBPA_LOG_ERROR:
			rdkLogLevel = RDK_LOG_ERROR;
			break;

		case WEBPA_LOG_INFO:
			rdkLogLevel = RDK_LOG_INFO;
			break;

		case WEBPA_LOG_PRINT:
			rdkLogLevel = RDK_LOG_DEBUG;
			break;
	}

	if( rdkLogLevel <= RDK_LOG_DEBUG )
	{
		pTempChar = (char*)malloc(4096);
		if(pTempChar)
		{
			va_start(arg, msg);
			ret = vsnprintf(pTempChar, 4096, msg,arg);
			va_end(arg);
			if(ret < 0)
			{
				perror(pTempChar);
			}
			RDK_LOG(rdkLogLevel,LOG_MOD_WEBPA, pTempChar);
			free(pTempChar);
		}
	}
}

char *getInterfaceNameFromConfig()
{
    char szBuf[256];
    static char szEthName[32] = "";
    FILE * fp = fopen("/etc/device.properties", "r");
    if(NULL != fp)
    {
        while(NULL != fgets(szBuf, sizeof(szBuf), fp))
        {
            char * pszTag = NULL;
            if(NULL != (pszTag = strstr(szBuf, "MOCA_INTERFACE")))
            {
                char * pszEqual = NULL;
                if(NULL != (pszEqual = strstr(pszTag, "=")))
                {
                    sscanf(pszEqual+1, "%s", szEthName);
                }
            }
        }
        fclose(fp);
    }
    return szEthName;
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
			ret = RDKV_WEBPA_COMPONENT_NAME;
			break;

		case WCFG_CFG_FILE:
                        if (access(RDKV_WEBPA_CFG_FILE_OVERRIDE, F_OK) != -1)
                        {
                                ret = RDKV_WEBPA_CFG_FILE_OVERRIDE;
                        }
                        else
                        {
                                ret = RDKV_WEBPA_CFG_FILE;
                        }
			break;

		case WCFG_CFG_FILE_SRC:
                        if (access(RDKV_WEBPA_CFG_FILE_SRC_OVERRIDE, F_OK) != -1)
                        {
                                ret = RDKV_WEBPA_CFG_FILE_SRC_OVERRIDE;
                        }
                        else
                        {
                                ret = RDKV_WEBPA_CFG_FILE_SRC;
                        }

			break;

		case WCFG_DEVICE_INTERFACE:
			ret = get_NetworkIfName();
			break;
		case WCFG_DEVICE_MAC:
			ret = RDKV_WEBPA_DEVICE_MAC;
			break;

		case WCFG_XPC_SYNC_PARAM_CID:
			ret = RDKV_XPC_SYNC_PARAM_CID;
			break;

		case WCFG_XPC_SYNC_PARAM_CMC:
			ret = RDKV_XPC_SYNC_PARAM_CMC;
			break;

		case WCFG_XPC_SYNC_PARAM_SPV:
			ret = RDKV_XPC_SYNC_PARAM_SPV;
			break;

                case WCFG_FIRMWARE_VERSION:
                        ret = RDKV_FIRMWARE_VERSION;
                        break;

                case WCFG_DEVICE_UP_TIME:
                        ret = RDKV_DEVICE_UP_TIME;
                        break;
		default:
			ret = STR_NOT_DEFINED;
	}
	
	return ret;
}

/**
 * @brief sendIoTMessage interface sends message to IoT. 
 *
 * @param[in] msg Message to be sent to IoT.
 * @return WAL_STATUS
 */
WAL_STATUS sendIoTMessage(const void *msg)
{
	//TODO: Implement sendIoTMessage fn
}

void addRowTable(const char *objectName,TableData *list,char **retObject, WAL_STATUS *retStatus)
{
	// TODO: Implement ADD row API for TR181 dynamic table objects
}

void deleteRowTable(const char *object,WAL_STATUS *retStatus)
{
	// TODO: Implement DELETE row API for TR181 dynamic table objects
}

void replaceTable(const char *objectName,TableData * list,int paramcount,WAL_STATUS *retStatus)
{
	// TODO: Implement REPLACE table (DELETE and ADD) API for TR181 dynamic table objects
}

void WALInit()
{
	// TODO: Implement any initialization required for webpa
}

void waitForConnectReadyCondition()
{
	// TODO: Implement wait for any dependent components for webpa to be ready to connect to server
}

void waitForOperationalReadyCondition()
{
	// TODO: Implement wait for any dependent components for webpa to be operational
}

void getNotifyParamList(const char ***paramList,int *size)
{
	//TODO: Implement Initial webpa notification SET if required
}

/* int main ( int arc, char **argv )
{
	// TODO:Implement main
	return 0;
}
*/

/* Get network interface from device properties */
char *get_NetworkIfName( void )
{
    static char *curIfName = NULL;
    RDK_LOG(RDK_LOG_TRACE1, LOG_MOD_WEBPA,"[%s] Enter %s\n", __FUNCTION__);
    if (!curIfName)
    {
	curIfName = ((access( "/tmp/wifi-on", F_OK ) != 0 ) ? getenv("MOCA_INTERFACE") : getenv("WIFI_INTERFACE"));
	RDK_LOG(RDK_LOG_DEBUG ,LOG_MOD_WEBPA," [%s] Interface Name : %s\n", __FUNCTION__, curIfName);
    }

    RDK_LOG(RDK_LOG_TRACE1, LOG_MOD_WEBPA,"[%s]Exit %s\n", __FUNCTION__);
    return curIfName;
}

