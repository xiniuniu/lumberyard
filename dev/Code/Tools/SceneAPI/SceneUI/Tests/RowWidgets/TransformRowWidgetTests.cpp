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

#include <AzTest/AzTest.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>
#include <SceneAPI/SceneUI/RowWidgets/TransformRowWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneUI
        {
            class TransformRowWidgetTest
                : public ::testing::Test
            {
            public:
                ExpandedTransform m_expanded;
                Transform m_transform;

                Vector3 m_translation = Vector3(10.0f, 20.0f, 30.0f);
                Vector3 m_rotation = Vector3(45.0f, 90.0f, 135.0f);
                Vector3 m_scale = Vector3(2.0f, 3.0f, 4.0f);

                void Compare(const Transform& lhs, const Transform& rhs, float error)
                {
                    for (int row = 0; row < 3; ++row)
                    {
                        for (int column = 0; column < 4; ++column)
                        {
                            EXPECT_NEAR(lhs.GetElement(row, column), lhs.GetElement(row, column), error);
                        }
                    }
                }
            };

            TEST_F(TransformRowWidgetTest, GetTranslation_TranslationInMatrix_TranslationCanBeRetrievedDirectly)
            {
                m_transform = Transform::CreateTranslation(m_translation);
                m_expanded.SetTransform(m_transform);

                const Vector3& returned = m_expanded.GetTranslation();
                EXPECT_NEAR(m_translation.GetX(), returned.GetX(), 0.1f);
                EXPECT_NEAR(m_translation.GetY(), returned.GetY(), 0.1f);
                EXPECT_NEAR(m_translation.GetZ(), returned.GetZ(), 0.1f);
            }

            TEST_F(TransformRowWidgetTest, GetTranslation_TranslationInMatrix_TranslationCanBeRetrievedFromTransform)
            {
                m_transform = Transform::CreateTranslation(m_translation);
                m_expanded.SetTransform(m_transform);

                Transform rebuild;
                m_expanded.GetTransform(rebuild);
                Vector3 returned = rebuild.GetTranslation();
                EXPECT_NEAR(m_translation.GetX(), returned.GetX(), 0.1f);
                EXPECT_NEAR(m_translation.GetY(), returned.GetY(), 0.1f);
                EXPECT_NEAR(m_translation.GetZ(), returned.GetZ(), 0.1f);
            }

            TEST_F(TransformRowWidgetTest, GetRotation_RotationInMatrix_RotationCanBeRetrievedDirectly)
            {
                m_transform = AZ::ConvertEulerDegreesToTransform(m_rotation);
                m_expanded.SetTransform(m_transform);

                const Vector3& returned = m_expanded.GetRotation();
                EXPECT_NEAR(m_rotation.GetX(), returned.GetX(), 1.0f);
                EXPECT_NEAR(m_rotation.GetY(), returned.GetY(), 1.0f);
                EXPECT_NEAR(m_rotation.GetZ(), returned.GetZ(), 1.0f);
            }

            TEST_F(TransformRowWidgetTest, GetRotation_RotationInMatrix_RotationCanBeRetrievedFromTransform)
            {
                m_transform.SetFromEulerDegrees(m_rotation);
                m_expanded.SetTransform(m_transform);

                Transform rebuild;
                m_expanded.GetTransform(rebuild);
                Vector3 returned = rebuild.GetEulerDegrees();
                EXPECT_NEAR(m_rotation.GetX(), returned.GetX(), 1.0f);
                EXPECT_NEAR(m_rotation.GetY(), returned.GetY(), 1.0f);
                EXPECT_NEAR(m_rotation.GetZ(), returned.GetZ(), 1.0f);
            }

            TEST_F(TransformRowWidgetTest, GetScale_ScaleInMatrix_ScaleCanBeRetrievedDirectly)
            {
                m_transform = Transform::CreateScale(m_scale);
                m_expanded.SetTransform(m_transform);

                const Vector3& returned = m_expanded.GetScale();
                EXPECT_NEAR(m_scale.GetX(), returned.GetX(), 0.1f);
                EXPECT_NEAR(m_scale.GetY(), returned.GetY(), 0.1f);
                EXPECT_NEAR(m_scale.GetZ(), returned.GetZ(), 0.1f);
            }

            TEST_F(TransformRowWidgetTest, GetScale_ScaleInMatrix_ScaleCanBeRetrievedFromTransform)
            {
                m_transform = Transform::CreateScale(m_scale);
                m_expanded.SetTransform(m_transform);

                Transform rebuild;
                m_expanded.GetTransform(rebuild);
                Vector3 returned = rebuild.RetrieveScaleExact();
                EXPECT_NEAR(m_scale.GetX(), returned.GetX(), 0.1f);
                EXPECT_NEAR(m_scale.GetY(), returned.GetY(), 0.1f);
                EXPECT_NEAR(m_scale.GetZ(), returned.GetZ(), 0.1f);
            }

            TEST_F(TransformRowWidgetTest, GetTransform_RotateAndTranslateInMatrix_ReconstructedTransformMatchesOriginal)
            {
                Quaternion quaternion = AZ::ConvertEulerDegreesToQuaternion(m_rotation);
                m_transform = Transform::CreateFromQuaternionAndTranslation(quaternion, m_translation);
                m_expanded.SetTransform(m_transform);

                Transform rebuild;
                m_expanded.GetTransform(rebuild);

                Compare(m_transform, rebuild, 0.001f);
            }
            
            TEST_F(TransformRowWidgetTest, GetTransform_RotateTranslateAndScaleInMatrix_ReconstructedTransformMatchesOriginal)
            {
                Quaternion quaternion = AZ::ConvertEulerDegreesToQuaternion(m_rotation);
                m_transform = Transform::CreateFromQuaternionAndTranslation(quaternion, m_translation);
                m_transform.MultiplyByScale(m_scale);
                m_expanded.SetTransform(m_transform);

                Transform rebuild;
                m_expanded.GetTransform(rebuild);

                Compare(m_transform, rebuild, 0.001f);
            }
        } // namespace SceneUI
    } // namespace SceneAPI
} // namespace AZ