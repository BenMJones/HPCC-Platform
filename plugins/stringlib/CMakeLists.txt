################################################################################
#    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
################################################################################


# Component: stringlib 

#####################################################
# Description:
# ------------
#    Cmake Input File for stringlib
#####################################################

set ( toolsdir "${HPCC_SOURCE_DIR}/tools" )


project( stringlib ) 

set (    SRCS 
         stringlib.cpp 
    )

include_directories ( 
         ./../../system/include 
    )

ADD_DEFINITIONS( -D_USRDLL -DSTRINGLIB_EXPORTS )

HPCC_ADD_LIBRARY( stringlib SHARED ${SRCS} )
install ( TARGETS stringlib DESTINATION plugins )


