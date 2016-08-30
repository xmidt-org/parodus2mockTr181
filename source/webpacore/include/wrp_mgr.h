/**
 * @file wrp_mgr.h
 *
 * @description This header defines the WRP APIs to handle request response mechanisms
 *
 * Copyright (c) 2015  Comcast
 */
 
#ifndef WRP_MGR_H_
#define WRP_MGR_H_
#include <stdbool.h>
#include "wal.h"

typedef enum
{
	SYNC_PARAM_CID = 0,
	SYNC_PARAM_CMC,
	SYNC_PARAM_FIRMWARE_VERSION,
	SYNC_PARAM_DEVICE_UP_TIME,
	SYNC_PARAM_MANUFACTURER,
	SYNC_PARAM_MODEL_NAME,
	SYNC_PARAM_SPV,
	SYNC_PARAM_HOSTS_VERSION,
	SYNC_PARAM_SYSTEM_TIME,
	SYNC_PARAM_RECONNECT_REASON,
	SYNC_PARAM_REBOOT_REASON
} SYNC_PARAM;

/**
 * @brief processGetRequest interface handles GET parameter requests.
 *returns the parameter values from stack and forms the response 
 *
 * @param[in] request cJSON GET parameter request.
 * @param[out] timeSpan timing_values for each component.
 * @param[out] response Returns GET response as CSJON Object.
 */
void processGetRequest(cJSON* request, money_trace_spans *timeSpan, cJSON* response);

/**
 * @brief processGetAttrRequest interface handles GET attribute requests.
 *returns the attribute values for parameters from stack and forms the response 
 *
 * @param[in] request cJSON GET attribute request.
 * @param[out] timeSpan timing_values for each component.
 * @param[out] response Returns GET attribute response as CSJON Object.
 */
void processGetAttrRequest(cJSON* request, money_trace_spans *timeSpan, cJSON* response);

/**
 * @brief processSetRequest interface handles SET parameter requests in both atomic and non-atomic way.
 * Sets the parameter values and attributes to stack and forms the response 
 *
 * @param[in] request cJSON SET parameter request.
 * @param[in] setType Flag to specify if the type of set operation.
 * @param[out] timeSpan timing_values for each component.
 * @param[out] response Returns SET response as CSJON Object.
 */
void processSetRequest(cJSON* request, WEBPA_SET_TYPE setType, money_trace_spans *timeSpan, cJSON* response,char * transaction_id);

/**
 * @brief processSetAttrRequest interface handles SET attribute requests in both atomic and non-atomic way.
 * Sets the parameter values and attributes to stack and forms the response 
 *
 * @param[in] request cJSON SET attribute request.
 * @param[out] timeSpan timing_values for each component.
 * @param[out] response Returns SET attribute response as CSJON Object.
 */
void processSetAttrRequest(cJSON* request, money_trace_spans *timeSpan, cJSON* response);

/**
 * @brief processTestandSetRequest interface handles Test and SET parameter requests. 
 * Checks cid values and then does SET in atomic way. 
 * Sets the parameter values to stack and forms the payload JSON response.
 *
 * @param[in] request cJSON SET parameter request.
 * @param[out] response Returns response as CSJON Object.
 */
void processTestandSetRequest(cJSON *request, money_trace_spans *timeSpan, cJSON *response,char * transaction_id);

/**
 * @brief processIoTRequest interface handles IoT requests. 
 *
 * @param[in] request cJSON SET parameter request.
 * @param[out] response Returns response as CSJON Object.
 */
void processIoTRequest(cJSON *request, cJSON *response);

/**
 * @brief sendResponse Allows to send a message over the provided connection with the provided length.
 * Forms the frgaments if the content length is more than the allowable nopoll conn object to handle 
 * and sends the continuation fragments 
 *
 * @param[in] conn The connection where the message will be sent.
 * @param[in] str The content to be sent
 * @param[in] bufferSize The lenght of the content to be sent
 */
void * sendResponse(noPollConn * conn,char *str, int bufferSize);

/**
 * @brief getSyncParam interface handles GET parameter requests for XPC Sync parameters.
 * Returns the XPC sync parameter value (CMC,CID or SPV) from stack
 *
 * @param[in] param SYNC_PARAM CID, CMC or SPV
 * @return char* XPC Sync parameter value
 */
char * getSyncParam(SYNC_PARAM param);

/**
 * @brief setSyncParam interface handles SET parameter requests for XPC Sync parameters in non-atomic way.
 * Sets the XPC sync parameter value to stack and returns the status
 *
 * @param[in] param SYNC_PARAM CID, CMC or SPV
 * @param[in] value XPC parameter value string
 * @return WAL_STATUS success or failure status
 */
WAL_STATUS setSyncParam(SYNC_PARAM param, char* value);


#endif /* WRP_MGR_H_ */
