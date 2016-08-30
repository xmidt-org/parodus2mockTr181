/**
 * @file wrp_mgr.c
 *
 * @description This file is used to manage WRP GET, SET requests and responses. It fetches values from the device based on the request and translates to appropriate WRP response.
 *
 * Copyright (c) 2015  Comcast
 */
#include "stdlib.h"
#include "cJSON.h"
#include "nopoll.h"
#include "wrp_mgr.h"
#include "wal.h"
#include "math.h"
#include "msgpack.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define MAX_PARAMETER_LENGTH        512
#define MAX_RESULT_STR_LENGTH       128

#define WRP_MSG_PAYLOAD_PARAMETERS  "parameters"
#define WRP_MSG_PAYLOAD_OLD_CID     "old-cid"
#define WRP_MSG_PAYLOAD_NEW_CID     "new-cid"
#define WRP_MSG_PAYLOAD_CMC         "sync-cmc"

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
typedef enum
{
	WEBPA_STATUS_SUCCESS = 200,
	WEBPA_ADDROW_STATUS_SUCCESS = 201,
	WEBPA_STATUS_GENERAL_FALURE =  520,
	WEBPA_STATUS_CID_TEST_FAILED = 550,
	WEBPA_STATUS_CMC_TEST_FAILED = 551,
	WEBPA_STATUS_ATOMIC_GET_SET_FAILED = 552,
	WEBPA_STATUS_WIFI_BUSY = 530
} WEBPA_RESPONSE_STATUS_CODE;

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static char * substr(char *s, char* resValue, int end,int start);
static void mapWalStatusToStatusMessage(WAL_STATUS status, char *result);
static void constructPayloadJsonResponse(const char * paramName[],ParamVal ***parametervalArr, int paramCount, int *count,WAL_STATUS *ret, cJSON *parameters);
static void checkReturnStatus(int paramCount, WAL_STATUS *ret1, WAL_STATUS *ret2,WAL_STATUS *ret);
static void getStatusCode(WEBPA_RESPONSE_STATUS_CODE *statusCode, int paramCount, WAL_STATUS * ret);
static void constructPayloadGetAttrJsonResponse(const char * paramName[],
	AttrVal ***attr, int paramCount, int *count,WAL_STATUS *ret, cJSON *parameters);
int processWildCardRequest(cJSON* response,cJSON* paramObj,const char * wildcardParamName, money_trace_spans *timeSpan);
void handleTimeSpanForWildcardGet(money_trace_spans *wildcardSpan, money_trace_spans *timeSpan);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
// To process GET request for binary payload
/**
 * @brief processGetRequest interface handles GET parameter requests.
 *returns the parameter values from stack and forms the response 
 *
 * @param[in] request cJSON GET parameter request.
 * @param[out] timeSpan timing_values for each component.
 * @param[out] response Returns GET response as CSJON Object.
 */
void processGetRequest(cJSON* request, money_trace_spans *timeSpan, cJSON* response)
{
	cJSON *parameters = NULL, *paramObj = NULL, *paramArray = NULL;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	WAL_STATUS status;
	WAL_STATUS *ret = NULL;
	char *result = NULL, *param = NULL;
	int *count = NULL;
	int *parameterFlag = NULL;
	ParamVal ***parametervalArr = NULL;
	const char *getParamList[MAX_PARAMETER_LENGTH];
	const char *wildcardGetParamList[MAX_PARAMETER_LENGTH];
	int wildcardParamCount = 0,nonWildcardParamCount = 0, i = 0;
	int paramCount = 0,cnt2=0, l=0, wildCardIndex=0, retWildcardStatus =0;
	money_trace_spans *wildcardSpan = NULL;
	
	paramArray = cJSON_GetObjectItem(request, "names");
	paramCount = cJSON_GetArraySize(paramArray);
	ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);
	count = (int *) malloc(sizeof(int) * paramCount);
	parameterFlag = (int *) malloc(sizeof(int) * paramCount);
	parametervalArr = (ParamVal ***) malloc( sizeof(ParamVal **) * paramCount);
	result = (char *) malloc(sizeof(char) * 100);
	
	for (i = 0; i < paramCount; i++) 
	{
		// initialize return and count to -1
		ret[i] = -1;
		count[i] = -1;

		param = (cJSON_GetArrayItem(paramArray, i)->valuestring);
		WalPrint("param : %s\n",param);
		
		// If input parameter length is greater than 512, send error and dont send request to wal
		if(strlen(param) > MAX_PARAMETER_LENGTH)
		{
			//form response and error out
			cJSON_AddNumberToObject(response, "statusCode", 520);	
			cJSON_AddStringToObject(response, "message", "Invalid Param");
			WAL_FREE(result);
			WAL_FREE(parameterFlag);
			WAL_FREE(parametervalArr);
			WAL_FREE(ret);
			WAL_FREE(count);
			WalPrint("freed and done with Invalid param case of long parameter name\n");	
			return;
		}
		// Check for Wildcard that ends with "."
		if(param[(strlen(param)-1)] == '.')
		{
			wildcardGetParamList[wildcardParamCount] = param;
			wildcardParamCount++; 
			parameterFlag[i]=1;
		}
		else
		{
			getParamList[nonWildcardParamCount] = param;	
			nonWildcardParamCount++;
			parameterFlag[i]=0;		
		}
	}
	
	if(nonWildcardParamCount>0)
	{
		WalPrint("before getValues\n");
		getValues(getParamList, nonWildcardParamCount, timeSpan, parametervalArr, count, ret);
		WalPrint("after getValues\n");
		getStatusCode(&statusCode, nonWildcardParamCount, ret);	
		cJSON_AddNumberToObject(response, "statusCode", statusCode);
		// Prepare payload response with parameter values only in case of success as GET is atomic
		if(statusCode != WEBPA_STATUS_SUCCESS)
		{
			WalPrint("Inside statusCode != WEBPA_STATUS_SUCCESS\n");
			status = ret[0];

			mapWalStatusToStatusMessage(status, result);
			cJSON_AddStringToObject(response, "message", result);
			WAL_FREE(result);
			WalPrint("freeing parameterFlag\n");
			WAL_FREE(parameterFlag);
			WAL_FREE(parametervalArr);
			WAL_FREE(ret);
			WAL_FREE(count);
			return;

		}
	}

	cJSON_AddItemToObject(response, WRP_MSG_PAYLOAD_PARAMETERS, parameters = cJSON_CreateArray());

	for (i = 0; i < paramCount; i++) 
	{			 	
		cJSON_AddItemToArray(parameters, paramObj = cJSON_CreateObject());
		WalPrint("parameterFlag[i]: %d \n",parameterFlag[i]);

		if(parameterFlag[i] == 0)
		{
			WalPrint("non wildcard parameter \n");
			status = ret[0];
			WalPrint("status %d \n",status);
			WalPrint("parametervalArr[0][%d]->name : %s\n",cnt2,parametervalArr[0][cnt2]->name);
			cJSON_AddStringToObject(paramObj, "name", parametervalArr[0][cnt2]->name);

			if(parametervalArr[0][cnt2]->value == NULL) 
			{
				cJSON_AddStringToObject(paramObj, "value","ERROR");
			}
			else
			{
				cJSON_AddStringToObject(paramObj, "value", parametervalArr[0][cnt2]->value);
				WalPrint("parametervalArr[0][%d]->value : %s\n",cnt2,parametervalArr[0][cnt2]->value);
			}
			
			WalPrint("parametervalArr[0][%d]->type : %d\n",cnt2,parametervalArr[0][cnt2]->type);
			cJSON_AddNumberToObject(paramObj, "dataType", parametervalArr[0][cnt2]->type);
			cJSON_AddNumberToObject(paramObj, "parameterCount", 1);
			mapWalStatusToStatusMessage(status, result);
			cJSON_AddStringToObject(paramObj, "message", result);

			WAL_FREE(parametervalArr[0][cnt2]->name);		
			WAL_FREE(parametervalArr[0][cnt2]->value);
			WAL_FREE(parametervalArr[0][cnt2]);

			cnt2++;
		}
		else if (parameterFlag[i] == 1)
		{
		        if(timeSpan)
		        {
                                wildcardSpan = (money_trace_spans *) malloc(sizeof(money_trace_spans));
                                memset(wildcardSpan,0,(sizeof(money_trace_spans))); 
			}
			retWildcardStatus= processWildCardRequest(response,paramObj,wildcardGetParamList[wildCardIndex], wildcardSpan);
			if(timeSpan)
			{			          
		                handleTimeSpanForWildcardGet(wildcardSpan,timeSpan);						      
			}
			if(retWildcardStatus>0)
			{
				WalPrint("freeing parameterFlag\n");
				WAL_FREE(parameterFlag);
				int j=0;
				for(j=i;j<nonWildcardParamCount;j++)
				{
					if(parametervalArr[0][j])
					{
						WalPrint("freeing parametervalArr[0][%d]\n",j);
						WAL_FREE(parametervalArr[0][j]->name);		
						WAL_FREE(parametervalArr[0][j]->value);
						WAL_FREE(parametervalArr[0][j]);
					}
					else
					{
						break;
					}
				}

				if(nonWildcardParamCount>0)
				{
					WalPrint("freeing parametervalArr[0]\n");
					WAL_FREE(parametervalArr[0]);
				}
				
				WAL_FREE(parametervalArr);
				WAL_FREE(ret);
				WAL_FREE(count);
				WAL_FREE(result);
				return;
			}
			wildCardIndex++;
		}

	}
	
	WalPrint("Added to response object\n");
	WalPrint("freeing parameterFlag\n");
        WAL_FREE(parameterFlag);
        if(nonWildcardParamCount>0)
	{
		WalPrint("freeing parametervalArr[0]\n");
		WAL_FREE(parametervalArr[0]);
        }
	WAL_FREE(parametervalArr);
	WAL_FREE(ret);
	WAL_FREE(count);
	WAL_FREE(result);
	WalPrint("freeing done\n");
}

int processWildCardRequest(cJSON* response,cJSON* paramObj,const char * wildcardParamName, money_trace_spans *timeSpan)
{
   	const char *getParamListWildCard[MAX_PARAMETER_LENGTH];
	WAL_STATUS *retWildCard = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * 1);
	int *countWildCard = (int *) malloc(sizeof(int) * 1);
	*retWildCard = -1;
	*countWildCard = -1;
   	WEBPA_RESPONSE_STATUS_CODE statusCodeWildCard = WEBPA_STATUS_GENERAL_FALURE;
   	cJSON *paramval, *paramvalObj;
	WAL_STATUS statusWildCard;
	ParamVal ***parametervalArrWildCard = (ParamVal ***) malloc( sizeof(ParamVal **) * 1);	
	int l=0, countOfWildCardsParameters = 0;
	char * resultWildCard = (char *) malloc(sizeof(char) * 100);
   	//getParamListWildCard[0] = wildcardGetParamList[wildCardIndex];
   	getParamListWildCard[0] = wildcardParamName;
   	WalPrint("getParamListWildCard[0] :%s\n",getParamListWildCard[0]);
   	
   	getValues(getParamListWildCard, 1, timeSpan, parametervalArrWildCard, countWildCard, retWildCard);
   	WalPrint("After GetValues stack call for wildcard\n");
   	getStatusCode(&statusCodeWildCard, 1, retWildCard);	
   	WalPrint("After getStatusCode wildcard\n");

	if(cJSON_GetObjectItem(response, "statusCode") == NULL)
	{
		WalPrint("adding statusCode in response\n");
		cJSON_AddNumberToObject(response, "statusCode", statusCodeWildCard);
	}
	// Prepare payload response with parameter values only in case of success as GET is atomic
	WalPrint("checking for statuscode done\n");
	if(statusCodeWildCard != WEBPA_STATUS_SUCCESS)
	{
		WalPrint("Inside statusCode != WEBPA_STATUS_SUCCESS\n");						
		
		if(cJSON_GetObjectItem(response, WRP_MSG_PAYLOAD_PARAMETERS) != NULL)
   	        {
			cJSON_DeleteItemFromObject(response,WRP_MSG_PAYLOAD_PARAMETERS);
			 WalPrint("deleted PARAMETERS from response\n");
		}
		
		if(cJSON_GetObjectItem(response, "statusCode") != NULL)
   		 {
   		        cJSON_DeleteItemFromObject(response,"statusCode");
   		        WalPrint("deleted statusCode in response\n");					   	        
	   	 }
   	 	WalPrint("adding statusCode in response\n");
   	 	cJSON_AddNumberToObject(response, "statusCode", statusCodeWildCard);						
		
		statusWildCard = retWildCard[0];

		mapWalStatusToStatusMessage(statusWildCard, resultWildCard);
		cJSON_AddStringToObject(response, "message", resultWildCard);
		WAL_FREE(resultWildCard);
		
		WAL_FREE(parametervalArrWildCard);
		WAL_FREE(retWildCard);
		WAL_FREE(countWildCard);
		return 1;
	}

   	WalPrint("after wildcard getvalues\n");
   	countOfWildCardsParameters = countWildCard[0];  	
   	WalPrint("countOfWildCardsParameters : %d\n",countOfWildCardsParameters);
   
	//without child elements
	if (countOfWildCardsParameters == 0)
	{
	 	WalPrint("wild card without child elements\n");
	 	WalPrint("wildcardParamName: %s\n",wildcardParamName);
		cJSON_AddStringToObject(paramObj, "name", wildcardParamName);
		cJSON_AddStringToObject(paramObj, "value","EMPTY");
		cJSON_AddNumberToObject(paramObj, "parameterCount", countOfWildCardsParameters);
		statusWildCard = retWildCard[0];
		WalPrint("before mapWalStatusToStatusMessage\n");
		mapWalStatusToStatusMessage(statusWildCard, resultWildCard);
		WalPrint("after mapWalStatusToStatusMessage\n");
		cJSON_AddStringToObject(paramObj, "message", resultWildCard);

		WalPrint("freeing without child case\n");
		WAL_FREE(parametervalArrWildCard);
		WAL_FREE(retWildCard);
		WAL_FREE(countWildCard);
		WalPrint("end of without child case\n");					 
	}
	else
	{
		l=0;
		cJSON_AddStringToObject(paramObj, "name", wildcardParamName);
		cJSON_AddItemToObject(paramObj, "value", paramval =
		cJSON_CreateArray());
	
		while (l < countOfWildCardsParameters) 
		{
			cJSON_AddItemToArray(paramval, paramvalObj = cJSON_CreateObject());	
			WalPrint("parametervalArrWildCard[0][%d]->name : %s\n",l,parametervalArrWildCard[0][l]->name);
			cJSON_AddStringToObject(paramvalObj, "name",parametervalArrWildCard[0][l]->name);
			if(parametervalArrWildCard[0][l]->value == NULL) 
			{
				cJSON_AddStringToObject(paramvalObj, "value","ERROR");
			}
			else
			{
				cJSON_AddStringToObject(paramvalObj, "value", parametervalArrWildCard[0][l]->value);
				WalPrint("parametervalArrWildCard[0][%d]->value : %s\n",l,parametervalArrWildCard[0][l]->value);
			}
			cJSON_AddNumberToObject(paramvalObj, "dataType",parametervalArrWildCard[0][l]->type);

			WAL_FREE(parametervalArrWildCard[0][l]->name);		
			WAL_FREE(parametervalArrWildCard[0][l]->value);
			WAL_FREE(parametervalArrWildCard[0][l]);

			l++;
	   
		}
	
		WalPrint("after while loop\n");			
		cJSON_AddNumberToObject(paramObj, "dataType", 11);
		cJSON_AddNumberToObject(paramObj, "parameterCount", countOfWildCardsParameters);
		mapWalStatusToStatusMessage(retWildCard[0], resultWildCard);
		cJSON_AddStringToObject(paramObj, "message", resultWildCard);
                
                WalPrint("freeing parametervalArrWildCard[0]\n");
		WAL_FREE(parametervalArrWildCard[0]);
		WAL_FREE(parametervalArrWildCard);
		WAL_FREE(retWildCard);
		WAL_FREE(countWildCard);						
	}  	
	WAL_FREE(resultWildCard);
	return 0;
}

void handleTimeSpanForWildcardGet(money_trace_spans *wildcardSpan, money_trace_spans *timeSpan)
{
        unsigned int cnt = 0, count = 0;
        int matchFlag = 0;
        WalPrint("----------------- start of handleTimeSpanForWildcardGet -------\n");
        count = timeSpan->count;
        WalPrint("count : %d\n",count);
        if(count == 0)
        {
                for(cnt = 0; cnt < wildcardSpan->count; cnt++)
                {
                        WalPrint("wildcardSpan->spans[%d].name : %s, wildcardSpan->spans[%d].start : %llu, wildcardSpan->spans[%d].duration : %lu \n ",cnt,wildcardSpan->spans[cnt].name,cnt,wildcardSpan->spans[cnt].start,cnt,wildcardSpan->spans[cnt].duration);
                }

                timeSpan->spans = wildcardSpan->spans;
                timeSpan->count = wildcardSpan->count;

                for(cnt = 0; cnt < timeSpan->count; cnt++)
                {
                        WalPrint("timeSpan->spans[%d].name : %s, timeSpan->spans[%d].start : %llu, timeSpan->spans[%d].duration : %lu \n ",cnt,timeSpan->spans[cnt].name,cnt,timeSpan->spans[cnt].start,cnt,timeSpan->spans[cnt].duration);
                }
        }
        else
        {
                for(cnt = 0; cnt < count; cnt++)
                {
                        if(!strcmp(timeSpan->spans[cnt].name, wildcardSpan->spans[0].name))
		        {
		                WalPrint("timeSpan->spans[%d].duration : %lu, wildcardSpan->spans[0].duration : %lu\n",cnt,timeSpan->spans[cnt].duration,wildcardSpan->spans[0].duration);
		                timeSpan->spans[cnt].duration = timeSpan->spans[cnt].duration + wildcardSpan->spans[0].duration;
		                WalPrint("timeSpan->spans[%d].duration : %lu\n",cnt,timeSpan->spans[cnt].duration);
		                WAL_FREE(wildcardSpan->spans[0].name);
		                matchFlag = 1;
		                break;
		        }
		}
	        if(matchFlag == 0)
	        {
	                timeSpan->count = timeSpan->count +1;
                        WalPrint("timeSpan->count : %d\n",timeSpan->count);
                        timeSpan->spans = (money_trace_span *) realloc(timeSpan->spans,sizeof(money_trace_span)* timeSpan->count); 
                        timeSpan->spans[timeSpan->count - 1].name = (char *)malloc(sizeof(char)*MAX_PARAMETER_LENGTH/2);
                        strcpy(timeSpan->spans[timeSpan->count - 1].name,wildcardSpan->spans[0].name);
                        WalPrint("timeSpan->spans[%d].name : %s\n",timeSpan->count - 1,timeSpan->spans[timeSpan->count - 1].name);
                        WalPrint("wildcardSpan->spans[0].start : %llu\n",wildcardSpan->spans[0].start);
                        timeSpan->spans[timeSpan->count - 1].start = wildcardSpan->spans[0].start;
                        WalPrint("timeSpan->spans[%d].start : %llu\n",timeSpan->count - 1,timeSpan->spans[timeSpan->count - 1].start);
                        WalPrint("wildcardSpan->spans[0].duration : %lu\n",wildcardSpan->spans[0].duration);
                        timeSpan->spans[timeSpan->count - 1].duration = wildcardSpan->spans[0].duration;
                        WalPrint("timeSpan->spans[%d].duration : %lu\n",timeSpan->count - 1,timeSpan->spans[timeSpan->count - 1].duration);
                        WAL_FREE(wildcardSpan->spans[0].name);
	        }                 
                WAL_FREE(wildcardSpan->spans);
                WAL_FREE(wildcardSpan);
        }
        WalPrint("----------------- End of handleTimeSpanForWildcardGet -------\n");
}
void processDeleteRowRequest(cJSON* request, cJSON* response)
{
	int paramCount =0,i =0;
	char *deleteList = NULL;
	char * result = NULL;
	WAL_STATUS status;
	WAL_STATUS *ret = NULL;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	
	WalPrint("<===========start of processDeleteRequest()==============>\n");
	
	ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS));
        deleteList = cJSON_GetObjectItem(request,"row")->valuestring;
        WalPrint("deleteList : %s\n",deleteList);
        deleteRowTable(deleteList,ret);        
      
        WalPrint("ret[0] :%d\n",ret[0]);
	getStatusCode(&statusCode, 1 , ret);
	WalPrint("statusCode :%d\n",statusCode);
	cJSON_AddNumberToObject(response, "statusCode", statusCode);
	
	status = ret[0];
	result = (char *) malloc(sizeof(char) * MAX_RESULT_STR_LENGTH);
	mapWalStatusToStatusMessage(status, result);
	WalPrint("result : %s\n",result);
	cJSON_AddStringToObject(response, "message", result);
	WAL_FREE(result);   			
	WAL_FREE(ret); 
        WalPrint("<===========End of processDeleteRequest()==============>\n ");	
}
void processAddRowRequest(cJSON* request, cJSON* response)
{
	cJSON *paramArray = NULL;
	int i=0, itemSize =0;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	WAL_STATUS status;
	char *result = NULL;
	char *objectName = NULL;
	WAL_STATUS *ret = NULL;
	TableData *list = NULL;
	char *retObject = NULL;
	
	WalPrint("<=================Start of processAddRowRequest()===================>\n");
	
	retObject = (char *)malloc(sizeof(char)*MAX_PARAMETER_LENGTH);
	paramArray = cJSON_GetObjectItem(request, "row");
	itemSize = cJSON_GetArraySize(paramArray);
	WalPrint("itemSize : %d\n",itemSize);
	ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS));		
	list = (TableData *)malloc(sizeof(TableData));
	objectName = cJSON_GetObjectItem(request,"table")->valuestring;
        WalPrint("objectName : %s\n",objectName);
        list->parameterNames = (char **) malloc(sizeof(char *) * itemSize);
	list->parameterValues = (char **) malloc(sizeof(char *) * itemSize);
        for (i = 0 ; i < itemSize ; i++)
	{
		list->parameterNames[i] = cJSON_GetArrayItem(paramArray, i)->string;
		WalPrint("list->parameterNames[%d] : %s\n",i,list->parameterNames[i]);
		list->parameterValues[i] = cJSON_GetArrayItem(paramArray, i)->valuestring;
		WalPrint("list->parameterValues[%d] : %s\n",i,list->parameterValues[i]);	
	}
	list->parameterCount = itemSize;
	
	addRowTable(objectName,list,&retObject,ret);
	WalPrint("ret[0] :%d\n",ret[0]);
	getStatusCode(&statusCode, 1, ret);
	WalPrint("statusCode :%d\n",statusCode);

	if(statusCode == WEBPA_STATUS_SUCCESS)
	{
		//changing statuscode to 201 as per requirement
		statusCode = WEBPA_ADDROW_STATUS_SUCCESS;
	        cJSON_AddNumberToObject(response, "statusCode", statusCode);
		WalPrint("Adding %s to response\n",retObject);
		cJSON_AddStringToObject(response, "row", retObject);
	}
	else
	{ 
	  	cJSON_AddNumberToObject(response, "statusCode", statusCode);
	}
	WalPrint("Final statusCode :%d\n",statusCode);
	status = ret[0];
	result = (char *) malloc(sizeof(char) * MAX_RESULT_STR_LENGTH);
	mapWalStatusToStatusMessage(status, result);
	WalPrint("result : %s\n",result);
	cJSON_AddStringToObject(response, "message", result);
	
	WAL_FREE(list->parameterNames);
	WAL_FREE(list->parameterValues);
	
	WAL_FREE(result);
	WAL_FREE(retObject);
	WAL_FREE(ret);         
        WalPrint("<=================End of processAddRowRequest()=============>\n");	
}
void processReplaceRowRequest(cJSON* request, cJSON* response)
{
	cJSON *paramArray = NULL;
	cJSON * subitem = NULL;
	char *objectName = NULL;
	int i =0,j=0, itemSize =0,subitemSize =0,ret = 0;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	TableData *addList = NULL;
	char *result = NULL;
	
	WalPrint("<============Start of processReplaceRowRequest()===============>\n");
	
	result = (char *) malloc(sizeof(char) * MAX_RESULT_STR_LENGTH);
	paramArray = cJSON_GetObjectItem(request, "rows");
	itemSize = cJSON_GetArraySize(paramArray);
	WalPrint("itemSize : %d\n",itemSize);	
	objectName = cJSON_GetObjectItem(request,"table")->valuestring;
	WalPrint("objectName : %s\n",objectName);
	
	// Initialize add/update List
	if(itemSize != 0)
	{
		addList = (TableData *) malloc(sizeof(TableData) * itemSize);

		for ( i = 0 ; i < itemSize ; i++)
		{
			subitem = cJSON_GetArrayItem(paramArray, i);
			subitemSize = cJSON_GetArraySize(subitem);
			WalPrint("subitemSize: %d\n",subitemSize);

			addList[i].parameterNames = (char **) malloc(sizeof(char *) * subitemSize);
			addList[i].parameterValues = (char **) malloc(sizeof(char *) * subitemSize);
			addList[i].parameterCount = subitemSize;

			for( j = 0 ; j < subitemSize ; j++)
			{
				addList[i].parameterNames[j] = cJSON_GetArrayItem(subitem, j)->string;
				WalPrint("addList[%d].parameterNames[%d] : %s\n",i,j,addList[i].parameterNames[j]);				
				addList[i].parameterValues[j] = cJSON_GetArrayItem(subitem, j)->valuestring;
				WalPrint("addList[%d].parameterValues[%d] : %s\n",i,j,addList[i].parameterValues[j]);			
			}
		}
	}

	replaceTable(objectName,addList,itemSize,&ret);

	// Free addList
	if(itemSize != 0)
	{
		for ( i = 0 ; i < itemSize ; i++)
		{
			WalPrint("addList[%d].parameterNames before freeing points to %p\n",i,addList[i].parameterNames);
			WAL_FREE(addList[i].parameterNames);
			WalPrint("addList[%d].parameterValues before freeing points to %p\n",i,addList[i].parameterValues);
			WAL_FREE(addList[i].parameterValues);
		}
	
		WalPrint("addList before freeing point to %p\n",addList);
		WAL_FREE(addList); 
	}	
	
	getStatusCode(&statusCode,1, &ret);
	WalPrint("statusCode :%d\n",statusCode);
	cJSON_AddNumberToObject(response, "statusCode", statusCode);
	mapWalStatusToStatusMessage(ret, result);
	WalPrint("result : %s\n",result);
	cJSON_AddStringToObject(response, "message", result);
	WAL_FREE(result); 
	WalPrint("<===============End of processReplaceRowRequest===============>\n");
}

void processGetAttrRequest(cJSON* request, money_trace_spans *timeSpan, cJSON* response)
{
	cJSON *parameters;
	cJSON *paramArray = cJSON_GetObjectItem(request, "names");
	int i;
	bool error = false;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	WAL_STATUS status;
	char *result = NULL;
	const char *getParamList[MAX_PARAMETER_LENGTH];
	int paramCount = cJSON_GetArraySize(paramArray);
	WAL_STATUS *ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);
	int *count = (int *) malloc(sizeof(int) * paramCount);
	AttrVal ***attr = (AttrVal ***) malloc( sizeof(AttrVal **) * paramCount);
	
	if((cJSON_GetObjectItem(request, "attributes") != NULL) && (strncmp(cJSON_GetObjectItem(request, "attributes")->valuestring, "notify", 6) == 0))
	{
		for (i = 0; i < paramCount; i++) 
		{
			// initialize return to -1
			ret[i] = -1;

			char *param = (cJSON_GetArrayItem(paramArray, i)->valuestring);
			WalPrint("param : %s\n",param);

			// Check for Wildcard that ends with "."
			if(param[(strlen(param)-1)] == '.')
			{
			   error = true;
			   break;     
			}
			getParamList[i] = param;			
		}
		
		if(error == false)
		{
			// Call method to get value from stack
			getAttributes(getParamList, paramCount, timeSpan, attr, count, ret);
			getStatusCode(&statusCode, paramCount, ret);	
			cJSON_AddNumberToObject(response, "statusCode", statusCode);
			WalPrint("statusCode : %d\n",statusCode);
			// Prepare payload response with parameter values only in case of success as GET is atomic
			if(statusCode == WEBPA_STATUS_SUCCESS)
			{
				cJSON_AddItemToObject(response, WRP_MSG_PAYLOAD_PARAMETERS, parameters = cJSON_CreateArray());
				constructPayloadGetAttrJsonResponse(getParamList, attr, paramCount, count, ret, parameters);
			}
			else
			{
				for (i=0; i<paramCount; i++) 
				{
					if (WAL_SUCCESS == ret[i]) 
					{
						WAL_FREE(attr[0][i]->name);		
						WAL_FREE(attr[0][i]->value);
						WAL_FREE(attr[0][i]);
					} 
					else 
					{
						status = ret[i];
					}
				}
				result = (char *) malloc(sizeof(char) * MAX_RESULT_STR_LENGTH);
				mapWalStatusToStatusMessage(status, result);
				cJSON_AddStringToObject(response, "message", result);
				WAL_FREE(result);
			}
		}
		else
		{
			WalError("----------------- Return error as wildcard not supported -----------\n");
			cJSON_AddNumberToObject(response, "statusCode", WEBPA_STATUS_ATOMIC_GET_SET_FAILED);
			cJSON_AddStringToObject(response, "message", "Invalid Param");
		}
	}
	else
	{
		WalError("----------------- Invalid input 'attributes'-----------\n");
		cJSON_AddNumberToObject(response, "statusCode", WEBPA_STATUS_ATOMIC_GET_SET_FAILED);
		cJSON_AddStringToObject(response, "message", "Invalid attributes");
	}
	WAL_FREE(attr);
	WAL_FREE(ret);
	WAL_FREE(count);
}

static void constructPayloadGetAttrJsonResponse(const char * paramName[],
	AttrVal ***attr, int paramCount, int *count,WAL_STATUS *ret, cJSON *parameters) {
	int cnt, notification;
	cJSON *paramObj, *attributes;
	char *result = (char *) malloc(sizeof(char) * 100);
	for (cnt = 0; cnt < paramCount; cnt++) 
	{
		WAL_STATUS status = ret[cnt];
		WalPrint("count[%d]: %d\n",cnt,count[cnt]);
		
		cJSON_AddItemToArray(parameters, paramObj = cJSON_CreateObject());
		WalPrint("attr[0][%d]->name : %s\n",cnt,attr[0][cnt]->name);
		cJSON_AddStringToObject(paramObj, "name", attr[0][cnt]->name);
		cJSON_AddItemToObject(paramObj, "attributes",attributes = cJSON_CreateObject());
		WalPrint("attr[0][%d]->value : %s\n",cnt,attr[0][cnt]->value);
		notification = atoi(attr[0][cnt]->value);
		cJSON_AddNumberToObject(attributes, "notify", notification);
		mapWalStatusToStatusMessage(status, result);
		cJSON_AddStringToObject(paramObj, "message", result);
		
		WAL_FREE(attr[0][cnt]->name);		
		WAL_FREE(attr[0][cnt]->value);
		WAL_FREE(attr[0][cnt]);
	}
	
	WAL_FREE(result);
	WAL_FREE(attr[0]);
}


// To process SET request for binary payload
/**
 * @brief processSetAtomicRequest interface handles SET parameter requests in an simple set and in atomic way.
 *sets the parameter values and attributes to stack and forms the response 
 * @param[in] request cJSON SET parameter request.
 * @param[in] setType Flag to specify if the type of set operation.
 * @param[out] timeSpan timing_values for each component.
 * @param[out] response Returns SET response as CSJON Object.
 */
void processSetRequest(cJSON* request, WEBPA_SET_TYPE setType, money_trace_spans *timeSpan, cJSON* response,char * transaction_id)
{ 
	cJSON *reqParamObj, *parameters, *resParamObj;
	reqParamObj=NULL;
	int flag = 0, i = 0, retReason= 0;
	const char *getParamList[MAX_PARAMETER_LENGTH];
	char *result = NULL;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	char message[MAX_RESULT_STR_LENGTH]={'\0'};
	walStrncpy(message,"Failure",sizeof(message));

	cJSON *paramArray = cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_PARAMETERS);
	int paramCount = cJSON_GetArraySize(paramArray);	
        ParamVal *parametervalArr = (ParamVal *) malloc(sizeof(ParamVal) * paramCount);
	
	WAL_STATUS *ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);
	
	for (i = 0; i < paramCount; i++) 
	{
		// initialize return to -1
		ret[i] = -1;		
		reqParamObj = cJSON_GetArrayItem(paramArray, i);
		parametervalArr[i].name = cJSON_GetObjectItem(reqParamObj, "name")->valuestring;
		WalPrint("Parameter name is %s \n",parametervalArr[i].name);
		
		// If input parameter is wildcard ending with "." then send error as wildcard is not supported for SET
		if(parametervalArr[i].name[(strlen(parametervalArr[i].name)-1)] == '.')
		{
			flag = 1;			
			walStrncpy(message, "Wildcard SET is not supported",sizeof(message));
		   	break;
		}
		// Prevent SET of CMC/CID through WebPA
		if(strcmp(parametervalArr[i].name, XPC_SYNC_PARAM_CID) == 0 || strcmp(parametervalArr[i].name, XPC_SYNC_PARAM_CMC) == 0)
		{
			flag = 1;			
			walStrncpy(message, "SET of CMC or CID is not supported",sizeof(message));
		   	break;
		}

		getParamList[i] = parametervalArr[i].name;
		
		// Parameter value should not be null
		if (cJSON_GetObjectItem(reqParamObj, "value") == NULL)
		{
			WalError("Parameter value field is not available\n");
			flag = 1;			
			walStrncpy(message, "Parameter value field is not available",sizeof(message));
		   	break;
		}
		else
		{
			if(cJSON_GetObjectItem(reqParamObj, "value")->valuestring != NULL && strlen(cJSON_GetObjectItem(reqParamObj, "value")->valuestring) == 0)
			{
				WalError("Parameter value is null\n");
				flag = 1;			
				walStrncpy(message, "Parameter value is null",sizeof(message));
				break;
			}
			else if(cJSON_GetObjectItem(reqParamObj, "value")->valuestring == NULL)
			{
				WalError("Parameter value field is not a string\n");
				flag = 1;			
				walStrncpy(message, "Invalid Parameter value",sizeof(message));
				break;
			}
			else
			{
				parametervalArr[i].value = cJSON_GetObjectItem(reqParamObj, "value")->valuestring;
			}
		}

		// Parameter dataType should not be null
		if (cJSON_GetObjectItem(reqParamObj, "dataType") == NULL)
		{
			flag = 1;			
			walStrncpy(message, "Parameter dataType is null",sizeof(message));
		   	break;
		}
		else
		{
			parametervalArr[i].type = cJSON_GetObjectItem(reqParamObj, "dataType")->valueint;
		}

		// Detect device reboot through WEBPA and log message for device reboot through webpa	
		if(strcmp(parametervalArr[i].name, WEBPA_DEVICE_REBOOT_PARAM) == 0 && strcmp(parametervalArr[i].value, WEBPA_DEVICE_REBOOT_VALUE) == 0)
		{
		        ParamVal *parametervalAr = (ParamVal *) malloc(sizeof(ParamVal));
		        
			// Using printf to log message to ArmConsolelog.txt.0 on XB3
			printf("RDKB_REBOOT : Reboot triggered through WEBPA\n");
			WalInfo("RDKB_REBOOT : Reboot triggered through WEBPA\n");
			parametervalAr[0].name = "Device.DeviceInfo.X_RDKCENTRAL-COM_LastRebootReason";
			parametervalAr[0].value = "webpa-reboot";
			parametervalAr[0].type = WAL_STRING;
					
			setValues(parametervalAr, 1, setType, NULL, &retReason,NULL);
			if(retReason != WAL_SUCCESS)
			{
				WalError("Failed to set Reason with status %d\n",retReason);
			}
			else
			{
				WalPrint("Successfully set Reason with status %d\n",retReason);
			}
			WAL_FREE(parametervalAr);
		}
	}
	
	if (flag != 1)
	{
		// Initialize response
		cJSON_AddItemToObject(response, WRP_MSG_PAYLOAD_PARAMETERS, parameters =cJSON_CreateArray());
		
		setValues(parametervalArr, paramCount, setType, timeSpan, ret,transaction_id);
		
		getStatusCode(&statusCode, paramCount, ret);
	
		cJSON_AddNumberToObject(response, "statusCode", statusCode);
	
		result = (char *) malloc(sizeof(char) * MAX_RESULT_STR_LENGTH);
		for (i = 0; i < paramCount; i++) 
		{
			WAL_STATUS status = ret[i];
			cJSON_AddItemToArray(parameters, resParamObj = cJSON_CreateObject());
			cJSON_AddStringToObject(resParamObj, "name", getParamList[i]);
			mapWalStatusToStatusMessage(status, result);
			cJSON_AddStringToObject(resParamObj, "message", result);
		}
		WAL_FREE(result); 
	}
	else
	{
		cJSON_AddNumberToObject(response, "statusCode", WEBPA_STATUS_ATOMIC_GET_SET_FAILED);
		cJSON_AddStringToObject(response, "message", message);
	}

	WAL_FREE(parametervalArr);
	WAL_FREE(ret);
}

/**
 * @brief processTestandSetRequest interface handles Test and SET parameter requests. 
 * Checks cid values and then does SET in atomic way. 
 * Sets the parameter values to stack and forms the payload JSON response.
 *
 * @param[in] request cJSON SET parameter request.
 * @param[out] response Returns response as CSJON Object.
 */
void processTestandSetRequest(cJSON *request, money_trace_spans *timeSpan, cJSON *response,char * transaction_id)
{ 
	cJSON *reqParamObj = NULL, *resParamObj1 = NULL ,*resParamObj2 = NULL, *parameters = NULL;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	WAL_STATUS setCidStatus = WAL_SUCCESS, setCmcStatus = WAL_SUCCESS;
	char *oldCID = NULL;
	char *newCID = NULL;
	char *dbCID = NULL;
	char newCMC[32]={'\0'};
	char *syncCMC = NULL;
	char *dbCMC = NULL;
	int i, paramCount, flag = 0, retReason = 0;
	char message[MAX_RESULT_STR_LENGTH]={'\0'};
	walStrncpy(message,"Failure",sizeof(message));   
 	
	snprintf(newCMC, sizeof(newCMC),"%d", CHANGED_BY_XPC);
	if(cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_OLD_CID) != NULL)
	{
		oldCID = cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_OLD_CID)->valuestring;
	}
	if(cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_NEW_CID) != NULL)
	{
		newCID = cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_NEW_CID)->valuestring;
	}

	// Get CMC from device database
	dbCMC = getSyncParam(SYNC_PARAM_CMC);
	// Get CID from device database
	dbCID = getSyncParam(SYNC_PARAM_CID);

	if(dbCMC != NULL && dbCID != NULL)
	{
		if(cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_CMC) != NULL)
		{
			syncCMC = cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_CMC)->valuestring;
			if(strcmp(syncCMC, dbCMC) != 0) // Error WEBPA_STATUS CMC_TEST_FAILED
			{
				statusCode = WEBPA_STATUS_CMC_TEST_FAILED;
				walStrncpy(message,"CMC test failed",sizeof(message));
				WalError("CMC check failed in Test And Set request as CMC (%s) did not match device CMC (%s) \n",syncCMC,dbCMC);
			}
		}

		if ((statusCode == WEBPA_STATUS_CMC_TEST_FAILED) || (newCID == NULL))
		{
			if(newCID == NULL && statusCode != WEBPA_STATUS_CMC_TEST_FAILED)
			{
				walStrncpy(message,"New-Cid is missing",sizeof(message));
				WalError("New-Cid input parameter is not present in Test And Set request\n");
			}
		}
		else
		{
			if (oldCID != NULL && strcmp(oldCID, dbCID) != 0) // Error WEBPA_STATUS_CID_TEST_FAILED
			{
				WalError("Test and Set Failed as old CID (%s) didn't match device CID (%s)\n", oldCID, dbCID);
				statusCode = WEBPA_STATUS_CID_TEST_FAILED;
				walStrncpy(message,"CID test failed",sizeof(message));
			}
			else // Test Success. Process Set.
			{
				// Set new CID & CMC
				setCidStatus = setSyncParam(SYNC_PARAM_CID, newCID);
				if(strcmp(dbCMC, newCMC) != 0)
				{
					setCmcStatus = setSyncParam(SYNC_PARAM_CMC, newCMC);
				}
				if((setCidStatus != WAL_SUCCESS) || (setCmcStatus != WAL_SUCCESS)) // Error WEBPA_STATUS_ATOMIC_GET_SET_FAILED
				{
					WalError("Error setting CID/CMC\n");
					statusCode = WEBPA_STATUS_ATOMIC_GET_SET_FAILED;
					walStrncpy(message,"Error setting CID/CMC",sizeof(message));
				}
				else // Check Parameters
				{
					cJSON *paramArray = cJSON_GetObjectItem(request, WRP_MSG_PAYLOAD_PARAMETERS);
					if (paramArray == NULL) // No Parameters
					{
						WalPrint("Test And Set doesn't have any parameter\n");
						statusCode = WEBPA_STATUS_SUCCESS;
						walStrncpy(message,"Success",sizeof(message));
					}
					else //Process parameters
					{
						paramCount = cJSON_GetArraySize(paramArray);
					
						const char *getParamList[MAX_PARAMETER_LENGTH];
						ParamVal *parametervalArr = (ParamVal *) malloc(sizeof(ParamVal) * paramCount);
						WAL_STATUS *ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);

						for (i = 0; i < paramCount; i++) 
						{
							reqParamObj = cJSON_GetArrayItem(paramArray, i);
							parametervalArr[i].name = cJSON_GetObjectItem(reqParamObj, "name")->valuestring;
							WalPrint("Parameter name is %s \n",parametervalArr[i].name);

							// If input parameter is wildcard ending with "." then send error as wildcard is not supported for TEST_AND_SET							
							if(parametervalArr[i].name[(strlen(parametervalArr[i].name)-1)] == '.')
							{
								flag = 1;	
								statusCode = WEBPA_STATUS_ATOMIC_GET_SET_FAILED;		
								walStrncpy(message, "Wildcard TEST_AND_SET is not supported",sizeof(message));
							   	break;
							}
							// Prevent SET of CMC/CID through WebPA
							if(strcmp(parametervalArr[i].name, XPC_SYNC_PARAM_CID) == 0 || strcmp(parametervalArr[i].name, XPC_SYNC_PARAM_CMC) == 0)
							{
								flag = 1;			
								statusCode = WEBPA_STATUS_ATOMIC_GET_SET_FAILED;
								walStrncpy(message, "Invalid Input parameter - CID/CMC value cannot be set",sizeof(message));
								break;
							}
							getParamList[i] = parametervalArr[i].name;
                                                        if (cJSON_GetObjectItem(reqParamObj, "value") == NULL) 
                                                        {
                                                                WalError("Parameter value field is not available\n");
                                                                flag = 1;			
                                                                walStrncpy(message, "Parameter value field is not available",sizeof(message));
                                                                break;
                                                        }
                                                        else
                                                        {
                                                                if(cJSON_GetObjectItem(reqParamObj, "value")->valuestring != NULL && strlen(cJSON_GetObjectItem(reqParamObj, "value")->valuestring) == 0)
                                                                {
                                                                        WalError("Parameter value is null\n");
                                                                        flag = 1;			
                                                                        walStrncpy(message, "Parameter value is null",sizeof(message));
                                                                        break;
                                                                }
                                                                else if(cJSON_GetObjectItem(reqParamObj, "value")->valuestring == NULL)
                                                                {
                                                                        WalError("Parameter value field is not a string\n");
                                                                        flag = 1;			
                                                                        walStrncpy(message, "Invalid Parameter value",sizeof(message));
                                                                        break;
                                                                }
                                                                else
                                                                {       
                                                                        parametervalArr[i].value = cJSON_GetObjectItem(reqParamObj, "value")->valuestring;
                                                                }
                                                        }
							if (cJSON_GetObjectItem(reqParamObj, "dataType") == NULL) 
							{
								parametervalArr[i].type = WAL_NONE;
							}
							else
							{
								parametervalArr[i].type = cJSON_GetObjectItem(reqParamObj, "dataType")->valueint;
							}
							// Detect device reboot through WEBPA and log message for device reboot through webpa	
							if(strcmp(parametervalArr[i].name, WEBPA_DEVICE_REBOOT_PARAM) == 0 && strcmp(parametervalArr[i].value, WEBPA_DEVICE_REBOOT_VALUE) == 0)
							{
								ParamVal *parametervalAr = (ParamVal *) malloc(sizeof(ParamVal) );
		        
			                                        // Using printf to log message to ArmConsolelog.txt.0 on XB3
			                                        printf("RDKB_REBOOT : Reboot triggered through WEBPA\n");
			                                        WalInfo("RDKB_REBOOT : Reboot triggered through WEBPA\n");
			                                        parametervalAr[0].name = "Device.DeviceInfo.X_RDKCENTRAL-COM_LastRebootReason";
			                                        parametervalAr[0].value = "webpa-reboot";
			                                        parametervalAr[0].type = WAL_STRING;
			
			                                        setValues(parametervalAr, 1, WEBPA_ATOMIC_SET_XPC, NULL, &retReason,NULL);
			                                        
			                                        if(retReason != WAL_SUCCESS)
			                                        {
				                                        WalError("Failed to set Reason with status %d\n",retReason);
			                                        }
			                                        else
			                                        {
				                                        WalPrint("Successfully set Reason and counter with status %d\n",retReason);
			                                        }
			                                        WAL_FREE(parametervalAr);
			    
							}
						}
					
						if (flag != 1)
						{
							setValues(parametervalArr, paramCount, WEBPA_ATOMIC_SET_XPC, timeSpan, ret,transaction_id); // Atomic Set
						}
					
						if (flag == 1 || ret[0] != WAL_SUCCESS) // Atomic Set failed
						{
							statusCode = WEBPA_STATUS_ATOMIC_GET_SET_FAILED;
							if(flag != 1)
							{											
								walStrncpy(message,"Atomic Set failed",sizeof(message));
							}
							// Revert the device CID to dbCID
							setCidStatus = setSyncParam(SYNC_PARAM_CID, dbCID);
							if(strcmp(dbCMC, newCMC) != 0)
							{
								setCmcStatus = setSyncParam(SYNC_PARAM_CMC, dbCMC);
							}
							if((setCidStatus != WAL_SUCCESS) || (setCmcStatus != WAL_SUCCESS))
							{
								WalError("Error setting CID/CMC\n");
								// Can't recover from this failure
							}
							else
							{
								WalInfo("Reverted device CID to %s\n", oldCID);
							}
						}
						else // Success
						{
							statusCode = WEBPA_STATUS_SUCCESS;
							walStrncpy(message,"Success",sizeof(message));
						}

						WAL_FREE(parametervalArr);
						WAL_FREE(ret);
					}
				}
			}
		}
	}
	else
	{
		WalError("Failed to Get CMC, CID value\n");
	}
		
	WalPrint("statusCode : %d\n",statusCode);
	// Construct response
	cJSON_AddNumberToObject(response, "statusCode", statusCode);	
	cJSON_AddStringToObject(response, "message", message);
		
	if(dbCMC != NULL && dbCID != NULL)
	{
		cJSON_AddItemToObject(response, WRP_MSG_PAYLOAD_PARAMETERS, parameters = cJSON_CreateArray());
		cJSON_AddItemToArray(parameters, resParamObj1 = cJSON_CreateObject());
		cJSON_AddStringToObject(resParamObj1, "name", XPC_SYNC_PARAM_CID);
		if(statusCode == WEBPA_STATUS_SUCCESS)
		{
			cJSON_AddStringToObject(resParamObj1, "value", newCID);
		}
		else
		{
			cJSON_AddStringToObject(resParamObj1, "value", dbCID);
		}

		cJSON_AddItemToArray(parameters, resParamObj2 = cJSON_CreateObject());
		cJSON_AddStringToObject(resParamObj2, "name", XPC_SYNC_PARAM_CMC);
		if(statusCode == WEBPA_STATUS_SUCCESS)
		{
			cJSON_AddNumberToObject(resParamObj2, "value", atoi(newCMC));
		}
		else
		{
			cJSON_AddNumberToObject(resParamObj2, "value", atoi(dbCMC));
		}
	}
	WAL_FREE(dbCID);
	WAL_FREE(dbCMC);	
}

/**
 * @brief processIoTRequest interface handles IoT requests. 
 *
 * @param[in] request cJSON SET parameter request.
 * @param[out] response Returns response as CSJON Object.
 */
void processIoTRequest(cJSON *request, cJSON *response)
{
	char *msg = NULL;

	msg = cJSON_PrintUnformatted(request);
	WalInfo("processIoTRequest request msg: %s\n", msg);

	if(sendIoTMessage((void *)msg) == WAL_SUCCESS)
	{
		cJSON_AddNumberToObject(response, "statusCode", WEBPA_STATUS_SUCCESS);
		cJSON_AddStringToObject(response, "message", "Success");
	}
	else
	{
		cJSON_AddNumberToObject(response, "statusCode", WEBPA_STATUS_GENERAL_FALURE);
		cJSON_AddStringToObject(response, "message", "Failed to send message to IoT");
	}
	WAL_FREE(msg);
}

/*
* @brief send the Binary response for GET request.
* @param[in] conn The Websocket connection object.
* @param[in] str contains response to be sent
* @param[in] bufferSize length of response to be sent 
* @param[out] send Binary response for GET request.
*/
void * sendResponse(noPollConn * conn,char *str, int bufferSize )
{
	int bytesWritten,len,max,position,cnt,indx,length;
	char *substring;
	length=-1;
	len = bufferSize;
	max=50000;
	long long int flush=2000000;
	position=0;
	char * resValueNew = NULL;
	int size1 = -1;
	int i;
	if(len>max) 
	{
		indx=ceil(len/max);
		cnt=max;
		for (i = 0; i < indx; ++i) 
		{
			int size = cnt - position + 2;// 1 for the inclusive limits and another 1 for the \0
			char * resValue = (char*)malloc(size);
			substring = substr(str,resValue,size,position);
			length = strlen(substring);
			if(i==0) 
			{
				bytesWritten=__nopoll_conn_send_common(conn, substring, length, nopoll_false, 0, NOPOLL_BINARY_FRAME);
			}
			else 
			{
				bytesWritten=__nopoll_conn_send_common(conn, substring, length, nopoll_false, 0, NOPOLL_CONTINUATION_FRAME);
			}
			position=cnt+1;
			cnt=max+cnt;
			if (bytesWritten != length) 
			{
				WalError("Failed to send bytes %d, bytes written were=%d (errno=%d, %s)..\n", length, bytesWritten, errno, strerror(errno));
			}
			WAL_FREE(resValue);
		}

		size1 = cnt - position + 2;// 1 for the inclusive limits and another 1 for the \0
		resValueNew = (char*)malloc(size1);
		substring = substr(str,resValueNew,size1,position);
		length = strlen(substring);
		bytesWritten=__nopoll_conn_send_common(conn, substring, length, nopoll_true, 0, NOPOLL_CONTINUATION_FRAME);

		if (bytesWritten != length) 
		{
			WalError("Failed to send bytes %d, bytes written were=%d (errno=%d, %s)..\n", length, bytesWritten, errno, strerror(errno));
		}
		WAL_FREE(resValueNew);
	}
	else 
	{
		bytesWritten=nopoll_conn_send_binary(conn, str, len);
		WalPrint("Number of bytes written: %d\n", bytesWritten);
		bytesWritten = nopoll_conn_flush_writes(conn,flush,bytesWritten);
		if (bytesWritten != len) 
		{
			WalError("Failed to send bytes %d, bytes written were=%d (errno=%d, %s)..\n", len, bytesWritten, errno, strerror(errno));
		}
	}
}

char * getSyncParam(SYNC_PARAM param)
{
	int paramCount=0;
	int *count=0;
	const char *getParamList[1];

	switch(param)
	{
		case SYNC_PARAM_CID:
			getParamList[0] = XPC_SYNC_PARAM_CID;
			break;

		case SYNC_PARAM_CMC:
			getParamList[0] = XPC_SYNC_PARAM_CMC;
			break;

		case SYNC_PARAM_SPV:
			getParamList[0] = XPC_SYNC_PARAM_SPV;
			break;
		
		case SYNC_PARAM_HOSTS_VERSION:
			getParamList[0] = WEBPA_PARAM_HOSTS_VERSION;
			break;
		
		case SYNC_PARAM_SYSTEM_TIME:
			getParamList[0] = WEBPA_PARAM_SYSTEM_TIME;
			break;
		
		case SYNC_PARAM_FIRMWARE_VERSION:
			getParamList[0] = WEBPA_FIRMWARE_VERSION;
			break;
			
		case SYNC_PARAM_DEVICE_UP_TIME:
			getParamList[0] = WEBPA_DEVICE_UP_TIME;
			break;
		
		case SYNC_PARAM_MANUFACTURER:
			getParamList[0] = WEBPA_MANUFACTURER;
			break;
			
		case SYNC_PARAM_RECONNECT_REASON:
			getParamList[0] = WEBPA_RECONNECT_REASON;
			break;
			
		case SYNC_PARAM_REBOOT_REASON:
			getParamList[0] = WEBPA_REBOOT_REASON;
			break;
	
		case SYNC_PARAM_MODEL_NAME:
			getParamList[0] = WEBPA_MODEL_NAME;
			break;

		default:
			WalError("Invalid sync param: %d\n", param);
			return NULL;
	}

	char *paramValue = (char *) malloc(sizeof(char)*64);
	paramCount = sizeof(getParamList)/sizeof(getParamList[0]);
	WAL_STATUS *ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);
	count = (int *) malloc(sizeof(int) * paramCount);
	ParamVal ***parametervalArr = (ParamVal ***) malloc(sizeof(ParamVal **) * paramCount);
	getValues(getParamList, paramCount, NULL,parametervalArr, count, ret);

	if (ret[0] == WAL_SUCCESS )
	{
		walStrncpy(paramValue, parametervalArr[0][0]->value,64);
		WAL_FREE(parametervalArr[0][0]->name);
		WAL_FREE(parametervalArr[0][0]->value);
		WAL_FREE(parametervalArr[0][0]);
		WAL_FREE(parametervalArr[0]);
	}
	else
	{
		WalError("Failed to GetValue for %s\n", getParamList[0]);
		WAL_FREE(paramValue);
	}
	
	WAL_FREE(parametervalArr);
	WAL_FREE(ret);
	WAL_FREE(count);

	return paramValue;
}

WAL_STATUS setSyncParam(SYNC_PARAM param, char* value)
{
	char paramValue[64]={'\0'};
	int paramCount = 1;
	WAL_STATUS status = WAL_FAILURE;
	ParamVal *parametervalArr = (ParamVal *) malloc(sizeof(ParamVal) * paramCount);
	WAL_STATUS *ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);
	ret[0] = -1; // Initialize return to -1
	bool error = false;

	switch(param)
	{
		case SYNC_PARAM_CID:
			parametervalArr[0].name = (char *) XPC_SYNC_PARAM_CID;
			parametervalArr[0].type = WAL_STRING;
			break;

		case SYNC_PARAM_CMC:
			parametervalArr[0].name = (char *) XPC_SYNC_PARAM_CMC;
			parametervalArr[0].type = WAL_UINT;
			break;

		case SYNC_PARAM_SPV:
			parametervalArr[0].name = (char *) XPC_SYNC_PARAM_SPV;
			parametervalArr[0].type = WAL_STRING;
			break;
			
		case SYNC_PARAM_RECONNECT_REASON:
			parametervalArr[0].name = (char *) WEBPA_RECONNECT_REASON;
			parametervalArr[0].type = WAL_STRING;
			break;
			
		case SYNC_PARAM_REBOOT_REASON:
			parametervalArr[0].name = (char *) WEBPA_REBOOT_REASON;
			parametervalArr[0].type = WAL_STRING;
			break;
	
		default:
			WalError("Invalid sync param: %d\n", param);
			error = true;
	}

	if(error == false)
	{
		walStrncpy(paramValue, value,sizeof(paramValue));
		parametervalArr[0].value = paramValue;
		
		setValues(parametervalArr, paramCount, WEBPA_SET, NULL, ret,NULL);
		status = ret[0];
		if (status == WAL_SUCCESS)
		{
			WalPrint("Successfully SetValue for %s\n", parametervalArr[0].name);
		}
		else
		{
			WalError("Failed to SetValue for %s\n", parametervalArr[0].name);
		}
	}

	WAL_FREE(parametervalArr);
	WAL_FREE(ret);

	return status;
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/*
* @brief returns substring value.
* @param[in] char s string to be sliced.
* @param[in] end value of string to be sliced .
* @param[in] start value of string .
* @param[out] return substring value.
*/
static char * substr(char *s, char* resValue, int end,int start)
{
	strncpy(resValue,s+start, end-1);
	resValue[end-1]='\0';
	return resValue;
}

/*
* @brief maps WAL status to "status" message string sent in the response.
* @param[in] status ccsp status.
* @param[out] result wal status.
*/
static void mapWalStatusToStatusMessage(WAL_STATUS status, char *result) 
{
	if (status == WAL_SUCCESS) 
	{ 
		strcpy(result,"Success");
	} 
	else if (status == WAL_ERR_INVALID_PARAMETER_NAME) 
	{
		strcpy(result, "Invalid parameter name");
	} 
	else if (status == WAL_ERR_INVALID_PARAMETER_TYPE) 
	{
		strcpy(result,"Invalid parameter type");
	}
	else if (status == WAL_ERR_INVALID_PARAMETER_VALUE) 
	{
		strcpy(result,"Invalid parameter value");
	} 
	else if (status == WAL_ERR_NOT_WRITABLE) 
	{
		strcpy(result,"Parameter is not writable");
	}
	else if (status == WAL_ERR_NOT_EXIST) 
	{
		strcpy(result,"Parameter does not exist");
	} 
	else if (status == WAL_FAILURE) 
	{
		strcpy(result,"Failure");
	} 
	else if (status == WAL_ERR_TIMEOUT) 
	{
		strcpy(result,"Error Timeout");
	}
	else if (status == WAL_ERR_SETATTRIBUTE_REJECTED) 
	{
		strcpy(result,"SetAttribute rejected");
	} 
	else if (status == WAL_ERR_NAMESPACE_OVERLAP) 
	{
		strcpy(result,"Error namespace overlap");
	} 
	else if (status == WAL_ERR_UNKNOWN_COMPONENT) 
	{
		strcpy(result,"Error unkown component");
	} 
	else if (status == WAL_ERR_NAMESPACE_MISMATCH) 
	{
		strcpy(result,"Error namespace mismatch");
	} 
	else if (status == WAL_ERR_UNSUPPORTED_NAMESPACE) 
	{
		strcpy(result,"Error unsupported namespace");
	} 
	else if (status == WAL_ERR_DP_COMPONENT_VERSION_MISMATCH) 
	{
		strcpy(result,"Error component version mismatch");
	} 
	else if (status == WAL_ERR_INVALID_PARAM) 
	{
		strcpy(result,"Invalid Param");
	}
	else if (status == WAL_ERR_UNSUPPORTED_DATATYPE) 
	{
		strcpy(result,"Unsupported datatype");
	}
	else if (status == WAL_ERR_WIFI_BUSY) 
	{
		strcpy(result,"WiFi is busy");
	}
	else 
	{
		strcpy(result,"Unknown Error");
	}
}

/**
 * @brief Constructs the payload response in JSON format.
 * @param[in] paramName List of Parameters.
 * @param[in] paramValArr Two dimentional array of parameter name/value pairs.
 * @param[in] attArr Array of attributes
 * @param[in] paramCount Number of parameters.
 * @param[in] count total number of parameters.
 * @param[out] parameters Returns response in JSON format.
 * @return WAL_STATUS
*/
static void constructPayloadJsonResponse(const char * paramName[],
	ParamVal ***parametervalArr, int paramCount, int *count,
	WAL_STATUS *ret, cJSON *parameters) 
{
	int cnt;
	cJSON *paramObj;
	char *result = (char *) malloc(sizeof(char) * 100);
	for (cnt = 0; cnt < paramCount; cnt++) 
	{
		WAL_STATUS status = ret[cnt];
		WalPrint("count[%d]: %d\n",cnt,count[cnt]);
		
		cJSON_AddItemToArray(parameters, paramObj = cJSON_CreateObject());
		WalPrint("parametervalArr[0][%d]->name : %s\n",cnt,parametervalArr[0][cnt]->name);
		cJSON_AddStringToObject(paramObj, "name", parametervalArr[0][cnt]->name);
		
		if(parametervalArr[0][cnt]->value == NULL)
		{
			cJSON_AddStringToObject(paramObj, "value","ERROR");
		}
		else
		{
			cJSON_AddStringToObject(paramObj, "value", parametervalArr[0][cnt]->value);
			WalPrint("parametervalArr[0][%d]->value : %s\n",cnt,parametervalArr[0][cnt]->value);
		}
		WalPrint("parametervalArr[0][%d]->type : %d\n",cnt,parametervalArr[0][cnt]->type);
		cJSON_AddNumberToObject(paramObj, "dataType", parametervalArr[0][cnt]->type);
		cJSON_AddNumberToObject(paramObj, "parameterCount", count[cnt]);
		mapWalStatusToStatusMessage(status, result);
		cJSON_AddStringToObject(paramObj, "message", result);
		
		WAL_FREE(parametervalArr[0][cnt]->name);		
		WAL_FREE(parametervalArr[0][cnt]->value);
		WAL_FREE(parametervalArr[0][cnt]);
	}
	
	WAL_FREE(result);
	WAL_FREE(parametervalArr[0]);
}

/*
* @brief Returns checking Status from the stack.
* @param[in] paramCount number of parameters.
* @param[in] ret1 Return status of getValues.
* @param[in] ret2 Return status of getAttributes.
* @param[out] ret Final return Status from the stack.
*/
static void checkReturnStatus(int paramCount, WAL_STATUS *ret1, WAL_STATUS *ret2, WAL_STATUS *ret)
{
	int i;
	for (i = 0; i < paramCount; i++) 
	{
		WalPrint("ret1[%d] = %d\n",i,ret1[i]);
		WalPrint("ret2[%d] = %d\n",i,ret2[i]);
		if (ret1[i] == WAL_SUCCESS && ((ret2[i] != WAL_SUCCESS) && (ret2[i] != -1))) 
		{
			ret[i] = ret2[i];
		}
		else if (ret2[i] == WAL_SUCCESS && ((ret1[i] != WAL_SUCCESS) && (ret1[i] != -1))) 
		{
			ret[i] = ret1[i];
		}
		else if ((ret2[i] != -1) && (ret1[i] == -1)) 
		{
			ret[i] = ret2[i];
		}
		else 
		{
			ret[i] = ret1[i];
		}
		WalPrint("ret[%d] = %d\n",i,ret[i]);
	}
}

/*
* @ getStatusCode brief adds status code for return value from stack
* @param[in] statusCode status code value
* @param[in] paramCount Number of parameters
* @param[in] ret return Status from the stack
*/
static void getStatusCode(WEBPA_RESPONSE_STATUS_CODE *statusCode, int paramCount, WAL_STATUS * ret)
{
	int i =0;
	for (i = 0; i < paramCount; i++) 
	{
		WalPrint("ret[%d] = %d\n",i,ret[i]);
		if (ret[i] == WAL_SUCCESS) 
		{
			*statusCode = WEBPA_STATUS_SUCCESS;
		}
		else if (ret[i] == WAL_ERR_WIFI_BUSY)
		{
			*statusCode = WEBPA_STATUS_WIFI_BUSY;
			break;
		}
		else 
		{
			*statusCode = WEBPA_STATUS_GENERAL_FALURE;
			break;
		}
	}
	WalPrint("statusCode = %d\n",*statusCode);
}

/**
* @brief walStrncpy WAL String copy function that copies the content of source string into destination string 
* and null terminates the destination string
*
* @param[in] destStr Destination String
* @param[in] srcStr Source String
* @param[in] destSize size of destination string
*/
void walStrncpy(char *destStr, const char *srcStr, size_t destSize)
{
    strncpy(destStr, srcStr, destSize-1);
    destStr[destSize-1] = '\0';
}

void processSetAttrRequest(cJSON* request, money_trace_spans *timeSpan, cJSON* response) 
{
	cJSON *reqParamObj, *parameters, *resParamObj, *attributes;
	cJSON *paramArray = cJSON_GetObjectItem(request, "parameters");
	int i,freeCount=0;
	WEBPA_RESPONSE_STATUS_CODE statusCode = WEBPA_STATUS_GENERAL_FALURE;
	char message[MAX_RESULT_STR_LENGTH] ; //={'\0'};
	walStrncpy(message,"Failure",sizeof(message));
	int paramCount = cJSON_GetArraySize(paramArray);
	char *getParamList[MAX_PARAMETER_LENGTH],*result;
	int notification = 0,flag=0;
	char notif[20] = "";
	AttrVal **attArr = NULL;
	attArr = (AttrVal **) malloc(sizeof(AttrVal *) * paramCount);
	WAL_STATUS *ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);
	for (i = 0; i < paramCount; i++)
	{
		freeCount++;
		WalPrint("freeCount : %d\n",freeCount);
		reqParamObj = cJSON_GetArrayItem(paramArray, i);
		attArr[i] = (AttrVal *) malloc(sizeof(AttrVal ) * 1);
		attArr[i]->name = cJSON_GetObjectItem(reqParamObj, "name")->valuestring;
		// If input parameter is wildcard ending with "." then send error as wildcard is not supported for SET_ATTRIBUTES
		if(attArr[i]->name[(strlen(attArr[i]->name)-1)] == '.')
		{
			flag = 1;			
			walStrncpy(message, "Wildcard SET_ATTRIBUTES is not supported",sizeof(message));
		   	break;
		}		
		getParamList[i] = attArr[i]->name;
		if (cJSON_GetObjectItem(reqParamObj, "attributes") == NULL) 
		{
			flag = 1;
			walStrncpy(message, "attributes is null",sizeof(message));
		   	break;
		} 
		else 
		{
			attributes = cJSON_GetObjectItem(reqParamObj, "attributes");
			if(cJSON_GetObjectItem(attributes, "notify") != NULL) 
			{
				notification = cJSON_GetObjectItem(attributes, "notify")->valueint;
				WalPrint("notification :%d\n",notification);
				snprintf(notif, sizeof(notif), "%d", notification);
				attArr[i]->value = (char *) malloc(sizeof(char) * 20);
				walStrncpy(attArr[i]->value, notif,(sizeof(char) * 20));
				WalPrint("attArr[i]->value: %s\n",attArr[i]->value);
			}
			else 
			{
				flag = 1;			
				walStrncpy(message, "notify is null",sizeof(message));
			   	break;
			}
						
		}
		
		attArr[i]->type = WAL_INT;
	}
	
	if(flag != 1)
	{
		// Initialize response
		cJSON_AddItemToObject(response, WRP_MSG_PAYLOAD_PARAMETERS, parameters =cJSON_CreateArray());
		setAttributes(getParamList, paramCount, timeSpan,attArr, ret);
		getStatusCode(&statusCode, paramCount, ret);

		cJSON_AddNumberToObject(response, "statusCode", statusCode);

		result = (char *) malloc(sizeof(char) * MAX_RESULT_STR_LENGTH);
		for (i = 0; i < paramCount; i++) 
		{
			WAL_STATUS status = ret[i];
			cJSON_AddItemToArray(parameters, resParamObj = cJSON_CreateObject());
			cJSON_AddStringToObject(resParamObj, "name", getParamList[i]);
			mapWalStatusToStatusMessage(status, result);
			cJSON_AddStringToObject(resParamObj, "message", result);
			WAL_FREE(attArr[i]->value);
		}
		WAL_FREE(result); 
	}	
	else
	{	
		cJSON_AddNumberToObject(response, "statusCode", WEBPA_STATUS_ATOMIC_GET_SET_FAILED);
		cJSON_AddStringToObject(response, "message", message);
	}
	
	for(i = 0; i < freeCount; i++)
	{		
		WAL_FREE(attArr[i]);	
	}
	
	WAL_FREE(ret);
	WAL_FREE(attArr);
}

