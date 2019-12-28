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

#include <CloudGemAWSScriptBehaviors_precompiled.h>

#include "AWSBehaviorS3Upload.h"

/// To use a specific AWS API request you have to include each of these.
#include <AzCore/PlatformDef.h>
AZ_PUSH_DISABLE_WARNING(4251 4355 4996, "-Wunknown-warning-option")
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
AZ_POP_DISABLE_WARNING
#include <fstream>
#include <CloudCanvas/CloudCanvasMappingsBus.h>

namespace CloudGemAWSScriptBehaviors
{
    static const char* UPLOAD_CLASS_TAG = "AWS:Primitive:AWSBehaviorS3Upload";

    AWSBehaviorS3Upload::AWSBehaviorS3Upload() :
        m_bucketName(),
        m_keyName(),
        m_localFileName()
    {

    }

    void AWSBehaviorS3Upload::ReflectSerialization(AZ::SerializeContext* serializeContext)
    {
        if (serializeContext)
        {
            serializeContext->Class<AWSBehaviorS3Upload>()
                ->Field("bucketName", &AWSBehaviorS3Upload::m_bucketName)
                ->Field("keyName", &AWSBehaviorS3Upload::m_keyName)
                ->Field("localFileName", &AWSBehaviorS3Upload::m_localFileName)
                ->Field("contentType", &AWSBehaviorS3Upload::m_contentType)
                ->Version(1);
        }
    }

    void AWSBehaviorS3Upload::ReflectBehaviors(AZ::BehaviorContext* behaviorContext)
    {
        behaviorContext->Class<AWSBehaviorS3Upload>("AWSBehaviorS3Upload")
            ->Method("Upload", &AWSBehaviorS3Upload::Upload, nullptr, "S3 upload operation on AWS")
            ->Property("bucketName", nullptr, BehaviorValueSetter(&AWSBehaviorS3Upload::m_bucketName))
            ->Property("keyName", nullptr, BehaviorValueSetter(&AWSBehaviorS3Upload::m_keyName))
            ->Property("localFileName", nullptr, BehaviorValueSetter(&AWSBehaviorS3Upload::m_localFileName))
            ->Property("contentType", nullptr, BehaviorValueSetter(&AWSBehaviorS3Upload::m_contentType));

        behaviorContext->EBus<AWSBehaviorS3UploadNotificationsBus>("AWSBehaviorS3UploadNotificationsBus")
            ->Handler<AWSBehaviorS3UploadNotificationsBusHandler>();
    }

    void AWSBehaviorS3Upload::ReflectEditParameters(AZ::EditContext* editContext)
    {
        editContext->Class<AWSBehaviorS3Upload>("AWSBehaviorS3Upload", "Wraps AWS S3 functionality")
            ->DataElement(AZ::Edit::UIHandlers::Default, &AWSBehaviorS3Upload::m_bucketName, "BucketName", "The name of the bucket to use")
            ->DataElement(AZ::Edit::UIHandlers::Default, &AWSBehaviorS3Upload::m_keyName, "KeyName", "What to name the object being uploaded. If you do not update this as you go, the existing object will be overwritten")
            ->DataElement(AZ::Edit::UIHandlers::Default, &AWSBehaviorS3Upload::m_localFileName, "LocalFileName", "Name of local file to upload.")
            ->DataElement(AZ::Edit::UIHandlers::Default, &AWSBehaviorS3Upload::m_contentType, "ContentType", "The mime content-type to use for the uploaded objects such as text/html, video/mpeg, video/avi, or application/zip.  This type is then stored in the S3 record and can be used to help identify what type of data you're looking at or retrieving later.");
    }

    void AWSBehaviorS3Upload::Upload()
    {
        if (m_localFileName.empty())
        {
            EBUS_EVENT(AWSBehaviorS3UploadNotificationsBus, OnError, "Please specify a local file name");
            return;
        }

        if (m_bucketName.empty())
        {
            EBUS_EVENT(AWSBehaviorS3UploadNotificationsBus, OnError, "Please specify a bucket name");
            return;
        }

        if (m_keyName.empty())
        {
            EBUS_EVENT(AWSBehaviorS3UploadNotificationsBus, OnError, "Please specify a key name");
            return;
        }

        if (m_contentType.empty())
        {
            EBUS_EVENT(AWSBehaviorS3UploadNotificationsBus, OnError, "Please specify a content type");
            return;
        }

        AZStd::string bucketName;
        EBUS_EVENT_RESULT(bucketName, CloudGemFramework::CloudCanvasMappingsBus, GetLogicalToPhysicalResourceMapping, m_bucketName.c_str());

        using S3UploadRequestJob = AWS_API_REQUEST_JOB(S3, PutObject);
        S3UploadRequestJob::Config config(S3UploadRequestJob::GetDefaultConfig());
        AZStd::string region;
        EBUS_EVENT_RESULT(region, CloudGemFramework::CloudCanvasMappingsBus, GetLogicalToPhysicalResourceMapping, "region");
        config.region = region.c_str();

        auto job = S3UploadRequestJob::Create(
            [](S3UploadRequestJob* job) // OnSuccess handler
            {
                AZStd::function<void()> notifyOnMainThread = []()
                {
                    AWSBehaviorS3UploadNotificationsBus::Broadcast(&AWSBehaviorS3UploadNotificationsBus::Events::OnSuccess, "File Uploaded");
                };
                AZ::TickBus::QueueFunction(notifyOnMainThread);
            },
            [](S3UploadRequestJob* job) // OnError handler
            {
                Aws::String errorMessage = job->error.GetMessage();
                AZStd::function<void()> notifyOnMainThread = [errorMessage]()
                {
                    AWSBehaviorS3UploadNotificationsBus::Broadcast(&AWSBehaviorS3UploadNotificationsBus::Events::OnError, errorMessage.c_str());
                };
                AZ::TickBus::QueueFunction(notifyOnMainThread);
            },
            &config
        );

        AZStd::array<char, AZ::IO::MaxPathLength> resolvedPath;
        AZ::IO::FileIOBase::GetInstance()->ResolvePath(m_localFileName.c_str(), resolvedPath.data(), resolvedPath.size());

        std::shared_ptr<Aws::IOStream> fileToUpload = Aws::MakeShared<Aws::FStream>(UPLOAD_CLASS_TAG, resolvedPath.data(), std::ios::binary | std::ios::in);

        if (fileToUpload->good())
        {
            job->request.SetKey(Aws::String(m_keyName.c_str()));
            job->request.SetBucket(Aws::String(bucketName.c_str()));
            job->request.SetContentType(Aws::String(m_contentType.c_str()));
            job->request.SetBody(fileToUpload);
            job->Start();
        }
        else
        {
            Aws::StringStream ss;
            char buf[1024];
            azstrerror_s(buf, sizeof(buf), errno);
            ss << buf;
            EBUS_EVENT(AWSBehaviorS3UploadNotificationsBus, OnError, ss.str().c_str());
        }
    }
}