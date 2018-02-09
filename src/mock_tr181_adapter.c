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

#include "mock_tr181_client.h"
#include "mock_tr181_adapter.h"

/*----------------------------------------------------------------------------*/
/*                 External interface functions                               */
/*----------------------------------------------------------------------------*/
/*
 * returns 0 - failure
 *         1 - success
 */

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
    fseek(fp, 0, SEEK_END);  //set file position to end
	ch_count = ftell(fp);    // get the file position
	fseek(fp, 0, SEEK_SET);  //set file position to start

	*data = (char *) malloc(sizeof(char) * (ch_count + 1)); //allocate memory for file size
	if(*data != NULL)
	{
		fread(*data, 1, ch_count,fp);  // copy file to memory
		(*data)[ch_count] ='\0';
	}
	else
	{
		Error("Failed to allocate memory of %d bytes\n", ch_count);
		return 0;
	}

	fclose(fp);
	return 1;
}

/*
 * returns 0 - failure
 *         1 - success
 */
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
