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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include "mock_tr181_adapter.h"
#include "mock_tr181_client.h"


static void __sig_handler(int sig);
static void init_signal_handler(void);
static void _exit_process_(int signum);
static bool terminate_mock_TR181_client = false;

int main( int argc, char **argv)
{
	const char *option_string = "p:c:d:h::";
	static const struct option options[] = {
			{ "help", optional_argument, 0, 'h' },
			{ "parodus-port", optional_argument, 0, 'p' },
			{ "client-port", optional_argument, 0, 'c' },
			{ "db-file", optional_argument, 0, 'd' },
			{ 0, 0, 0, 0 }
	};

	int item = 0, opt_index = 0;
	char* parodus_port = NULL;
	char* client_port = NULL;
    
    init_signal_handler();

	while (-1 != (item = getopt_long(argc, argv, option_string, options, &opt_index)))
	{
		switch (item)
		{
			case 'p':
				Info("Option p read\n");
				parodus_port = strdup(optarg);
				break;
			case 'c':
				Info("Option c read\n");
				client_port = strdup(optarg);
				break;
			case 'd':
				Info("Option d read\n");
                /* Note: Specifying "null" or "/dev/null" will override the make
                   built-in data-base file, so we start with an empty db
                   If this option is not used, or the file is not found then the
                   internal db is used. */
				mock_tr181_db_init(optarg);
				break;
			case 'h':
				Info("Option h read\n");
				printf("Usage:%s [-p <parodus_port>] [-c <client_port>] [-d <../path/to/database_name>]\n", argv[0]);
				return 0;
			case '?':
				Info("Option invalid\n");
				if (strchr(option_string, optopt))
				{
					printf("%s Option %c requires an argument!\n", argv[0], optopt);
					printf("Usage:%s [-p <parodus_port>] [-c <client_port>] [-d <../path/to/database_name>]\n", argv[0]);
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
		sleep(5);
        if (terminate_mock_TR181_client) {
            Info("%s terminating!\n", argv[0]);
            exit(-1);
        }
	}
	return 0;
}


void init_signal_handler(void)
{
      signal(SIGTERM, __sig_handler);
      signal(SIGINT, __sig_handler);
      signal(SIGUSR1, __sig_handler);
      signal(SIGUSR2, __sig_handler);
      signal(SIGSEGV, __sig_handler);
      signal(SIGBUS, __sig_handler);
      signal(SIGKILL, __sig_handler);
      signal(SIGFPE, __sig_handler);
      signal(SIGILL, __sig_handler);
      signal(SIGQUIT, __sig_handler);
      signal(SIGHUP, __sig_handler);
      signal(SIGALRM, __sig_handler);
}


void __sig_handler(int sig)
{

    if ( sig == SIGINT ) {
        signal(SIGINT, __sig_handler); /* reset it to this function */
        Info("mock_TR181_client SIGINT received!\n");
        _exit_process_(sig);
    }
    else if ( sig == SIGUSR1 ) {
        signal(SIGUSR1, __sig_handler); /* reset it to this function */
        Info("WEBPA SIGUSR1 received!\n");
    }
    else if ( sig == SIGUSR2 ) {
        Info("mock_TR181_client SIGUSR2 received!\n");
    }
    else if ( sig == SIGCHLD ) {
        signal(SIGCHLD, __sig_handler); /* reset it to this function */
        Info("mock_TR181_client SIGHLD received!\n");
    }
    else if ( sig == SIGPIPE ) {
        signal(SIGPIPE, __sig_handler); /* reset it to this function */
        Info("mock_TR181_client SIGPIPE received!\n");
    }
    else if ( sig == SIGALRM ) {
        signal(SIGALRM, __sig_handler); /* reset it to this function */
        Info("mock_TR181_client SIGALRM received!\n");
    }
    else if( sig == SIGTERM ) {
        Info("mock_TR181_client SIGTERM received!\n");
        _exit_process_(sig);
    } else if ( sig == SIGABRT ) {
         Info("mock_TR181_client SIGABRT received!\n");
        _exit_process_(sig);       
    }
    else {
        Info("mock_TR181_client Signal %d received!\n", sig);
        _exit_process_(sig);
    }
}

void _exit_process_(int signum)
{
  char *s = strsignal(signum);
  Info("mock_TR181_client ready to exit! SIGNAL %d [%s]\n", signum, s ? s : "unknown signal");
  terminate_mock_TR181_client = true;
  sleep(1);
  signal(signum, SIG_DFL);
  kill(getpid(), signum);
}

