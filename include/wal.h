/**
 * @file wal.h
 *
 * @description This header defines the WebPA Abstraction APIs
 *
 * Copyright (c) 2015  Comcast
 */

#ifndef _WAL_H_
#define _WAL_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * @brief Platform specific defines.
 */
#define WEBPA_COMPONENT_NAME            getWebPAConfig(WCFG_COMPONENT_NAME)
#define WEBPA_CFG_FILE                  getWebPAConfig(WCFG_CFG_FILE)
#define WEBPA_CFG_FILE_SRC              getWebPAConfig(WCFG_CFG_FILE_SRC)
#define WEBPA_CFG_DEVICE_INTERFACE      getWebPAConfig(WCFG_DEVICE_INTERFACE)
#define WEBPA_DEVICE_MAC                getWebPAConfig(WCFG_DEVICE_MAC)
#define WEBPA_DEVICE_REBOOT_PARAM	getWebPAConfig(WCFG_DEVICE_REBOOT_PARAM)
#define WEBPA_DEVICE_REBOOT_VALUE	getWebPAConfig(WCFG_DEVICE_REBOOT_VALUE)
#define XPC_SYNC_PARAM_CID              getWebPAConfig(WCFG_XPC_SYNC_PARAM_CID)
#define XPC_SYNC_PARAM_CMC              getWebPAConfig(WCFG_XPC_SYNC_PARAM_CMC)
#define WEBPA_FIRMWARE_VERSION		getWebPAConfig(WCFG_FIRMWARE_VERSION)
#define WEBPA_DEVICE_UP_TIME		getWebPAConfig(WCFG_DEVICE_UP_TIME)
#define WEBPA_MANUFACTURER		getWebPAConfig(WCFG_MANUFACTURER)
#define WEBPA_MODEL_NAME		getWebPAConfig(WCFG_MODEL_NAME)
#define XPC_SYNC_PARAM_SPV              getWebPAConfig(WCFG_XPC_SYNC_PARAM_SPV)
#define WEBPA_PARAM_HOSTS_NAME		getWebPAConfig(WCFG_PARAM_HOSTS_NAME)
#define WEBPA_PARAM_HOSTS_VERSION	getWebPAConfig(WCFG_PARAM_HOSTS_VERSION)
#define WEBPA_PARAM_SYSTEM_TIME		getWebPAConfig(WCFG_PARAM_SYSTEM_TIME)
#define WEBPA_RECONNECT_REASON		getWebPAConfig(WCFG_RECONNECT_REASON)
#define WEBPA_REBOOT_REASON		getWebPAConfig(WCFG_REBOOT_REASON)
#define WEBPA_CFG_FILE_NAME             "webpa_cfg.json"
#define WEBPA_CFG_SERVER_IP             "ServerIP"
#define WEBPA_CFG_SERVER_PORT           "ServerPort"
#define WEBPA_CFG_SERVER_SECURE         "Secure"
#define WEBPA_CFG_DEVICE_NW_INTERFACE   "DeviceNetworkInterface"
#define WEBPA_CFG_RETRY_INTERVAL        "RetryIntervalInSec"
#define WEBPA_CFG_PING_WAIT_TIME        "MaxPingWaitTimeInSec"
#define WEBPA_CFG_NOTIFY                "Notify"
#define WEBPA_CFG_FIRMWARE_VER		"oldFirmwareVersion"
#define WAL_FREE(__x__) if(__x__ != NULL) { free((void*)(__x__)); __x__ = NULL;} else {WalError("Trying to free null pointer\n");}

/**
 * @brief Enables or disables debug logs.
 */
#define WEBPA_LOG_ERROR                 0
#define WEBPA_LOG_INFO                  1
#define WEBPA_LOG_PRINT                 2
#define WalError(...)                   _WEBPA_LOG(WEBPA_LOG_ERROR, __VA_ARGS__)
#define WalInfo(...)                    _WEBPA_LOG(WEBPA_LOG_INFO, __VA_ARGS__)
#define WalPrint(...)                   _WEBPA_LOG(WEBPA_LOG_PRINT, __VA_ARGS__)

/**
 * @brief WebPA Data types.
 */
typedef enum
{
    WAL_STRING = 0,
    WAL_INT,
    WAL_UINT,
    WAL_BOOLEAN,
    WAL_DATETIME,
    WAL_BASE64,
    WAL_LONG,
    WAL_ULONG,
    WAL_FLOAT,
    WAL_DOUBLE,
    WAL_BYTE,
    WAL_NONE
} DATA_TYPE;

/**
 * @brief WebPA Error codes.
 */
typedef enum
{
    WAL_SUCCESS = 0,                    /**< Success. */
    WAL_FAILURE,                        /**< General Failure */
    WAL_ERR_TIMEOUT,
    WAL_ERR_NOT_EXIST,
    WAL_ERR_INVALID_PARAMETER_NAME,
    WAL_ERR_INVALID_PARAMETER_TYPE,
    WAL_ERR_INVALID_PARAMETER_VALUE,
    WAL_ERR_NOT_WRITABLE,
    WAL_ERR_SETATTRIBUTE_REJECTED,
    WAL_ERR_NAMESPACE_OVERLAP,
    WAL_ERR_UNKNOWN_COMPONENT,
    WAL_ERR_NAMESPACE_MISMATCH,
    WAL_ERR_UNSUPPORTED_NAMESPACE,
    WAL_ERR_DP_COMPONENT_VERSION_MISMATCH,
    WAL_ERR_INVALID_PARAM,
    WAL_ERR_UNSUPPORTED_DATATYPE,
    WAL_STATUS_RESOURCES,
	WAL_ERR_WIFI_BUSY
} WAL_STATUS;

/**
 * @brief Component or source that changed a param value.
 */
typedef enum
{
    CHANGED_BY_FACTORY_DEFAULT      = (1<<0), /**< Factory Defaults */
    CHANGED_BY_ACS                  = (1<<1), /**< ACS/TR-069 */
    CHANGED_BY_WEBPA                = (1<<2), /**< WebPA */
    CHANGED_BY_CLI                  = (1<<3), /**< Command Line Interface (CLI) */
    CHANGED_BY_SNMP                 = (1<<4), /**< SNMP */
    CHANGED_BY_FIRMWARE_UPGRADE     = (1<<5), /**< Firmware Upgrade */
    CHANGED_BY_WEBUI                = (1<<7), /**< Local Web UI (HTTP) */
    CHANGED_BY_UNKNOWN              = (1<<8), /**< Unknown */
    CHANGED_BY_XPC                  = (1<<9)  /**< xPC */
} PARAMVAL_CHANGE_SOURCE;

/**
 * @brief Set operations supported by WebPA.
 */
typedef enum
{
    WEBPA_SET = 0,
    WEBPA_ATOMIC_SET,
    WEBPA_ATOMIC_SET_XPC
} WEBPA_SET_TYPE;

/**
 * @brief WebPA Config params.
 */
typedef enum
{
    WCFG_COMPONENT_NAME = 0,
    WCFG_CFG_FILE,
    WCFG_CFG_FILE_SRC,
    WCFG_DEVICE_INTERFACE,
    WCFG_DEVICE_MAC,
    WCFG_DEVICE_REBOOT_PARAM,
    WCFG_DEVICE_REBOOT_VALUE,
    WCFG_XPC_SYNC_PARAM_CID,
    WCFG_XPC_SYNC_PARAM_CMC,
    WCFG_FIRMWARE_VERSION,
    WCFG_DEVICE_UP_TIME,
    WCFG_MANUFACTURER,
    WCFG_MODEL_NAME,
    WCFG_XPC_SYNC_PARAM_SPV,
    WCFG_PARAM_HOSTS_NAME,
    WCFG_PARAM_HOSTS_VERSION,
    WCFG_PARAM_SYSTEM_TIME,
    WCFG_RECONNECT_REASON,
    WCFG_REBOOT_REASON,
    WCFG_REBOOT_COUNTER
} WCFG_PARAM_NAME;

/**
 * @brief Structure to store Parameter info or Attribute info.
 */
typedef struct
{
    char *name;
    char *value;
    DATA_TYPE type;
} ParamVal, AttrVal;

/**
 * @brief Structure to return Parameter info in Notification callback.
 */
typedef struct
{
    const char *paramName;
    const char *oldValue;
    const char *newValue;
    DATA_TYPE type;
    PARAMVAL_CHANGE_SOURCE changeSource;
} ParamNotify;

typedef struct
{
    char *msg;
    unsigned int msgLength;
} UpstreamMsg;

typedef struct
{
   char *transId;
}TransData;

typedef struct
{
   char *nodeMacId;
   char *status;
}NodeData;

typedef union
{
    ParamNotify *notify;
    UpstreamMsg *msg;
    TransData * status;
    NodeData * node;
} Notify_Data;

typedef enum
{
    PARAM_NOTIFY = 0,
    UPSTREAM_MSG,
    TRANS_STATUS,
    CONNECTED_CLIENT_NOTIFY
} NOTIFY_TYPE;

typedef struct
{
    NOTIFY_TYPE type;
    Notify_Data *data;
} NotifyData;


/**
 * @brief WebPA Configuration parameters.
 */
typedef struct
{
    char interfaceName[16];
    char serverIP[256];
    unsigned int serverPort;
    int secureFlag;
    unsigned int retryIntervalInSec;
    unsigned int maxPingWaitTimeInSec;
    char oldFirmwareVersion[256];
} WebPaCfg;

typedef struct 
{
  int parameterCount;   
  char **parameterNames;
  char **parameterValues;
} TableData;

typedef struct  
{
    char *name;
    uint64_t start;     
    uint32_t duration;  
} money_trace_span;

typedef struct  
{
    money_trace_span *spans;
    size_t count;
} money_trace_spans;

/**
 * @brief Function pointer for Notification callback
 */
typedef void (*notifyCB)(NotifyData *notifyDataPtr);

/**
 * @brief Initializes the Message Bus and registers WebPA component with the stack.
 *
 * @param[in] name WebPA Component Name.
 * @return WAL_STATUS.
 */
WAL_STATUS msgBusInit(const char *name);

/**
 * @brief Registers the notification callback function.
 *
 * @param[in] cb Notification callback function.
 * @return WAL_STATUS.
 */
WAL_STATUS RegisterNotifyCB(notifyCB cb);

/**
 * @brief getValues interface returns the parameter values.
 *
 * getValues supports an option to pass wildward parameters. This can be achieved by passing an object name followed by '.'
 * instead of parameter name.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[out] timeSpan timing_values for each component. 
 * @param[out] paramValArr Two dimentional array of parameter name/value pairs.
 * @param[out] retValCount List of "number of parameters" for each input paramName. Usually retValCount will be 1 except
 * for wildcards request where it represents the number of param/value pairs retrieved for the particular wildcard parameter.
 * @param[out] retStatus List of Return status.
 */
void getValues(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, ParamVal ***paramValArr, int *retValCount, WAL_STATUS *retStatus);

/**
 * @brief setValues interface sets the parameter value.
 *
 * @param[in] paramVal List of Parameter name/value pairs.
 * @param[in] paramCount Number of parameters.
 * @param[in] setType Flag to specify the type of set operation.
 * @param[out] timeSpan timing_values for each component. 
 * @param[out] retStatus List of Return status.
 */
void setValues(const ParamVal paramVal[], const unsigned int paramCount, const WEBPA_SET_TYPE setType, money_trace_spans *timeSpan, WAL_STATUS *retStatus,char * transaction_id);

/**
 * @brief getAttributes interface returns all the attribute/value pairs of the given parameters.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[out] timeSpan timing_values for each component. 
 * @param[out] attr Two dimentional array of attribute name/value pairs.
 * @param[out] retAttrCount List of "number of attributes" for each input paramName.
 * @param[out] retStatus List of Return status.
 */
void getAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus);

/**
 * @brief setAttributes interface sets the attribute values.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[out] timeSpan timing_values for each component. 
 * @param[in] attr List of attribute name/value pairs.
 * @param[out] retStatus List of Return status.
 */
void setAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, const AttrVal *attr[], WAL_STATUS *retStatus);

/**
 * @brief Loads the WebPA config file, parses it and extracts the WebPA config parameters.
 *
 * @param[in] cfgFileName Config filename (with absolute path)
 * @param[out] cfg WebPA config parameters
 * @return WAL_STATUS
 */
WAL_STATUS loadCfgFile(const char *cfgFileName, WebPaCfg *cfg);

/**
 * @brief WALInit Initalize wal
 */
void WALInit();

/**
 * @brief LOGInit Initialize RDK Logger
 */
void LOGInit();

/**
 * @brief waitForConnectReadyCondition wait till all dependent components 
 * required for connecting to server are initialized
 */
void waitForConnectReadyCondition();

/**
 * @brief waitForOperationalReadyCondition wait till all dependent components
 * required for being operational are initialized
 */
void waitForOperationalReadyCondition();

/**
 * @brief _WEBPA_LOG WEBPA RDK logger API
 */

void _WEBPA_LOG(unsigned int level, const char *msg, ...)
    __attribute__((format (printf, 2, 3)));

/**
 * @brief getWebPAConfig interface returns the WebPA config data.
 *
 * @param[in] param WebPA config param name.
 * @return const char* WebPA config param value.
 */
const char* getWebPAConfig(WCFG_PARAM_NAME param);

/**
 * @brief sendIoTMessage interface sends message to IoT. 
 *
 * @param[in] msg Message to be sent to IoT.
 * @return WAL_STATUS
 */
WAL_STATUS sendIoTMessage(const void *msg);

/**
* @brief walStrncpy WAL String copy function that copies the content of source string into destination string 
* and null terminates the destination string
*
* @param[in] destStr Destination String
* @param[in] srcStr Source String
* @param[in] destSize size of destination string
*/
void walStrncpy(char *destStr, const char *srcStr, size_t destSize);

void addRowTable(const char *objectName,TableData *list,char **retObject, WAL_STATUS *retStatus);
void deleteRowTable(const char *object,WAL_STATUS *retStatus);
void replaceTable(const char *objectName,TableData * list,int paramcount,WAL_STATUS *retStatus);

void getNotifyParamList(const char ***paramList,int *size);
#endif /* _WAL_H_ */
