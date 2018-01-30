/**
 * Copyright 2016 Comcast Cable Communications Management, LLC
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

#include "mock_tr181_client.h"
#include "mock_tr181_adapter.h"

/*----------------------------------------------------------------------------*/
/*                              functions                             */
/*----------------------------------------------------------------------------*/

int readFromDB(char **data)
{
    FILE *fp;
    int ch_count = 0;
    fp = fopen(DB_FILE, "r");
    if (fp == NULL)
    {
        Error("Failed to open file %s\n", DB_FILE);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
	ch_count = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	*data = (char *) malloc(sizeof(char) * (ch_count + 1));
	fread(*data, 1, ch_count,fp);
	(*data)[ch_count] ='\0';
	fclose(fp);
	return 1;
}


int writeToDB(char *data)
{
    FILE *fp;
    fp = fopen(DB_FILE, "w");
    if (fp == NULL)
    {
        Error("Failed to open file %s\n", DB_FILE);
        return 0;
    }
    fwrite(data, strlen(data), 1, fp);
    fclose(fp);
    return 1;
}
