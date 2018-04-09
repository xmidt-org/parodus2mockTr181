/**
 *  Copyright 2010-2018 Comcast Cable Communications Management, LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <CUnit/Basic.h>
#include <stdbool.h>

#include "../src/mock_tr181_client.h"
#include "../src/mock_tr181_adapter.h"

static char *request = "{ \"names\":[\"Device.DNS.Client.Enable\",\"Device.DNS.Client.\"],\"command\": \"GET\"}";
static char *invalid_request = "{ \"names\":[\"Device.DNSZZ.Client.Enable\",\"Device.DNS.Client.\"],\"command\": \"GET\"}";

static char *attributes[] = {
    "{\"names\":[\"Device.Bridging.Bridge.8.Port.2.LowerLayers\",\"Device.Bridging.Bridge.8.Port.2.ManagementPort\"],\"attributes\":\"notify\",\"command\": \"GET_ATTRIBUTES\"}",
    "{\"names\":[\"Device.WiFi.SSID.1.Enable\",\"Device.WiFi.SSID.1.SSID\"],\"attributes\":\"notify\",\"command\": \"GET_ATTRIBUTES\"}",
    "{\"names\":[\"Device.Ethernet.Link.2.LastChange\",\"Device.Ethernet.Link.2\"],\"attributes\":\"notify\",\"command\": \"GET_ATTRIBUTES\"}",
 };

static char  *set_request= "{\"parameters\":[{\"name\":\"Device.DeviceInfo.ProductClass\",\
\"value\":\"XB3\",\"dataType\":0,\"attributes\": { \"notify\": 1}},{\"name\":\
\"Device.DeviceInfo.SerialNumber\",\"value\":\"14cfe2142142\",\"dataType\":0,\
\"attributes\": { \"notify\": 1}}],\"command\":\"SET\"}";


void test_init(void)
{
    // Implement me?    
}

void test_large_db()
{
    char *response = NULL;
    cJSON *cached_db = NULL;
    int delay = 0;
    int cnt;
    char *cJSON_Error = NULL;

    cached_db = mock_tr181_db_init(NULL);
    cJSON_Error = (char *) cJSON_GetErrorPtr();
    if (NULL == cached_db) {
        printf("\n\n\ncJSON_Error %s\n\n\n", cJSON_Error);
    }
    CU_ASSERT(cached_db != NULL);   

    processRequest(request, &response, &delay);
    printf("response: %s\n", response);
    CU_ASSERT(response != NULL);
    free(response);
    printf("\n**********************\nReturned Delay Value:%d\n**********************\n", delay);
    processRequest(invalid_request, &response, &delay);
    printf("response: %s\n", response);
    CU_ASSERT(response != NULL);   
    free(response);
    printf("\n**********************\nReturned Delay Value:%d\n**********************\n", delay);
    
    processRequest(set_request, &response, &delay);
    printf("response: %s\n", response);
    CU_ASSERT(response != NULL);   
    free(response);
    
    for (cnt = 0; cnt < 3;cnt++) {
        processRequest(attributes[cnt], &response, &delay);
        printf("response: %s\n", response);
        CU_ASSERT(response != NULL);   
        free(response);
    }

    cJSON_Delete(cached_db);
}

void add_suites( CU_pSuite *suite )
{
    *suite = CU_add_suite( "mock_tr181 encoding tests", NULL, NULL );
    CU_add_test( *suite, "test_large_db", test_large_db );
}

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main( void )
{
    unsigned rv = 1;
    CU_pSuite suite = NULL;

    if( CUE_SUCCESS == CU_initialize_registry() ) {
        add_suites( &suite );

        if( NULL != suite ) {
            test_init();
            CU_basic_set_mode( CU_BRM_VERBOSE );
            CU_basic_run_tests();
            printf( "\n" );
            CU_basic_show_failures( CU_get_failure_list() );
            printf( "\n\n" );
            rv = CU_get_number_of_tests_failed();
        }

        CU_cleanup_registry();
    }

    if( 0 != rv ) {
        return 1;
    }
    return 0;
}
