﻿/*
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

#include "ToString.h"

#include <AzCore/Math/VectorFloat.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/Color.h>

namespace AZ
{
    AZStd::string ToString(const AZ::VectorFloat& vectorFloat)
    {
        return AZStd::string::format("%f", static_cast<float>(vectorFloat));
    }

    AZStd::string ToString(const AZ::Vector2& vector2)
    {
        return AZStd::string::format("(X:%f, Y:%f)", static_cast<float>(vector2.GetX()), static_cast<float>(vector2.GetY()));
    }

    AZStd::string ToString(const AZ::Vector3& vector3)
    {
        return AZStd::string::format("(X:%f, Y:%f, Z:%f)", static_cast<float>(vector3.GetX()), static_cast<float>(vector3.GetY()), static_cast<float>(vector3.GetZ()));
    }

    AZStd::string ToString(const AZ::Vector4& vector4)
    {
        return AZStd::string::format("(X:%f, Y:%f, Z:%f W:%f)", static_cast<float>(vector4.GetX()), static_cast<float>(vector4.GetY()), static_cast<float>(vector4.GetZ()), static_cast<float>(vector4.GetW()));
    }

    AZStd::string ToString(const AZ::Quaternion& quaternion)
    {
        return AZStd::string::format("(X:%f, Y:%f, Z:%f W:%f)", static_cast<float>(quaternion.GetX()), static_cast<float>(quaternion.GetY()), static_cast<float>(quaternion.GetZ()), static_cast<float>(quaternion.GetW()));
    }

    AZStd::string ToString(const AZ::Transform& transform)
    {
        auto row1 = transform.GetRow(0);
        auto row2 = transform.GetRow(1);
        auto row3 = transform.GetRow(2);

        return AZStd::string::format(
            "[%f, %f, %f, %f]\n"
            "[%f, %f, %f, %f]\n"
            "[%f, %f, %f, %f]",
            static_cast<float>(row1.GetX()), static_cast<float>(row1.GetY()), static_cast<float>(row1.GetZ()), static_cast<float>(row1.GetW()),
            static_cast<float>(row2.GetX()), static_cast<float>(row2.GetY()), static_cast<float>(row2.GetZ()), static_cast<float>(row2.GetW()),
            static_cast<float>(row3.GetX()), static_cast<float>(row3.GetY()), static_cast<float>(row3.GetZ()), static_cast<float>(row3.GetW())
        );
    }

    AZStd::string ToString(const AZ::Matrix3x3& matrix33)
    {
        auto row1 = matrix33.GetRow(0);
        auto row2 = matrix33.GetRow(1);
        auto row3 = matrix33.GetRow(2);

        return AZStd::string::format(
            "[%f, %f, %f]\n"
            "[%f, %f, %f]\n"
            "[%f, %f, %f]",
            static_cast<float>(row1.GetX()), static_cast<float>(row1.GetY()), static_cast<float>(row1.GetZ()),
            static_cast<float>(row2.GetX()), static_cast<float>(row2.GetY()), static_cast<float>(row2.GetZ()),
            static_cast<float>(row3.GetX()), static_cast<float>(row3.GetY()), static_cast<float>(row3.GetZ())
        );
    }

    AZStd::string ToString(const AZ::Matrix4x4& matrix44)
    {
        auto row1 = matrix44.GetRow(0);
        auto row2 = matrix44.GetRow(1);
        auto row3 = matrix44.GetRow(2);
        auto row4 = matrix44.GetRow(3);

        return AZStd::string::format(
            "[%f, %f, %f %f]\n"
            "[%f, %f, %f %f]\n"
            "[%f, %f, %f %f]\n",
            "[%f, %f, %f %f]",
            static_cast<float>(row1.GetX()), static_cast<float>(row1.GetY()), static_cast<float>(row1.GetZ()), static_cast<float>(row1.GetW()),
            static_cast<float>(row2.GetX()), static_cast<float>(row2.GetY()), static_cast<float>(row2.GetZ()), static_cast<float>(row2.GetW()),
            static_cast<float>(row3.GetX()), static_cast<float>(row3.GetY()), static_cast<float>(row3.GetZ()), static_cast<float>(row3.GetW()),
            static_cast<float>(row4.GetX()), static_cast<float>(row4.GetY()), static_cast<float>(row4.GetZ()), static_cast<float>(row4.GetW())
        );
    }

    AZStd::string ToString(const AZ::Color& color)
    {
        return AZStd::string::format("(R:%d, G:%d, B:%d A:%d)", color.GetR8(), color.GetG8(), color.GetB8(), color.GetA8());
    }
}