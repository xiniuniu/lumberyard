/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AudioSystemImpl_wwise.h>

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AzCore/Android/AndroidEnv.h>

namespace Audio
{
    namespace Platform
    {
        void SetupAkSoundEngine(AkPlatformInitSettings& platformInitSettings)
        {
            JNIEnv* jniEnv = AZ::Android::AndroidEnv::Get()->GetJniEnv();
            jobject javaActivity = AZ::Android::AndroidEnv::Get()->GetActivityRef();
            JavaVM* javaVM = nullptr;
            if (jniEnv)
            {
               jniEnv->GetJavaVM(&javaVM);
            }

            platformInitSettings.pJavaVM = javaVM;
            platformInitSettings.jNativeActivity = javaActivity;
        }
    }
}
