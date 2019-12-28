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

import service

import additonal_report_Info

@service.api
def get(request, universal_unique_identifier):
    return additonal_report_Info.get_report_comments(universal_unique_identifier)

@service.api
def put(request, report):
    return {'status': additonal_report_Info.update_report_comment(report)}