#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
# $Revision: #1 $

from resource_manager_common import stack_info

class StackInfoManagerContext(object):
    def __init__(self, context):
        self.__context = context
        self.__stack_info_manager = None

    @property
    def manager(self):
        if self.__stack_info_manager == None:
            self.__stack_info_manager = stack_info.StackInfoManager(default_session=self.__context.aws.session)

        return self.__stack_info_manager
