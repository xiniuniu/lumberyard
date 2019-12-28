#!/bin/sh

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
#

if [ -z "$3" ]; then
    echo "Missing one or more required params. Usage:\nlmbr_pak_shaders.sh gameProjectName GLES3|METAL es3|ios|osx_gl [source shader list path]"
    exit 1
fi

# Extract an optional external engine path if present, otherwise use the cwd as the engine dir
EXTERNAL_ENGINE_PATH=`cat engine.json | grep "ExternalEnginePath" | awk -F":" '{ print $2 }' | sed "s/,//g" | sed "s/\"//g" | xargs echo -n`
if [ -z $EXTERNAL_ENGINE_PATH ]; then
    ENGINE_DIR=$(dirname "$0")
elif [ -d $EXTERNAL_ENGINE_PATH ]; then
    ENGINE_DIR=$EXTERNAL_ENGINE_PATH
else
    echo External Path in engine.json "$EXTERNAL_ENGINE_PATH" does not exist
    exit 1
fi

BIN_FOLDER=`cat _WAF_/user_settings.options | grep "out_folder_mac64" | awk -F"=" '{ print $2 }' | sed "s/^[ \t]*//g" | sed "s/\"//g" | xargs echo -n`
if [ -z "$BIN_FOLDER" ]; then
    echo Unable to find Mac64 bin output folder in _WAF/user_settings.options. Defaulting to BinMac64.
    BIN_FOLDER="BinMac64"
fi

game="$1"
shader_type="$2"
platform="$3"
source_shader_list="$4"

gen_args="$game $platform $shader_type $BIN_FOLDER -e $ENGINE_DIR"
if [ ! -z $source_shader_list ]; then
    gen_args=$gen_args -s $source_shader_list
fi

env python "$ENGINE_DIR/Tools/PakShaders/gen_shaders.py" $gen_args

if [ $? -ne 0 ]; then
    echo Failed to generate shaders.
    exit 1
fi

source="Cache/$game/$platform/user/cache/shaders/cache"
output="Build/$platform/$game"

env python "$ENGINE_DIR/Tools/PakShaders/pak_shaders.py" $output -r $source -s $shader_type

if [ $? -ne 0 ]; then
    echo Failed to pack shaders.
    exit 1
fi

