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

#pragma once

#include <AzCore/Math/Vector4.h>
#include <ScriptCanvas/Core/NodeFunctionGeneric.h>
#include <ScriptCanvas/Data/NumericData.h>
#include <ScriptCanvas/Libraries/Math/MathNodeUtilities.h>

namespace ScriptCanvas
{
    namespace TransformNodes
    {
        using namespace Data;
        using namespace MathNodeUtilities;
        
        AZ_INLINE std::tuple<Vector3Type, TransformType> ExtractScale(TransformType source)
        {
            auto scale(source.ExtractScale());
            return std::make_tuple( scale, source );
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_MULTI_RESULTS_NODE(ExtractScale, "Math/Transform", "{8DFE5247-0950-4CD1-87E6-0CAAD42F1637}", "returns a vector which is the length of the scale components, and a transform with the scale extracted ", "Source", "Scale", "Extracted");
        
        AZ_INLINE TransformType FromColumns(Vector3Type c0, Vector3Type c1, Vector3Type c2, Vector3Type c3)
        {
            return TransformType::CreateFromColumns(c0, c1, c2, c3);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromColumns, "Math/Transform", "{0954EBE9-438D-4D02-BAD2-A2C23ABBD89D}", "returns the transform from the columns", "Column 0", "Column 1", "Column 2", "Column 3");

        AZ_INLINE TransformType FromDiagonal(Vector3Type diagonal)
        {
            return TransformType::CreateDiagonal(diagonal);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromDiagonal, "Math/Transform", "{8CD45F74-0FA6-4E8A-81AF-5541FBC4BB50}", "returns a transform with a diagonal matrix and the translation set to zero", "Diagonal");

        AZ_INLINE TransformType FromMatrix3x3(Matrix3x3Type source)
        {
            return TransformType::CreateFromMatrix3x3(source);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromMatrix3x3, "Math/Transform", "{DA430502-CF75-41BA-BA41-6701994EFB64}", "returns a transform with from 3x3 matrix and with the translation set to zero", "Source");

        AZ_INLINE TransformType FromMatrix3x3AndTranslation(Matrix3x3Type matrix, Vector3Type translation)
        {
            return TransformType::CreateFromMatrix3x3AndTranslation(matrix, translation);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromMatrix3x3AndTranslation, "Math/Transform", "{AD0725EB-0FF0-4F99-A45F-C3F8CBABF11D}", "returns a transform from the 3x3 matrix and the translation", "Matrix", "Translation");

        AZ_INLINE TransformType FromRotation(QuaternionType rotation)
        {
            return TransformType::CreateFromQuaternion(rotation);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromRotation, "Math/Transform", "{8BBF4F22-EA7D-4E7B-81FD-7D11CA237BA6}", "returns a transform from the rotation and with the translation set to zero", "Source");

        AZ_INLINE TransformType FromRotationAndTranslation(QuaternionType rotation, Vector3Type translation)
        {
            return TransformType::CreateFromQuaternionAndTranslation(rotation, translation);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromRotationAndTranslation, "Math/Transform", "{99A4D55D-6EFB-4E24-8113-F5B46DE3A194}", "returns a transform from the rotation and the translation", "Rotation", "Translation");

        AZ_INLINE TransformType FromRows(Vector4Type r0, Vector4Type r1, Vector4Type r2)
        {
            return TransformType::CreateFromRows(r0, r1, r2);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromRows, "Math/Transform", "{39BB92B1-7C34-46FE-AB0B-D990C59EA3F6}", "returns the transform from the rows", "Row 0", "Row 1", "Row 2");

        AZ_INLINE TransformType FromScale(Vector3Type scale)
        {
            return TransformType::CreateScale(scale);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromScale, "Math/Transform", "{4B6454BC-015C-41BB-9C78-34ADBCF70187}", "returns a scale matrix and the translation set to zero", "Scale");

        AZ_INLINE TransformType FromTranslation(Vector3Type translation)
        {
            return TransformType::CreateTranslation(translation);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromTranslation, "Math/Transform", "{A60083C8-AEEC-456E-A3F5-75D0E0D094E1}", "returns a translation matrix and the rotation set to zero", "Translation");

        AZ_INLINE TransformType FromValue(NumberType value)
        {
            return TransformType::CreateFromValue(ToVectorFloat(value));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(FromValue, "Math/Transform", "{F72F2502-783A-489F-9AE4-C3E09E25231B}", "returns a translation with all values set to the source", "Source");

        AZ_INLINE Vector3Type GetColumn(const TransformType& source, NumberType index)
        {
            return source.GetColumn(AZ::GetClamp(aznumeric_cast<int>(index), 0, 3));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(GetColumn, "Math/Transform", "{B1B515E8-BAEC-4E82-8966-E91485385BCB}", "returns the column specified by the index, [0,3]", "Source", "Column");

        template<int t_Index>
        AZ_INLINE void DefaultScale(Node& node) { SetDefaultValuesByIndex<t_Index>::_(node, Data::One()); }

        AZ_INLINE Vector3Type GetRight(const TransformType& source, NumberType scale)
        {
            Vector3Type vector = source.GetBasisX();
            vector.SetLength(aznumeric_cast<float>(scale));
            return vector;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE_WITH_DEFAULTS(GetRight, DefaultScale<1>, "Math/Transform", "{65811752-711F-4566-869E-5AEF53206342}", "returns the right direction vector from the specified transform scaled by a given value (Lumberyard uses Z up, right handed)", "Source", "Scale");

        AZ_INLINE Vector3Type GetForward(const TransformType& source, NumberType scale)
        {
            Vector3Type vector = source.GetBasisY();
            vector.SetLength(aznumeric_cast<float>(scale));
            return vector;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE_WITH_DEFAULTS(GetForward, DefaultScale<1>, "Math/Transform", "{3602a047-9f12-46d4-9648-8f53770c8130}", "returns the forward direction vector from the specified transform scaled by a given value (Lumberyard uses Z up, right handed)", "Source", "Scale");

        AZ_INLINE Vector3Type GetUp(const TransformType& source, NumberType scale)
        {
            Vector3Type vector = source.GetBasisZ();
            vector.SetLength(aznumeric_cast<float>(scale));
            return vector;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE_WITH_DEFAULTS(GetUp, DefaultScale<1>, "Math/Transform", "{F10F52D2-E6F2-4E39-84D5-B4A561F186D3}", "returns the up direction vector from the specified transform scaled by a given value (Lumberyard uses Z up, right handed)", "Source", "Scale");

        AZ_INLINE std::tuple<Vector3Type, Vector3Type, Vector3Type, Vector3Type> GetColumns(const TransformType& source)
        {
            Vector3Type c0, c1, c2, c3;
            source.GetColumns(&c0, &c1, &c2, &c3);
            return std::make_tuple(c0, c1, c2, c3);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_MULTI_RESULTS_NODE(GetColumns, "Math/Transform", "{59D6DCE7-626C-499C-A0C1-9BFD822549A8}", "returns the columns that make up the transform", "Source", "Column 0", "Column 1", "Column 2", "Column 3");

        AZ_INLINE NumberType GetElement(const TransformType& source, NumberType row, NumberType col)
        {
            return source.GetElement(AZ::GetClamp(aznumeric_cast<int>(row), 0, 2), AZ::GetClamp(aznumeric_cast<int>(col), 0, 3));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(GetElement, "Math/Transform", "{F9E2528A-31EA-49C0-8CCB-715EDAB6468A}", "returns the determinant of the upper 3x3 matrix of Source", "Source", "Row", "Column"); AZ_INLINE Vector4Type GetRow(const TransformType& source, NumberType index)
        {
            return source.GetRow(AZ::GetClamp(aznumeric_cast<int>(index), 0, 2));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(GetRow, "Math/Transform", "{31170014-C83A-47D6-A5E0-C31588271198}", "returns the column specified by the index, [0,3]", "Source", "Row");

        AZ_INLINE std::tuple<Vector4Type, Vector4Type, Vector4Type> GetRows(const TransformType source)
        {
            Vector4Type r0, r1, r2;
            source.GetRows(&r0, &r1, &r2);
            return std::make_tuple(r0, r1, r2);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_MULTI_RESULTS_NODE(GetRows, "Math/Transform", "{208672E0-C2F2-448B-8F7D-5542C5298588}", "returns the columns that make up the transform", "Source", "Row 0", "Row 1", "Row 2");

        AZ_INLINE Vector3Type GetTranslation(const TransformType& source)
        {
            return source.GetPosition();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(GetTranslation, "Math/Transform", "{6C2AC46D-C92C-4A64-A2EB-48DA52002B8A}", "returns the translation of Source", "Source");

        AZ_INLINE TransformType InvertOrthogonal(const TransformType& source)
        {
            return source.GetInverseFast();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(InvertOrthogonal, "Math/Transform", "{635F8FD0-6B16-4622-A893-463422D817CF}", "returns the inverse of the source assuming it only contains an orthogonal matrix, faster then InvertSlow, but won't handle scale, or skew.", "Source");

        AZ_INLINE TransformType InvertSlow(const TransformType& source)
        {
            return source.GetInverseFull();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(InvertSlow, "Math/Transform", "{A4AC7041-F0A3-4364-8917-6AE2352F5DD4}", "returns the inverse of the source, event if it is scaled or skewed, slower than InvertOrthogonal. Assumes the last row is (0,0,0,1)", "Source");
        
        AZ_INLINE BooleanType IsClose(const TransformType& a, const TransformType& b, NumberType tolerance)
        {
            return a.IsClose(b, ToVectorFloat(tolerance));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE_WITH_DEFAULTS(IsClose, DefaultToleranceSIMD<2>, "Math/Transform", "{52914912-5C4A-48A5-A675-11CF15B5FB4B}", "returns true if every row of A is within Tolerance of corresponding row in B, else false", "A", "B", "Tolerance");

        AZ_INLINE BooleanType IsFinite(const TransformType& source)
        {
            return source.IsFinite();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(IsFinite, "Math/Transform", "{B7D23934-0101-40B9-80E8-3D88C8580B25}", "returns true if every row of source is finite, else false", "Source");
        
        AZ_INLINE BooleanType IsOrthogonal(const TransformType& source, NumberType tolerance)
        {
            return source.IsOrthogonal(ToVectorFloat(tolerance));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE_WITH_DEFAULTS(IsOrthogonal, DefaultToleranceSIMD<1>, "Math/Transform", "{9A143AC1-ED6B-4D96-939E-40D9F6D01A76}", "returns true if the upper 3x3 matrix of Source is within Tolerance of orthogonal, else false", "Source", "Tolerance");

        AZ_INLINE TransformType ModRotation(const TransformType& source, const Matrix3x3Type& rotation)
        {
            return TransformType::CreateFromMatrix3x3AndTranslation(rotation, source.GetPosition());
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(ModRotation, "Math/Transform", "{ECC408EB-32D7-4DA8-A907-3DB36E8E54A9}", "returns the transform with translation from Source, and rotation from Rotation", "Source", "Rotation");

        AZ_INLINE TransformType ModTranslation(TransformType source, Vector3Type translation)
        {
            source.SetPosition(translation);
            return source;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(ModTranslation, "Math/Transform", "{27BF9798-A6B3-4C2C-B19E-2AF90434090A}", "returns the transform with rotation from Source, and translation from Translation", "Source", "Translation");

        AZ_INLINE Vector3Type Multiply3x3ByVector3(const TransformType& source, const Vector3Type multiplier)
        {
            return source.Multiply3x3(multiplier);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(Multiply3x3ByVector3, "Math/Transform", "{4F2ABFC6-2E93-4A9D-8639-C7967DB318DB}", "returns Source's 3x3 upper matrix post multiplied by Multiplier", "Source", "Multiplier");
        
        AZ_INLINE TransformType MultiplyByScale(TransformType source, Vector3Type scale)
        {
            source.MultiplyByScale(scale);
            return source;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(MultiplyByScale, "Math/Transform", "{90472D62-65A8-40C1-AB08-FA66D793F689}", "returns Source multiplied by the scale matrix produced by Scale", "Source", "Scale");

        AZ_INLINE TransformType MultiplyByTransform(const TransformType& a, const TransformType& b)
        {
            return a * b;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE_DEPRECATED(MultiplyByTransform, "Math/Transform", "{66C3FBB9-498E-4E96-8683-63843F28AFE9}", "This node is deprecated, use Multiply (*), it provides contextual type and slots", "A", "B");

        AZ_INLINE Vector3Type MultiplyByVector3(const TransformType& source, const Vector3Type multiplier)
        {
            return source * multiplier;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(MultiplyByVector3, "Math/Transform", "{147E4714-5028-49A3-A038-6BFB3ED45E0B}", "returns Source post multiplied by Multiplier", "Source", "Multiplier");

        AZ_INLINE Vector4Type MultiplyByVector4(const TransformType& source, const Vector4Type multiplier)
        {
            return source * multiplier;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(MultiplyByVector4, "Math/Transform", "{7E21DC19-C924-4479-817C-A942A52C8B20}", "returns Source post multiplied by Multiplier", "Source", "Multiplier");

        AZ_INLINE TransformType Orthogonalize(const TransformType& source)
        {
            return source.GetOrthogonalized();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(Orthogonalize, "Math/Transform", "{2B4140CD-6E22-44D3-BDB5-309E69FE7CC2}", "returns an orthogonal matrix if the Source is almost orthogonal", "Source");
        
        AZ_INLINE TransformType RotationXDegrees(NumberType degrees)
        {
            return TransformType::CreateRotationX(ToVectorFloat(AZ::DegToRad(aznumeric_caster(degrees))));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(RotationXDegrees, "Math/Transform", "{1C43EF69-D4BD-46BD-BB91-3AC93ECB878C}", "returns a transform representing a rotation Degrees around the X-Axis", "Degrees");

        AZ_INLINE TransformType RotationYDegrees(NumberType degrees)
        {
            return TransformType::CreateRotationY(ToVectorFloat(AZ::DegToRad(aznumeric_caster(degrees))));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(RotationYDegrees, "Math/Transform", "{0426C64C-CC1D-415A-8FA8-2267DE8CA317}", "returns a transform representing a rotation Degrees around the Y-Axis", "Degrees");

        AZ_INLINE TransformType RotationZDegrees(NumberType degrees)
        {
            return TransformType::CreateRotationZ(ToVectorFloat(AZ::DegToRad(aznumeric_caster(degrees))));
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(RotationZDegrees, "Math/Transform", "{F848306A-C07C-4586-B52F-BEEE489045D2}", "returns a transform representing a rotation Degrees around the Z-Axis", "Degrees");

        AZ_INLINE NumberType ToDeterminant3x3(const TransformType& source)
        {
            return source.GetDeterminant3x3();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(ToDeterminant3x3, "Math/Transform", "{1C14AF6C-B533-4893-9E9F-EA0D251D6BE9}", "returns the determinant of the upper 3x3 matrix of Source", "Source");
        
        AZ_INLINE std::tuple<TransformType, TransformType> ToPolarDecomposition(const TransformType& source)
        {
            std::tuple<TransformType, TransformType> result;
            source.GetPolarDecomposition(&std::get<0>(result), &std::get<1>(result));
            return result;
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_MULTI_RESULTS_NODE(ToPolarDecomposition, "Math/Transform", "{85B1FFC9-4863-4195-B13C-9E6FBB75BA33}", "returns only the orthogonal (unitary) and symmetric (hermitian) of the polar decomposition of Source, translation included in orthogonal", "Source", "Orthogonal", "Symmetric");

        AZ_INLINE TransformType ToPolarDecompositionOrthogonalOnly(const TransformType& source)
        {
            return source.GetPolarDecomposition();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(ToPolarDecompositionOrthogonalOnly, "Math/Transform", "{7A60789A-89C7-4BE4-B536-D55A475CD25A}", "returns only the orthogonal (unitary) part of the polar decomposition of Source, translation included", "Source");
        
        AZ_INLINE Vector3Type ToScale(const TransformType& source)
        {
            return source.RetrieveScale();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(ToScale, "Math/Transform", "{063C58AD-F567-464D-A432-F298FE3953A6}", "returns the scale part of the Source, the length of the scale components", "Source");
        
        AZ_INLINE TransformType Transpose(const TransformType& source)
        {
            return source.GetTranspose();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(Transpose, "Math/Transform", "{75FB56CA-3DCE-4FF4-B05A-83101A59EFA9}", "returns the transpose of Source, resets the last column (position)", "Source");

        AZ_INLINE TransformType Transpose3x3(const TransformType& source)
        {
            return source.GetTranspose3x3();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(Transpose3x3, "Math/Transform", "{38DE0F56-6AFD-4790-8340-EFC117DF0EF3}", "returns the transpose of Source, leaves untouched the last column (position)", "Source");

        AZ_INLINE Vector3Type TransposedMultiply3x3(const TransformType& source, Vector3Type vector)
        {
            return source.TransposedMultiply3x3(vector);
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(TransposedMultiply3x3, "Math/Transform", "{9FBA4F8F-86D5-4508-8B4E-16ABE213F608}", "returns the transpose * 3x3 matrix of source", "Source", "Transpose");

        AZ_INLINE TransformType Zero()
        {
            return TransformType::CreateZero();
        }
        SCRIPT_CANVAS_GENERIC_FUNCTION_NODE(Zero, "Math/Transform", "{84956CE9-3717-4256-A9DC-8FE397524168}", "returns a transform with all elements set to zero");

        using Registrar = RegistrarGeneric
            < 
#if ENABLE_EXTENDED_MATH_SUPPORT
            ExtractScaleNode ,
#endif
              FromColumnsNode
            , FromDiagonalNode
            , FromMatrix3x3AndTranslationNode
            , FromMatrix3x3Node
            , FromRotationAndTranslationNode
            , FromRotationNode
            , FromRowsNode
            , FromScaleNode
            , FromTranslationNode
            , FromValueNode
            , GetColumnNode
            , GetColumnsNode
            , GetElementNode
            , GetRowNode
            , GetRowsNode
            , GetTranslationNode
            , GetUpNode
            , GetRightNode
            , GetForwardNode

#if ENABLE_EXTENDED_MATH_SUPPORT
            , InvertOrthogonalNode
#endif

            , InvertSlowNode
            , IsCloseNode
            , IsFiniteNode
            , IsOrthogonalNode

#if ENABLE_EXTENDED_MATH_SUPPORT
            , ModRotationNode
            , ModTranslationNode
            , Multiply3x3ByVector3Node
#endif

            , MultiplyByScaleNode
            , MultiplyByTransformNode
            , MultiplyByVector3Node
            , MultiplyByVector4Node
            , OrthogonalizeNode
            , RotationXDegreesNode
            , RotationYDegreesNode
            , RotationZDegreesNode

#if ENABLE_EXTENDED_MATH_SUPPORT
            , ToDeterminant3x3Node
            , ToPolarDecompositionNode
            , ToPolarDecompositionOrthogonalOnlyNode
#endif

            , ToScaleNode
            , TransposeNode

#if ENABLE_EXTENDED_MATH_SUPPORT
            , Transpose3x3Node
            , TransposedMultiply3x3Node
            , ZeroNode
#endif
            >;

    } // namespace TransformNodes

} // namespace ScriptCanvas

