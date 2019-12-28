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
#ifndef AZCORE_MATH_OBB_H
#define AZCORE_MATH_OBB_H 1

#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Matrix3x3.h>

namespace AZ
{
    class Aabb;

    /**
     * An oriented bounding box.
     */
    class Obb
    {
        friend void MathReflect(class BehaviorContext&);
        friend void MathReflect(class SerializeContext&);
    public:
        struct Axis
        {
            Vector3 m_axis;
            float m_halfLength;
        };

        AZ_TYPE_INFO(Obb, "{004abd25-cf14-4eb3-bd41-022c247c07fa}");
        //AZ_DECLARE_CLASS_POOL_ALLOCATOR(Obb);

        Obb()       { }

        static const Obb CreateFromPositionAndAxes(const Vector3& position,
            const Vector3& axisX, float halfX,
            const Vector3& axisY, float halfY,
            const Vector3& axisZ, float halfZ)
        {
            Obb result;
            result.m_position = position;
            result.m_axes[0].m_axis = axisX;
            result.m_axes[0].m_halfLength = halfX;
            result.m_axes[1].m_axis = axisY;
            result.m_axes[1].m_halfLength = halfY;
            result.m_axes[2].m_axis = axisZ;
            result.m_axes[2].m_halfLength = halfZ;
            return result;
        }

        ///Converts an AABB into an OBB
        static const Obb CreateFromAabb(const Aabb& aabb);

        const Vector3& GetPosition() const         { return m_position; }
        Matrix3x3 GetOrientation() const
        {
            return Matrix3x3::CreateFromColumns(
                m_axes[0].m_axis,
                m_axes[1].m_axis,
                m_axes[2].m_axis);
        }

        const Vector3& GetAxisX() const            { return m_axes[0].m_axis; }
        const Vector3& GetAxisY() const            { return m_axes[1].m_axis; }
        const Vector3& GetAxisZ() const            { return m_axes[2].m_axis; }
        const Vector3& GetAxis(int index) const
        {
            AZ_Assert((index >= 0) && (index < 3), "Invalid axis");
            return m_axes[index].m_axis;
        }
        float GetHalfLengthX() const               { return m_axes[0].m_halfLength; }
        float GetHalfLengthY() const               { return m_axes[1].m_halfLength; }
        float GetHalfLengthZ() const               { return m_axes[2].m_halfLength; }
        float GetHalfLength(int index) const
        {
            AZ_Assert((index >= 0) && (index < 3), "Invalid axis");
            return m_axes[index].m_halfLength;
        }

        void SetPosition(const Vector3& position)  { m_position = position; }
        void SetAxisX(const Vector3& axis)         { m_axes[0].m_axis = axis; }
        void SetAxisY(const Vector3& axis)         { m_axes[1].m_axis = axis; }
        void SetAxisZ(const Vector3& axis)         { m_axes[2].m_axis = axis; }
        void SetAxis(int index, const Vector3& axis)
        {
            AZ_Assert((index >= 0) && (index < 3), "Invalid axis");
            m_axes[index].m_axis = axis;
        }
        void SetHalfLengthX(float halfLength)          { m_axes[0].m_halfLength = halfLength; }
        void SetHalfLengthY(float halfLength)          { m_axes[1].m_halfLength = halfLength; }
        void SetHalfLengthZ(float halfLength)          { m_axes[2].m_halfLength = halfLength; }
        void SetHalfLength(int index, float halfLength)
        {
            AZ_Assert((index >= 0) && (index < 3), "Invalid axis");
            m_axes[index].m_halfLength = halfLength;
        }

        bool IsFinite() const;

        bool operator==(const Obb& rhs) const;
        bool operator!=(const Obb& rhs) const;

    private:
        Vector3 m_position;
        Axis m_axes[3];
    };

    const Obb operator*(const class Transform& transform, const Obb& obb);
}

#endif
#pragma once
