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


#include <getopt.h>
#include "mock_tr181_adapter.h"
#include "mock_tr181_client.h"

int main( int argc, char **argv)
{
    const char *option_string = "p:c:d:h::";
    static const struct option options[] = {
        { "help",          optional_argument, 0, 'h' },
        { "parodus-port",  optional_argument, 0, 'p' },
        { "client-port",   optional_argument, 0, 'c' },
        { "db-file",       optional_argument, 0, 'd' },
        { 0, 0, 0, 0 }
    };

    int item = 0, opt_index = 0;
    char* parodus_port = NULL, client_port = NULL;

    while( -1 != (item = getopt_long(argc, argv, option_string, options, &opt_index)) )
	{
		switch (item)
		{
			case 'p':
				parodus_port = strdup(optarg);
				break;
			case 'c':
				client_port = strdup(optarg);
				break;
			case 'd':
				mock_tr181_db_init(optarg);
				break;
			case 'h':
				printf("Usage:%s [-p <parodus_port>] [-c <client_port>] [-d <fully_qualified_database_name>]\n", argv[0]);
				break;
			case '?':
				if (strchr(option_string, optopt))
				{
					printf("%s Option %c requires an argument!\n", argv[0], optopt);
					printf("Usage:%s [-p <parodus_port>] [-c <client_port>] [-d <fully_qualified_database_name>]\n", argv[0]);
					break;
				}
				else
				{
					printf("%s Unrecognized option %c\n", argv[0], optopt);
				}

				break;

			default:
				break;
		}
	}

	connect_parodus(parodus_port, client_port);
	startParodusReceiveThread();
	sleep(5);

	while (1)  //start startParodusReceiveThread and wait...
	{
		sleep(10);
	}
	return 0;
}
