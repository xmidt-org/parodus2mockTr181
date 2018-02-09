/**
 * Copyright 2018 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//#include <stdbool.h>
//#include <stdint.h>

#include "mock_tr181_client.h"
#include "mock_tr181_adapter.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
// Limit for total number of wild card matched params in the Response
#define MAX_WILDCARD_PARAMS 100

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
libpd_instance_t mock_tr181_instance;

typedef struct param_t_list
{
	param_t p;
	struct param_t_list* next;
}_param_t_list;

/*----------------------------------------------------------------------------*/
/*                             Internal Functions                             */
/*----------------------------------------------------------------------------*/
void *parodus_receive_wait();

static void free_param_t_list(struct param_t_list* pList)
{
	while (pList)
	{
		struct param_t_list* tmp = pList;
		pList = pList->next;
		free(tmp);
	}
}

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

// Required by Cimplog
const char *rdk_logger_module_fetch(void)
{
    return "LOG.RDK.MOCK_TR181";
}


void connect_parodus()
{

    libpd_cfg_t cfg = { .service_name = "config",
                        .receive = true,
                        .keepalive_timeout_secs = 64,
                        .parodus_url = "tcp://127.0.0.1:6666",
                        .client_url = "tcp://127.0.0.1:6663"
                      };

    Info("Configurations => service_name : %s parodus_url : %s client_url : %s\n", cfg.service_name, cfg.parodus_url, cfg.client_url );

    while(1)
    {
        int ret = libparodus_init (&mock_tr181_instance, &cfg);

        if(ret ==0)
        {
            Info("Init for parodus Success..!!\n");
            break;
        }
        else
        {
            Error("Init for parodus failed: '%s'\n",libparodus_strerror(ret));
            sleep(5);
        }
	    libparodus_shutdown(&mock_tr181_instance);

    }
}


void startParodusReceiveThread()
{
    int err = 0;
    pthread_t threadId;

    err = pthread_create(&threadId, NULL, parodus_receive_wait, NULL);
    if (err != 0)
    {
        Error("Error creating thread :[%s]\n", strerror(err));
        exit(1);
    }
    else
    {
        Info("Parodus Receive wait thread created Successfully %d\n", (int ) threadId);
    }
}



/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static int isWildcardQuery(char *req)
{
	// TR181 - wildcard queries are Object based queries, should end in dot [.] char
	if(req)
	{
		int len = strlen(req);
		if(len > 0 && req[len-1] == '.')
			return 1;
	}

	return 0;
}


static void processRequest(char *reqPayload, char **resPayload)
{
    req_struct *reqObj = NULL;
    res_struct *resObj = NULL;
    cJSON *obj = NULL;
    char *payload = NULL;
    int reqParamCount = 0, resParamCount = 0, i = 0, matchFlag = 0, count = 0, j = 0, k = 0;
    WDMP_STATUS ret = WDMP_SUCCESS;

    /* Parse Request Payload.
     * Convert JSON Payload into struct req_struct
     */
    wdmp_parse_request(reqPayload, &reqObj);
    cJSON *paramList = NULL;
    char *dbData = NULL, *addData = NULL;

    /* Read TR181 DB file.
     * get all the TR181 params from db file.
     * Note: db in json format
     */
    int status = readFromDB(&dbData);
    if(status == 1)
    {
        Info("Data from DB: %s\n", dbData);
        paramList = cJSON_Parse(dbData); //paramList contains TR181 db
    }
    else
    {
        Error("Failed to read from DB\n");
        paramList = cJSON_CreateArray();
    }

    /* Create res_struct
     *   - allocate res_struct. gets tricky for wildcard Req, since response size is variable.
     *   - populate values for each req param
     *   - set paramCnt and retParamCnt accordingly
     */
    if(reqObj != NULL)
    {
        Print("Request:> Type : %d\n",reqObj->reqType);

        // allocate response struct
        resObj = (res_struct *) malloc(sizeof(res_struct));
        if(resObj)
        {
        		memset(resObj, 0, sizeof(res_struct));
        }
        else
        	{
        		Error("Malloc Failed!\n");
        		return;
        	}

        // Res Type same as Req Type
        resObj->reqType = reqObj->reqType;
        Print("Response:> type = %d\n", resObj->reqType);

        // Create Res for GET
        if(reqObj->reqType == GET)
        {
        		//Response paramCnt is same as Req paramCnt
        		// Note : for wildcard, the retParamCnt for each param differs.
        		//  Param here is loosly used to refer both tr181 pamameter and object
        		Print("Request:> ParamCount = %zu\n",reqObj->u.getReq->paramCnt);
        		resParamCount = resObj->paramCnt = reqObj->u.getReq->paramCnt;
        		Print("Response:> paramCnt = %zu\n", resObj->paramCnt);

        		// allocate retStatus to point to array of WDMP_STATUS
            resObj->retStatus = (WDMP_STATUS*) malloc(sizeof(WDMP_STATUS) * resParamCount);
            if(!resObj->retStatus)
            {
            		Error("Malloc Failed!\n");
            		return;
            }

            resObj->timeSpan = NULL;

            //Ok. We took care of res_struct. Now allocate the union within the struct
            resObj->u.getRes = (get_res_t *) malloc(sizeof(get_res_t));
            if(resObj->u.getRes)
            {
            		memset(resObj->u.getRes, 0, sizeof(get_res_t));
            }
            else
            {
            		Error("Malloc Failed!\n");
            		return;
            }

            /* below code populates each member within the get_res_t struct referred by getRes union */

            // paramCnt is same as the number of params/objects in Request
            resObj->u.getRes->paramCnt = resParamCount;

            // allocate paramNames to hold as many pointers as the names of params/objects in the Request
            resObj->u.getRes->paramNames = (char **) malloc(sizeof(char*) * resParamCount);

            // allocate retParamCnt to hold as many values as the names of params/objects in the Request
            // Now, the value of retParamCnt will vary depending on parameter query or wildcard query.
            //      if paramNames[i] is a parameter, then retParamCnt is 1
            //      if it is a wildcard (object query), then the retParamCnt will
            //          hold the size of the total number of parameters found for that wildcard query.
            resObj->u.getRes->retParamCnt = (size_t *) malloc(sizeof(size_t) * resParamCount);

            // allocate params (array of params pointers)
            //   this holds as many pointers as the names of params/objects in the Request
            resObj->u.getRes->params = (param_t **) malloc(sizeof(param_t*) * resParamCount);
            memset(resObj->u.getRes->params, 0, sizeof(param_t*) * resParamCount);

            /* go through each Req param/obj(wildcard) and look for matches in db */
            reqParamCount = (int)reqObj->u.getReq->paramCnt;

            for(i = 0; i < reqParamCount; i++)
            {
            		int retParamCnt = 0;

            		if(isWildcardQuery(reqObj->u.getReq->paramNames[i]))
            		{
            			/* Wildcard query */
            			/* look for all params matching the wildcard */
            			struct param_t_list* pList = NULL, *pLast = NULL;

            			count = cJSON_GetArraySize(paramList);
					for (j = 0; j < count; j++)
					{
						obj = cJSON_GetArrayItem(paramList, j);
						if (  (NULL != obj) && (cJSON_GetObjectItem(obj, "name") != NULL) )
						{
							if (strncmp(reqObj->u.getReq->paramNames[i], cJSON_GetObjectItem(obj, "name")->valuestring, strlen(reqObj->u.getReq->paramNames[i])) == 0)
							{
								retParamCnt++;
								//add to a tmp list
								struct param_t_list *tmp = (struct param_t_list*)malloc(sizeof(struct param_t_list));
								if(tmp)
								{
									tmp->p.name = strdup(cJSON_GetObjectItem(obj, "name")->valuestring);
									tmp->p.value = strdup(cJSON_GetObjectItem(obj, "value")->valuestring);
									tmp->p.type = cJSON_GetObjectItem(obj, "type")->valueint;
									tmp->next = NULL;
									//add to list
									if(pList)
									{
										pLast->next = tmp;
										pLast = tmp;
									}
									else
									{
										pList = pLast = tmp;
									}
								}
								else
								{
				            			Error("Malloc Failed!\n");
				            			return;
								}
							}
						}
					}

					if (retParamCnt)
					{
						resObj->u.getRes->paramNames[i] = reqObj->u.getReq->paramNames[i];
						Print("Response:> paramNames[%d] = %s\n", i, resObj->u.getRes->paramNames[i]);

						resObj->u.getRes->retParamCnt[i] = retParamCnt;
						Print("Response:> retParamCnt[%d] = %zu\n", i, resObj->u.getRes->retParamCnt[i]);

						resObj->u.getRes->params[i] = (param_t *) malloc(sizeof(param_t) * retParamCnt);
						struct param_t_list* pl = pList;
						for(k = 0; k < retParamCnt && pList != NULL; k++, pl = pl->next)
						{
							resObj->u.getRes->params[i][k].name = strdup(pl->p.name);
							resObj->u.getRes->params[i][k].value = strdup(pl->p.value);
							resObj->u.getRes->params[i][k].type = pl->p.type;
						}
						free_param_t_list(pList);

						resObj->retStatus[i] = ret;
						Print("Response:> retStatus[%d] = %d\n", i, resObj->retStatus[i]);
					}
					else
					{
						resObj->retStatus[i] = WDMP_ERR_INVALID_PARAMETER_NAME;
						Print("Response:> retStatus[%d] = %d\n", i, resObj->retStatus[i]);
					}

            		}
            		else /* Query for individual parameter. Not wildcard */
            		{
            			/* query for individual parameter. Stop at first find. */

 					count = cJSON_GetArraySize(paramList);
           			// look through each param for the DB for a match. Stop at first match
					for (j = 0; j < count; j++)
					{
						obj = cJSON_GetArrayItem(paramList, j);
						if (  (NULL != obj)
							  && (cJSON_GetObjectItem(obj, "name") != NULL)
							  && (strcmp(reqObj->u.getReq->paramNames[i], cJSON_GetObjectItem(obj, "name")->valuestring) == 0)  )
						{
							resObj->u.getRes->params[i] = (param_t *) malloc(sizeof(param_t));
							resObj->u.getRes->params[i][0].name = strdup(cJSON_GetObjectItem(obj, "name")->valuestring);
							resObj->u.getRes->params[i][0].value = strdup(cJSON_GetObjectItem(obj, "value")->valuestring);
							resObj->u.getRes->params[i][0].type = cJSON_GetObjectItem(obj, "type")->valueint;
							retParamCnt = 1;
							break;
						}
					}

					if (retParamCnt)
					{
						resObj->u.getRes->paramNames[i] = reqObj->u.getReq->paramNames[i];
						Print("Response:> paramNames[%d] = %s\n", i, resObj->u.getRes->paramNames[i]);

						resObj->u.getRes->retParamCnt[i] = 1;
						Print("Response:> retParamCnt[%d] = %zu\n", i, resObj->u.getRes->retParamCnt[i]);

						resObj->retStatus[i] = ret;
						Print("Response:> retStatus[%d] = %d\n", i, resObj->retStatus[i]);
					}
					else
					{
						resObj->retStatus[i] = WDMP_ERR_INVALID_PARAMETER_NAME;
						Print("Response:> retStatus[%d] = %d\n", i, resObj->retStatus[i]);
					}
				}

            }

        }
        else if(reqObj->reqType == GET_ATTRIBUTES)
        {
            Print("Request:> ParamCount = %zu\n",reqObj->u.getReq->paramCnt);
            resObj->paramCnt = reqObj->u.getReq->paramCnt;
            Print("Response:> paramCnt = %zu\n", resObj->paramCnt);
            resObj->retStatus = (WDMP_STATUS *) malloc(sizeof(WDMP_STATUS)*resObj->paramCnt);
            resObj->timeSpan = NULL;
            reqParamCount = (int)reqObj->u.getReq->paramCnt;
            resObj->u.paramRes = (param_res_t *) malloc(sizeof(param_res_t));
            memset(resObj->u.paramRes, 0, sizeof(param_res_t));

            resObj->u.paramRes->params = (param_t *) malloc(sizeof(param_t)*reqParamCount);
            memset(resObj->u.paramRes->params, 0, sizeof(param_t)*reqParamCount);

            for (i = 0; i < reqParamCount; i++)
            {
                count = cJSON_GetArraySize(paramList);
                for( j = 0; j < count; j++)
                {
                    obj = cJSON_GetArrayItem(paramList, j);
                    if((strcmp(reqObj->u.getReq->paramNames[i], cJSON_GetObjectItem(obj, "name")->valuestring) == 0) && (cJSON_GetObjectItem(obj, "notify") != NULL))
                    {
                        resObj->u.paramRes->params[i].name = (char*) malloc (sizeof(char)*100);
                        resObj->u.paramRes->params[i].value = (char*) malloc (sizeof(char)*100);

                        strcpy(resObj->u.paramRes->params[i].name, cJSON_GetObjectItem(obj, "name")->valuestring);
                        Print("Response:> params[%d].name = %s\n",i,resObj->u.paramRes->params[i].name);
                        strcpy(resObj->u.paramRes->params[i].value, cJSON_GetObjectItem(obj, "notify")->valuestring);
                        Print("Response:> params[%d].value = %s\n",i,resObj->u.paramRes->params[i].value);
                        resObj->u.paramRes->params[i].type = WDMP_INT;
                        Print("Response:> params[%d].type = %d\n",i,resObj->u.paramRes->params[i].type);
                        matchFlag = 1;
                        break;
                    }
                    else
                    {
                        matchFlag = 0;
                    }
                }

                if(matchFlag)
                {
                    resObj->retStatus[i] = ret;
                }
                else
                {
                    resObj->retStatus[i] = WDMP_ERR_INVALID_PARAMETER_NAME;
                }

                Print("Response:> retStatus[%d] = %d\n",i,resObj->retStatus[i]);
            }
        }
        else if((reqObj->reqType == SET) || (reqObj->reqType == SET_ATTRIBUTES))
        {
            Print("Request:> ParamCount = %zu\n",reqObj->u.setReq->paramCnt);
            resObj->paramCnt = reqObj->u.setReq->paramCnt;
            Print("Response:> paramCnt = %zu\n", resObj->paramCnt);
            resObj->retStatus = (WDMP_STATUS *) malloc(sizeof(WDMP_STATUS)*resObj->paramCnt);
            resObj->timeSpan = NULL;
            reqParamCount = (int)reqObj->u.setReq->paramCnt;
            resObj->u.paramRes = (param_res_t *) malloc(sizeof(param_res_t));
            memset(resObj->u.paramRes, 0, sizeof(param_res_t));
            resObj->u.paramRes->params = (param_t *) malloc(sizeof(param_t)*reqParamCount);
            memset(resObj->u.paramRes->params, 0, sizeof(param_t)*reqParamCount);

            for (i = 0; i < reqParamCount; i++)
            {
                Print("Request:> param[%d].name = %s\n",i,reqObj->u.setReq->param[i].name);
                Print("Request:> param[%d].value = %s\n",i,reqObj->u.setReq->param[i].value);
                Print("Request:> param[%d].type = %d\n",i,reqObj->u.setReq->param[i].type);

                cJSON_AddItemToArray(paramList, obj = cJSON_CreateObject());
                cJSON_AddStringToObject(obj, "name", reqObj->u.setReq->param[i].name);
                if(reqObj->reqType == SET)
                {
                    cJSON_AddStringToObject(obj, "value", reqObj->u.setReq->param[i].value);
                    cJSON_AddNumberToObject(obj, "type", reqObj->u.setReq->param[i].type);
                }
                else
                {
                    cJSON_AddStringToObject(obj, "notify", reqObj->u.setReq->param[i].value);
                }

                resObj->u.paramRes->params[i].name = (char *) malloc(sizeof(char) * 512);
                strcpy(resObj->u.paramRes->params[i].name, reqObj->u.setReq->param[i].name);
                Print("Response:> params[%d].name = %s\n",i,resObj->u.paramRes->params[i].name);
                resObj->u.paramRes->params[i].value = NULL;
                resObj->u.paramRes->params[i].type = 0;

                resObj->retStatus[i] = ret;
                Print("Response:> retStatus[%d] = %d\n",i,resObj->retStatus[i]);
            }

            addData = cJSON_Print(paramList);
            Print("addData : %s\n",addData);
            status = writeToDB(addData);
            if(status == 1)
            {
                Info("Data is successfully added to DB\n");
            }
            else
            {
                Error("Failed to add data to DB\n");
            }
        }
    }

    wdmp_form_response(resObj,&payload);
    Print("payload : %s\n",payload);
    *resPayload = payload;

    Info("Response:> Payload = %s\n", *resPayload);

    if(NULL != reqObj)
    {
        wdmp_free_req_struct(reqObj);
    }
    if(NULL != resObj)
    {
        wdmp_free_res_struct(resObj);
    }
}

/*----------------------------------------------------------------------------*/
/*                             Function Callback                              */
/*----------------------------------------------------------------------------*/

void *parodus_receive_wait()
{
    int rtn;
    wrp_msg_t *wrp_msg = NULL;
    wrp_msg_t *res_wrp_msg = NULL;
    char *contentType = NULL;

    while (1)
    {
        rtn = libparodus_receive (mock_tr181_instance, &wrp_msg, 2000);
        if (rtn == 1)
        {
            continue;
        }

        if (rtn != 0)
        {
            Error ("Libparodus failed to recieve message: '%s'\n",libparodus_strerror(rtn));
            sleep(5);
            continue;
        }

        if (wrp_msg && wrp_msg->msg_type == WRP_MSG_TYPE__REQ)
        {
            res_wrp_msg = (wrp_msg_t *)malloc(sizeof(wrp_msg_t));
            if(res_wrp_msg)
            {
            		memset(res_wrp_msg, 0, sizeof(wrp_msg_t));
            }
            else
            {
            		Error("In parodus_receive_wait() - malloc Failed !!!");
            		free(wrp_msg);
            		break;
            }

            Info("Request message : %s\n",(char *)wrp_msg->u.req.payload);

            processRequest((char *)wrp_msg->u.req.payload, ((char **)(&(res_wrp_msg->u.req.payload))));

            Info("Response payload is %s\n",(char *)(res_wrp_msg->u.req.payload));
            res_wrp_msg->u.req.payload_size = strlen(res_wrp_msg->u.req.payload);
            res_wrp_msg->msg_type = wrp_msg->msg_type;
            res_wrp_msg->u.req.source = wrp_msg->u.req.dest;
            res_wrp_msg->u.req.dest = wrp_msg->u.req.source;
            res_wrp_msg->u.req.transaction_uuid = wrp_msg->u.req.transaction_uuid;
            contentType = (char *)malloc(sizeof(char)*(strlen(CONTENT_TYPE_JSON)+1));
            if(contentType)
            {
            		strncpy(contentType,CONTENT_TYPE_JSON,strlen(CONTENT_TYPE_JSON)+1);
            		res_wrp_msg->u.req.content_type = contentType;
            }
            else
			{
				Error("In parodus_receive_wait() - malloc Failed !!!");
				free(wrp_msg);
				free(res_wrp_msg);
				break;
			}

            int sendStatus = libparodus_send(mock_tr181_instance, res_wrp_msg);
            Print("sendStatus is %d\n",sendStatus);
            if(sendStatus == 0)
            {
                Info("Sent message successfully to parodus\n");
            }
            else
            {
                Error("libparodus_send() Failed to send message: '%s'\n",libparodus_strerror(sendStatus));
            }
            wrp_free_struct (res_wrp_msg);
        }
        free(wrp_msg);
    }

    libparodus_shutdown(&mock_tr181_instance);
    Print ("End of parodus_upstream\n");
    return 0;
}




