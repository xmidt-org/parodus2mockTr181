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
#ifndef __MOCK_TR181_CLIENT_H__
#define __MOCK_TR181_CLIENT_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "libparodus.h"
#include <nng/compat/nanomsg/nn.h>
#include <nng/compat/nanomsg/pipeline.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "wrp-c.h"
#include "wdmp-c.h"
#include "cJSON.h"
#include <string.h>
#include <cimplog.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define CONTENT_TYPE_JSON  "application/json"

#define LOGGING_MODULE     "MOCK_TR181"
#define Error(...)      cimplog_error(LOGGING_MODULE, __VA_ARGS__)
#define Info(...)       cimplog_info(LOGGING_MODULE, __VA_ARGS__)
#define Print(...)      cimplog_debug(LOGGING_MODULE, __VA_ARGS__)

// Default to not doing get/set attributes.
//#define SUPPORT_GET_SET_ATTRIBUTES

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
void connect_parodus(char* parodus_port, char* client_port);
/* bypass MUST only be set to true for unit tests which ignore parodus */
void connect_parodus_default(bool bypass);
void startParodusReceiveThread();

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Internal Private functions                     */
/*----------------------------------------------------------------------------*/
void processRequest(char *reqPayload, char **resPayload, int* resDelay);


#endif /* __MOCK_TR181_CLIENT_H__ */
