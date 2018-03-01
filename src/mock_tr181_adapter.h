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

#ifndef __MOCK_TR181_ADAPTER_H__
#define __MOCK_TR181_ADAPTER_H__

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define P2M_DB_FILE      "../../mock_tr181.json"

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
int mock_tr181_db_init(char* db_name);

int mock_tr181_db_read(char **data);

int mock_tr181_db_write(char *data);

/*--------------------------------- End --------------------------------------*/
#endif /*__MOCK_TR181_ADAPTER_H__*/

