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
#ifndef AZCORE_MATH_QUATERNION_H
#define AZCORE_MATH_QUATERNION_H 1

#include <AzCore/Math/Vector3.h>

namespace AZ
{
    class Quaternion
    {
    public:
        //AZ_DECLARE_CLASS_POOL_ALLOCATOR(Quaternion);
        AZ_TYPE_INFO(Quaternion, "{73103120-3DD3-4873-BAB3-9713FA2804FB}")

        //===============================================================
        // Constructors
        //===============================================================

        ///Default constructor, components are uninitialized
        Quaternion()    { }

        ///Constructs quaternion with all components set to the same specified value
        explicit Quaternion(const VectorFloat& x);

        explicit Quaternion(const VectorFloat& x, const VectorFloat& y, const VectorFloat& z, const VectorFloat& w);

        ///For internal use only, arrangement of values in SIMD type is not guaranteed.
        explicit Quaternion(SimdVectorType value);

        static AZ_MATH_FORCE_INLINE const Quaternion CreateIdentity()               { float xyzw[] = {0.0f, 0.0f, 0.0f, 1.0f}; return CreateFromFloat4(xyzw); }
        static const Quaternion CreateZero();

        ///Sets components from an array of 4 floats, stored in xyzw order
        static const Quaternion CreateFromFloat4(const float* values);

        ///Sets components using a Vector3 for the imaginary part and setting the real part to zero
        static AZ_MATH_FORCE_INLINE const Quaternion CreateFromVector3(const Vector3& v) { Quaternion result; result.Set(v, VectorFloat::CreateZero()); return result; }

        ///Sets components using a Vector3 for the imaginary part and a VectorFloat for the real part
        static const Quaternion CreateFromVector3AndValue(const Vector3& v, const VectorFloat& w);

        ///Sets the quaternion to be a rotation around a specified axis.
        /*@{*/
        static const Quaternion CreateRotationX(float angle);
        static const Quaternion CreateRotationY(float angle);
        static const Quaternion CreateRotationZ(float angle);
        /*@}*/

        /// Creates a quaternion using the rotation part of a Transform matrix.
        /// \note If the transform has a scale other than (1,1,1) be sure to extract the scale first
        /// with AZ::Transform::ExtractScale or ::ExtractScaleExact. If you simply want a rotation/orientation
        /// from a Transform, prefer the CreateRotationFrom**** versions.
        static const Quaternion CreateFromTransform(const class Transform& t);

        /// Creates a rotation in quaternion form using the rotation part of a Transform matrix.
        /// \note Transform is passed by value intentionally as ExtractScale is called internally to remove any
        /// scale from the transform. CreateRotationFromScaledTransform is useful to call if you cannot guarantee
        /// the incoming transform will have unit scale (e.g. Entity transforms often may contains scale).
        static const Quaternion CreateRotationFromScaledTransform(Transform t);

        /// Creates a rotation in quaternion form using the rotation part of a Transform matrix.
        /// \note The incoming Transform must have unit scale when calling this function. If the caller knows
        /// for certain the transform has no scale applied, this is safe to call, otherwise prefer CreateRotationFromScaledTransform.
        static const Quaternion CreateRotationFromUnscaledTransform(const Transform& t);

        ///Creates a quaternion from a Matrix3x3
        static const Quaternion CreateFromMatrix3x3(const class Matrix3x3& m);

        ///Creates a quaternion using the rotation part of a Matrix4x4
        static const Quaternion CreateFromMatrix4x4(const class Matrix4x4& m);

        static const Quaternion CreateFromAxisAngle(const Vector3& axis, const VectorFloat& angle);

        static const Quaternion CreateFromAxisAngleExact(const Vector3& axis, const VectorFloat& angle);

        static const Quaternion CreateShortestArc(const Vector3& v1, const Vector3& v2);

        //===============================================================
        // Store
        //===============================================================

        ///Stores the vector to an array of 4 floats. The floats need only be 4 byte aligned, 16 byte alignment is NOT
        ///required.
        void StoreToFloat4(float* values) const;

        //===============================================================
        // Component access
        //===============================================================

        const VectorFloat GetX() const;
        const VectorFloat GetY() const;
        const VectorFloat GetZ() const;
        const VectorFloat GetW() const;
        void SetX(const VectorFloat& x);
        void SetY(const VectorFloat& y);
        void SetZ(const VectorFloat& z);
        void SetW(const VectorFloat& w);

        // We recommend using GetX,Y,Z,W. GetElement can be slower.
        const VectorFloat GetElement(int index) const;
        // We recommend using SetX,Y,Z,W. SetElement can be slower.
        void SetElement(int index, const VectorFloat& v);

        /**
         * Indexed access using operator(). It's just for convenience.
         * \note This can be slower than using GetX,GetY, etc.
         */
        /*@{*/
        AZ_MATH_FORCE_INLINE float operator()(int index) const  { return GetElement(index); }
        /*@}*/

        void Set(const VectorFloat& x);
        void Set(const VectorFloat& x, const VectorFloat& y, const VectorFloat& z, const VectorFloat& w);
        void Set(const Vector3& v, const VectorFloat& w);
        void Set(const float values[]);

        //===============================================================
        // Inverse/conjugate
        //===============================================================

        ///The conjugate of a quaternion is (-x, -y, -z, w)
        const Quaternion GetConjugate() const;

        /**
         * For a unit quaternion, the inverse is just the conjugate. This function assumes
         * a unit quaternion.
         */
        /*@{*/
        AZ_MATH_FORCE_INLINE const Quaternion GetInverseFast() const { return GetConjugate(); }
        void InvertFast() { *this = GetInverseFast(); }
        /*@}*/

        /**
         * This is the inverse for any quaternion, not just unit quaternions.
         */
        /*@{*/
        AZ_MATH_FORCE_INLINE const Quaternion GetInverseFull() const { return GetConjugate() / GetLengthSq(); }
        void InvertFull() { *this = GetInverseFull(); }
        /*@}*/

        //===============================================================
        // Dot product
        //===============================================================

        const VectorFloat Dot(const Quaternion& q) const;

        //===============================================================
        // Length/normalizing
        //===============================================================

        AZ_MATH_FORCE_INLINE const VectorFloat GetLengthSq() const      { return Dot(*this); }

        ///Returns length of the quaternion, medium speed and medium accuracy, typically uses a refined estimate
        const VectorFloat GetLength() const;
        ///Returns length of the quaternion, fast but low accuracy, uses raw estimate instructions
        const VectorFloat GetLengthApprox() const;
        ///Returns length of the quaternion, full accuracy
        const VectorFloat GetLengthExact() const;

        ///Returns 1/length, medium speed and medium accuracy, typically uses a refined estimate
        const VectorFloat GetLengthReciprocal() const;
        ///Returns 1/length of the quaternion, fast but low accuracy, uses raw estimate instructions
        const VectorFloat GetLengthReciprocalApprox() const;
        ///Returns 1/length of the quaternion, full accuracy
        const VectorFloat GetLengthReciprocalExact() const;

        ///Returns normalized quaternion, medium speed and medium accuracy, typically uses a refined estimate
        const Quaternion GetNormalized() const;
        ///Returns normalized quaternion, fast but low accuracy, uses raw estimate instructions
        const Quaternion GetNormalizedApprox() const;
        ///Returns normalized quaternion, full accuracy
        const Quaternion GetNormalizedExact() const;

        ///Normalizes the quaternion in-place, medium speed and medium accuracy, typically uses a refined estimate
        AZ_MATH_FORCE_INLINE void Normalize()       { *this = GetNormalized(); }
        ///Normalizes the quaternion in-place, fast but low accuracy, uses raw estimate instructions
        AZ_MATH_FORCE_INLINE void NormalizeApprox() { *this = GetNormalizedApprox(); }
        ///Normalizes the quaternion in-place, full accuracy
        AZ_MATH_FORCE_INLINE void NormalizeExact()  { *this = GetNormalizedExact(); }

        ///Normalizes the vector in-place and returns the previous length. This takes a few more instructions than calling
        ///just Normalize().
        /*@{*/
        const VectorFloat NormalizeWithLength();
        const VectorFloat NormalizeWithLengthApprox();
        const VectorFloat NormalizeWithLengthExact();
        /*@}*/

        //===============================================================
        // Interpolation
        //===============================================================

        /**
         * Linearly interpolate towards a destination quaternion.
         * @param[in] dest The quaternion to interpolate towards.
         * @param[in] t Normalized interpolation value where 0.0 represents the current and 1.0 the destination value.
         * @result The interpolated quaternion at the given interpolation point.
         */
        const Quaternion Lerp(const Quaternion& dest, const VectorFloat& t) const;

        /**
         * Linearly interpolate towards a destination quaternion, and normalize afterwards.
         * @param[in] dest The quaternion to interpolate towards.
         * @param[in] t Normalized interpolation value where 0.0 represents the current and 1.0 the destination value.
         * @result The interpolated and normalized quaternion at the given interpolation point.
         */
        const Quaternion NLerp(const Quaternion& dest, const VectorFloat& t) const;

        /**
         * Spherical linear interpolation. Result is NOT normalized.
         */
        const Quaternion Slerp(const Quaternion& dest, float t) const;

        /**
         * Spherical linear interpolation, but with in/out tangent quaternions.
         */
        const Quaternion Squad(const Quaternion& dest, const Quaternion& in, const Quaternion& out, float t) const;

        //===============================================================
        // Comparison
        //===============================================================

        /**
         * Checks if the quaternion is close to another quaternion with a given floating point tolerance
         */
        bool IsClose(const Quaternion& q, const VectorFloat& tolerance = g_simdTolerance) const;

        bool IsIdentity(const VectorFloat& tolerance = g_simdTolerance) const;

        AZ_MATH_FORCE_INLINE bool IsZero(const VectorFloat& tolerance = g_fltEps) const { return IsClose(CreateZero(), tolerance); }

        //===============================================================
        // Standard operators
        //===============================================================
#if defined(AZ_SIMD)
        AZ_MATH_FORCE_INLINE Quaternion(const Quaternion& q)
            : m_value(q.m_value)   {}
        AZ_MATH_FORCE_INLINE Quaternion& operator= (const Quaternion& rhs)          { m_value = rhs.m_value; return *this;  }
#endif

        const Quaternion operator-() const;
        const Quaternion operator+(const Quaternion& q) const;
        const Quaternion operator-(const Quaternion& q) const;
        const Quaternion operator*(const Quaternion& q) const;
        const Quaternion operator*(const VectorFloat& multiplier) const;
        const Quaternion operator/(const VectorFloat& divisor) const;

        AZ_MATH_FORCE_INLINE Quaternion& operator+=(const Quaternion& q)        { *this = *this + q; return *this; }
        AZ_MATH_FORCE_INLINE Quaternion& operator-=(const Quaternion& q)        { *this = *this - q; return *this; }
        AZ_MATH_FORCE_INLINE Quaternion& operator*=(const Quaternion& q)        { *this = *this * q; return *this; }
        AZ_MATH_FORCE_INLINE Quaternion& operator*=(const VectorFloat& multiplier)  { *this = *this * multiplier; return *this; }
        AZ_MATH_FORCE_INLINE Quaternion& operator/=(const VectorFloat& divisor)     { *this = *this / divisor; return *this; }

        bool operator==(const Quaternion& rhs) const;
        bool operator!=(const Quaternion& rhs) const;

        /**
         * Transforms a vector. The multiplication order is defined to be q*v, which matches the matrix multiplication order.
         */
        const Vector3 operator*(const Vector3& v) const;

        //-------------------------------------------------------
        // Euler transforms
        //-------------------------------------------------------

        //! Create, from the quaternion, a set of Euler angles of rotations around first z-axis,
        //!        then y-axis and then x-axis.
        //! @return Vector3 A vector containing component-wise rotation angles in degrees.
        AZ::Vector3 GetEulerDegrees() const;

        //! Create, from the quaternion, a set of Euler angles of rotations around first z-axis,
        //!        then y-axis and then x-axis.
        //! @return Vector3 A vector containing component-wise rotation angles in radians.
        AZ::Vector3 GetEulerRadians() const;

        //! An alias for SetFromEulerRadiansAprox
        //! @param eulerRadians A vector containing component-wise rotation angles in radians.
        void SetFromEulerRadians(const AZ::Vector3& eulerRadians);

        //! Set the quaternion from composite rotations of Euler angles in the order of
        //!        rotation around first z-axis, then y-axis and then x-axis.
        //!        Faster but less accurate than SetFromEulerRadiansExact
        //! @param eulerRadians A vector containing component-wise rotation angles in radians.
        void SetFromEulerRadiansApprox(const AZ::Vector3& eulerRadians);

        //! Set the quaternion from composite rotations of Euler angles in the order of
        //!        rotation around first z-axis, then y-axis and then x-axis.
        //!        Slower but more accurate than SetFromEulerRadiansApprox
        //! @param eulerRadians A vector containing component-wise rotation angles in radians.
        void SetFromEulerRadiansExact(const AZ::Vector3& eulerRadians);

        //! An alias for SetFrom EulerDegreesApprox
        //! @param eulerDegrees A vector containing component-wise rotation angles in degrees.
        void SetFromEulerDegrees(const AZ::Vector3& eulerDegrees);

        //! Set the quaternion from composite rotations of Euler angles in the order of
        //!        rotation around first z-axis, then y-axis and then x-axis.
        //!        Faster but less accurate than SetFromEulerDegreesExact
        //! @param eulerDegrees A vector containing component-wise rotation angles in degrees.
        void SetFromEulerDegreesApprox(const AZ::Vector3& eulerDegrees);

        //! Set the quaternion from composite rotations of Euler angles in the order of
        //!        rotation around first z-axis, then y-axis and then x-axis.
        //!        Slower but more accurate than SetFromEulerDegreesApprox
        //! @param eulerDegrees A vector containing component-wise rotation angles in degrees.
        void SetFromEulerDegreesExact(const AZ::Vector3& eulerDegrees);

        //! Populate axis and angle of rotation from Quaternion
        //! @param[out] outAxis A Vector3 defining the rotation axis.
        //! @param[out] outAngle A float rotation angle around the axis in radians.
        void ConvertToAxisAngle(AZ::Vector3& outAxis, float& outAngle) const;

        //===============================================================
        // Miscellaneous
        //===============================================================

        //TODO: write proper simd version
        AZ_MATH_FORCE_INLINE const Vector3 GetImaginary() const     { return Vector3(GetX(), GetY(), GetZ()); }

        /**
         * Return angle in radians.
         */
        VectorFloat GetAngle() const;

        AZ_MATH_FORCE_INLINE bool IsFinite() const { return IsFiniteFloat(GetX()) && IsFiniteFloat(GetY()) && IsFiniteFloat(GetZ()) && IsFiniteFloat(GetW()); }

    private:

        ///Takes the absolute value of each component of
        const Quaternion GetAbs() const;

        friend const Quaternion operator*(const VectorFloat& multiplier, const Quaternion& rhs);

        #if defined(AZ_SIMD)
        SimdVectorType m_value;
        #else
        AZ_ALIGN(float m_x, 16);        //align just to be consistent with simd implementations
        float m_y, m_z, m_w;
        #endif
    };

    //! Non-member functionality belonging to the AZ namespace
    //!
    //! Create, from a quaternion, a set of Euler angles of rotations around first z-axis,
    //!        then y-axis and then x-axis.
    //! @param q a quaternion representing the rotation
    //! @return A vector containing component-wise rotation angles in degrees.
    AZ::Vector3 ConvertQuaternionToEulerDegrees(const AZ::Quaternion& q);

    //! Create, from a quaternion, a set of Euler angles of rotations around first z-axis,
    //!        then y-axis and then x-axis.
    //! @param q a quaternion representing the rotation
    //! @return A vector containing component-wise rotation angles in radians.
    AZ::Vector3 ConvertQuaternionToEulerRadians(const AZ::Quaternion& q);

    //! An alias for ConvertEulerRadiansToQuaternionApprox
    //! @param eulerRadians A vector containing component-wise rotation angles in radians.
    //! @return a quaternion made from composition of rotations around principle axes.
    AZ::Quaternion ConvertEulerRadiansToQuaternion(const AZ::Vector3& eulerRadians);

    //! Create a quaternion from composite rotations of Euler angles in the order of
    //!        rotation around first z-axis, then y-axis and then x-axis.
    //!        Faster but less accurate than ConvertEulerRadiansToQuaternionExact
    //! @param eulerRadians A vector containing component-wise rotation angles in radians.
    //! @return a quaternion made from composition of rotations around principle axes.
    AZ::Quaternion ConvertEulerRadiansToQuaternionApprox(const AZ::Vector3& eulerRadians);

    //! Create a quaternion from composite rotations of Euler angles in the order of
    //!        rotation around first z-axis, then y-axis and then x-axis.
    //!        Slower but more accurate than ConvertEulerRadiansToQuaternionExact
    //! @param eulerRadians A vector containing component-wise rotation angles in radians.
    //! @return a quaternion made from composition of rotations around principle axes.
    AZ::Quaternion ConvertEulerRadiansToQuaternionExact(const AZ::Vector3& eulerRadians);

    //! An alias for ConvertEulerDegreesToQuaternionApprox
    //! @param eulerDegrees A vector containing component-wise rotation angles in degrees.
    //! @return a quaternion made from composition of rotations around principle axes.
    AZ::Quaternion ConvertEulerDegreesToQuaternion(const AZ::Vector3& eulerDegrees);

    //! Create a quaternion from composite rotations of Euler angles in the order of
    //!        rotation around first z-axis, then y-axis and then x-axis.
    //!        Faster but less accurate than ConvertEulerDegreesToQuaternionExact
    //! @param eulerDegrees A vector containing component-wise rotation angles in degrees.
    //! @return a quaternion made from composition of rotations around principle axes.
    AZ::Quaternion ConvertEulerDegreesToQuaternionApprox(const AZ::Vector3& eulerDegrees);

    //! Create a quaternion from composite rotations of Euler angles in the order of
    //!        rotation around first z-axis, then y-axis and then x-axis.
    //!        Slower but more accurate than ConvertEulerDegreesToQuaternionExact
    //! @param eulerDegrees A vector containing component-wise rotation angles in degrees.
    //! @return a quaternion made from composition of rotations around principle axes.
    AZ::Quaternion ConvertEulerDegreesToQuaternionExact(const AZ::Vector3& eulerDegrees);

    //! Populate axis and angle of rotation from Quaternion
    //! @param[in] quat A source quaternion
    //! @param[out] outAxis A Vector3 defining the rotation axis.
    //! @param[out] outAngle A float rotation angle around the axis in radians.
    void ConvertQuaternionToAxisAngle(const AZ::Quaternion& quat, AZ::Vector3& outAxis, float& outAngle);
}

#if AZ_TRAIT_USE_PLATFORM_SIMD
    #include <AzCore/Math/Internal/QuaternionWin32.inl>
#else
    #include <AzCore/Math/Internal/QuaternionFpu.inl>
#endif

#endif
#pragma once
