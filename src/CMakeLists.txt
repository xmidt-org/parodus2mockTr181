#   Copyright 2016 Comcast Cable Communications Management, LLC
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

file(GLOB HEADERS mock_tr181_adapter.h mock_tr181_client.h)
set(SOURCES mock_tr181_adapter.c mock_tr181_client.c mock_tr181_main.c)

add_library(${PROJ_MOCK_TR181} STATIC ${HEADERS} ${SOURCES})
add_library(${PROJ_MOCK_TR181}.shared SHARED ${HEADERS} ${SOURCES})
set_target_properties(${PROJ_MOCK_TR181}.shared PROPERTIES OUTPUT_NAME ${PROJ_MOCK_TR181})

install (TARGETS ${PROJ_MOCK_TR181} DESTINATION lib${LIB_SUFFIX})
install (TARGETS ${PROJ_MOCK_TR181}.shared DESTINATION lib${LIB_SUFFIX})
install (FILES ccsp.h DESTINATION include/${PROJ_MOCK_TR181})
