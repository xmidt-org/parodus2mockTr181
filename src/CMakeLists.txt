#   Copyright 2018 Comcast Cable Communications Management, LLC
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

set(PROJ_MOCK_TR181 mock_tr181)

set(SOURCES mock_tr181_adapter.c mock_tr181_client.c mock_tr181_main.c)

add_executable(${PROJ_MOCK_TR181} ${SOURCES})

## Hint: To use the small DB : cmake -D SMALL_JSON_DB=ON ..
option(SMALL_JSON_DB "SMALL_JSON_DB")

if (SMALL_JSON_DB)

execute_process(COMMAND cp mock_tr181_small.json mock_tr181_db.json
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src)

else ()

execute_process(COMMAND cp mock_tr181.json mock_tr181_db.json
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src)
endif ()


execute_process(COMMAND xxd -i mock_tr181_db.json mock_tr181_json.h
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src)

target_link_libraries (${PROJ_MOCK_TR181}
                       ${CMAKE_THREAD_LIBS_INIT}
                       -lwrp-c
                       -lwdmp-c
                       -lmsgpackc
                       -ltrower-base64
                       -lm
                       -lcimplog
                       -llibparodus
                       -lcjson
                       -lnanomsg
                       -lpthread
                       -lrt
                       )
    
install (TARGETS ${PROJ_MOCK_TR181} DESTINATION bin)
