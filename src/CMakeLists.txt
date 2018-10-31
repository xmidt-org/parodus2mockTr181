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
                       -lnng
                       -lpthread
                       -lrt
                       )
    
install (TARGETS ${PROJ_MOCK_TR181} DESTINATION bin)
