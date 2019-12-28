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

import random
import time
import json
import datetime
import os
import boto3
import boto3.session
import urllib

from resource_manager_common import constant
import custom_resource_utils
import json_utils

from botocore.exceptions import ClientError
from botocore.exceptions import EndpointConnectionError
from botocore.exceptions import IncompleteReadError
from botocore.exceptions import ConnectionError
from botocore.exceptions import BotoCoreError
from botocore.exceptions import UnknownEndpointError
from botocore.client import Config


current_region = os.environ.get('AWS_REGION')


LOG_ATTEMPT = 3
LOG_SUCCESS = 2
LOG_FAILURE = 1
LOG_LEVEL_NONE = []
LOG_LEVEL_ALL = [LOG_ATTEMPT, LOG_SUCCESS, LOG_FAILURE]

class ClientWrapper(object):
    BACKOFF_BASE_SECONDS = 1.25
    BACKOFF_MAX_SECONDS = 60.0
    BACKOFF_MAX_TRYS = 15
    MAX_LOG_STRING_LENGTH = 200

    def __init__(self, wrapped_client, do_not_log_args = [], log_level=LOG_LEVEL_ALL):
        self.__wrapped_client = wrapped_client
        self.__client_type = type(wrapped_client).__name__
        self.__do_not_log_args = do_not_log_args
        self.__log_level = log_level

    @property
    def client_type(self):
        return self.__client_type

    def __getattr__(self, attr):
        orig_attr = self.__wrapped_client.__getattribute__(attr)
        if callable(orig_attr):

            def client_wrapper(*args, **kwargs):
                # http://www.awsarchitectureblog.com/2015/03/backoff.html
                sleep_seconds = self.BACKOFF_BASE_SECONDS
                backoff_base = self.BACKOFF_BASE_SECONDS
                backoff_max = self.BACKOFF_MAX_SECONDS

                if 'baseBackoff' in kwargs:
                    backoff_base = kwargs['baseBackoff']
                    del kwargs['baseBackoff']

                if 'maxBackoff' in kwargs:
                    backoff_max = kwargs['maxBackoff']
                    del kwargs['maxBackoff']

                count = 1
                while True:
                    self.__log_attempt(attr, args, kwargs)
                    try:
                        result = orig_attr(*args, **kwargs)
                        self.__log_success(attr, result)
                        return result
                    except (ClientError, EndpointConnectionError, IncompleteReadError, ConnectionError, UnknownEndpointError) as e:

                        # Do not catch BotoCoreError here!!! That error is the base for all kinds of errors
                        # that should not be retried. For example: ParamValidationError. Errors like this
                        # will never succeed, but the backoff takes a very long time. In the case of
                        # custom resource handlers this can cause the lambda to timeout, which causes cloud
                        # formation to retry quite a few times before giving up. This can effectivly hang
                        # stack update/create for hours.

                        self.__log_failure(attr, e)

                        if count == self.BACKOFF_MAX_TRYS or (
                            isinstance(e, ClientError) and e.response['Error']['Code'] not in ['Throttling',
                                                                                               'TooManyRequestsException']):
                            raise e

                        temp = min(backoff_max, backoff_base * 2 ** count)

                        sleep_seconds = temp / 2 + random.uniform(0, temp / 2)

                        self.__log(attr, 'Retry attempt {}. Sleeping {} seconds'.format(count, sleep_seconds))

                        time.sleep(sleep_seconds)

                        count += 1

                    except Exception as e:
                        self.__log_failure(attr, e)
                        raise e

            return client_wrapper

        else:
            return orig_attr

    def __log(self, method_name, log_msg):

        msg = 'AWS '
        msg += self.__client_type
        msg += '.'
        msg += method_name
        msg += ' '
        msg += log_msg

        print msg

    def __log_attempt(self, method_name, args, kwargs):
        if not LOG_ATTEMPT in self.__log_level:
            return
        msg = 'attempt: '

        comma_needed = False
        for arg in args:
            if comma_needed: msg += ', '
            msg += arg
            msg += type(arg).__name__
            comma_needed = True

        for key, value in kwargs.iteritems():
            if key in self.__do_not_log_args: continue
            if comma_needed: msg += ', '
            msg += key
            msg += '='
            if isinstance(value, basestring):
                msg += '"'
                msg += value
                msg += '"'
            elif isinstance(value, dict):
                msg += json.dumps(value, cls=json_utils.SafeEncoder)
            else:
                msg += str(value)
            comma_needed = True

        self.__log(method_name, msg)

    def __log_success(self, method_name, result):
        if not LOG_SUCCESS in self.__log_level:
            return
        msg = 'success: '
        msg += type(result).__name__
        if isinstance(result, dict):
            msg += json.dumps(result, cls=json_utils.SafeEncoder)
        else:
            msg += str(result)

        self.__log(method_name, msg)

    def __log_failure(self, method_name, e):
        if not LOG_FAILURE in self.__log_level:
            return
        msg = ' failure '
        msg += type(e).__name__
        msg += ': '
        msg += str(getattr(e, 'response', e.message))

        self.__log(method_name, msg)


ID_DATA_MARKER = '::'


def get_data_from_custom_physical_resource_id(physical_resource_id):
    '''Returns data extracted from a physical resource id with embedded JSON data.'''
    if physical_resource_id:
        embedded_physical_resource_id = custom_resource_utils.get_embedded_physical_id(physical_resource_id)
        i_data_marker = embedded_physical_resource_id.find(ID_DATA_MARKER)
        if i_data_marker == -1:
            id_data = {}
        else:
            try:
                id_data = json.loads(embedded_physical_resource_id[i_data_marker + len(ID_DATA_MARKER):])
            except Exception as e:
                print 'Could not parse JSON data from physical resource id {}. {}'.format(embedded_physical_resource_id,
                                                                                          e.message)
                id_data = {}
    else:
        id_data = {}
    return id_data


def construct_custom_physical_resource_id_with_data(stack_arn, logical_resource_id, id_data):
    '''Creates a physical resource id with embedded JSON data.'''
    physical_resource_name = get_stack_name_from_stack_arn(stack_arn) + '-' + logical_resource_id
    id_data_string = json.dumps(id_data, sort_keys=True)
    return physical_resource_name + ID_DATA_MARKER + id_data_string


# Stack ARN format: arn:aws:cloudformation:{region}:{account}:stack/{name}/{uuid}

def get_stack_name_from_stack_arn(arn):
    return arn.split('/')[1]


def s3_key_join(*args):
    return constant.S3_DELIMETER.join(args)


def get_region_from_stack_arn(arn):
    return arn.split(':')[3]


def get_account_id_from_stack_arn(arn):
    return arn.split(':')[4]


def get_cloud_canvas_metadata(resource, metadata_name):
    metadata_string = resource.get('Metadata', None)
    if metadata_string is None: return

    try:
        metadata = json.loads(metadata_string)
    except ValueError as e:
        raise RuntimeError('Could not parse CloudCanvas {} Metadata: {}. {}'.format(metadata_name, metadata_string, e))

    cloud_canvas_metadata = metadata.get('CloudCanvas', None)
    if cloud_canvas_metadata is None: return

    return cloud_canvas_metadata.get(metadata_name, None)


def paginate(fn, params, paginator_key='Marker', next_paginator_key='NextMarker'):
    """
    A generic paginator that should work with any paginated AWS function, including those that do not have a built-in
    paginator supplied for them.

    :param fn: A client function to call, e.g. boto3.client('s3').list_objects
    :param params: A dictionary of parameters to pass into the function, e.g. {'Bucket': "foo"}
    :param paginator_key: The key used as the marker for fetching results, e.g. 'Marker'
    :param next_paginator_key: The key returned in the response to fetch the next page, e.g. 'NextMarker'
    :return: An iterator to the results of the function call.
    """
    while True:
        response = fn(**params)
        yield response

        next_key = response.get(next_paginator_key, None)
        if next_key and len(next_key):
            params[paginator_key] = next_key
        else:
            break


def get_resource_arn(type_definitions, stack_arn, resource_type, physical_id, optional=False, lambda_client=None):
    result = None
    physical_id = custom_resource_utils.get_embedded_physical_id(physical_id)
    type_definition = type_definitions.get(resource_type, None)
    if type_definition is None:
        if optional:
            return None
        else:
            raise RuntimeError(
                'Unsupported ARN mapping for resource type {} on resource {}. To add support for additional resource types, add a Custom::ResourceTypes dependency to your resource describing the type.'.format(
                    resource_type, physical_id))

    if type_definition.arn_function:
        if not lambda_client:
            lambda_client = ClientWrapper(boto3.client("lambda", get_region_from_stack_arn(stack_arn)))
        response = lambda_client.invoke(
            FunctionName=type_definition.get_arn_lambda_function_name(),
            Payload=json.dumps(
                {
                    'Handler': type_definition.arn_function,
                    'ResourceType': resource_type,
                    'Region': get_region_from_stack_arn(stack_arn),
                    'AccountId': get_account_id_from_stack_arn(stack_arn),
                    'ResourceName': physical_id,
                    'StackId': stack_arn
                }
            )
        )

        if response['StatusCode'] == 200:
            response_data = json.loads(response['Payload'].read().decode())
            result = response_data.get('Arn', None)
            if not result:
                raise RuntimeError("ARN lambda response for resource type '%s' did not contain an 'Arn' field. "
                                   "Response: %s" % (resource_type, response_data))

        else:
            raise RuntimeError("ARN lambda for resource type '%s' failed to execute, returned HTTP status %d" %
                               (resource_type, response['StatusCode']))

    elif type_definition.arn_format:
        result = type_definition.arn_format.format(
            resource_type=resource_type,
            region=get_region_from_stack_arn(stack_arn),
            account_id=get_account_id_from_stack_arn(stack_arn),
            resource_name=physical_id)

    else:
        raise RuntimeError(
            'Invalid ARN definition for resource type {} on resource {}. (This should have been detected earlier when the type was loaded.)'.format(
                resource_type, physical_id))

    return result


def get_role_name_from_role_arn(arn):
    # Role ARN format: arn:aws:iam::{account_id}:role/{resource_name}
    if arn is None: return None
    return arn[arn.rfind('/')+1:]


def get_cognito_pool_from_file(configuration_bucket, configuration_key, logical_name, stack):
    s3 = ClientWrapper(boto3.client('s3', current_region, config=Config(
        region_name=current_region, signature_version='s3v4', s3={'addressing_style': 'virtual'})))
    s3_key = configuration_key + '/cognito-pools.json'
    try:
        res = s3.get_object(Bucket=configuration_bucket, Key=s3_key)
        content = json.loads(res['Body'].read())
    except Exception as e:
        return ""
    key = ""
    if stack.is_deployment_access_stack:
        key = "DeploymentAccess"
    if stack.is_project_stack:
        key = "Project"

    return content.get(key, {}).get(logical_name, "")
