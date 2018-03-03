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

static char* g_mock_tr181_db_name = NULL;
/*----------------------------------------------------------------------------*/
/*                 External interface functions                               */
/*----------------------------------------------------------------------------*/
/*
 * returns 0 - failure
 *         1 - success
 */

int mock_tr181_db_init(char* db_name)
{
	if(g_mock_tr181_db_name)
	{
		free(g_mock_tr181_db_name);
		g_mock_tr181_db_name = NULL;
	}

	if(db_name)
	{
		Info("MockTR181 DB file: \"%s\"\n", db_name);
		g_mock_tr181_db_name = strdup(db_name);
	}
	else
	{
		Error("Mock DB name invalid !!\n");
		return 0;
	}

	return 1;
}


int mock_tr181_db_read(char **data)
{
	Info("mock_tr181_db_read() Entered\n");
	FILE *fp;
	int ch_count = 0;
	if(g_mock_tr181_db_name)
	{
		fp = fopen(g_mock_tr181_db_name, "r");
		if (fp == NULL)
		{
			Error("Failed to open db file \"%s\" !\n", g_mock_tr181_db_name);
			return 0;
		}

		Info("Opened db file \"%s\"\n", g_mock_tr181_db_name);
	}
	else
	{
		Info("Mock db file not specified in options. Using default db file: %s\n", P2M_DB_FILE);
		fp = fopen(P2M_DB_FILE, "r");
		if (fp == NULL)
		{
			Error("Failed to open db file \"%s\"\n", P2M_DB_FILE);
			return 0;
		}

		Info("Opened default db file \"%s\"\n", P2M_DB_FILE);
	}

	fseek(fp, 0, SEEK_END);  //set file position to end
	ch_count = ftell(fp);    // get the file position
	fseek(fp, 0, SEEK_SET);  //set file position to start

	Info("DB file size : %d\n", ch_count);
	*data = (char *) malloc(sizeof(char) * (ch_count + 1)); //allocate memory for file size
	if (*data != NULL)
	{
		memset(*data, 0, ch_count+1);
		size_t sz = fread(*data, 1, ch_count, fp);  // copy file to memory
		if(sz == 0)
		{
			Error("Error: Read file size: %d\n", sz);
		}

		Info("Read %d bytes from db file\n", sz);
		(*data)[ch_count] = '\0';
	}
	else
	{
		Error("Failed to allocate memory of %d bytes\n", ch_count);
		return 0;
	}

	fclose(fp);
	Print("mock_tr181_db_read() Returned Success\n");
	return 1;
}

/*
 * returns 0 - failure
 *         1 - success
 */
int mock_tr181_db_write(char *data)
{
	Print("mock_tr181_db_write() Entered\n");
	FILE *fp;

	if(g_mock_tr181_db_name)
	{
		fp = fopen(g_mock_tr181_db_name, "w");
		if (fp == NULL)
		{
			Error("Failed to open db file \"%s\"\n", g_mock_tr181_db_name);
			return 0;
		}
	}
	else
	{
		Print("Mock db file not specified in options. Using default db file: %s\n", P2M_DB_FILE);
		fp = fopen(P2M_DB_FILE, "w");
		if (fp == NULL)
		{
			Error("Failed to open db file \"%s\"\n", P2M_DB_FILE);
			return 0;
		}
	}

	fwrite(data, strlen(data), 1, fp);

	fclose(fp);
	Print("mock_tr181_db_write() Returned Success\n");
	return 1;
}
