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

#include <SceneAPI/SceneData/GraphData/MeshVertexTangentData.h>

namespace AZ
{
    namespace SceneData
    {
        namespace GraphData
        {

            size_t MeshVertexTangentData::GetCount() const
            {
                return m_tangents.size();
            }


            const AZ::Vector4& MeshVertexTangentData::GetTangent(size_t index) const
            {
                AZ_Assert(index < m_tangents.size(), "Invalid index %i for mesh tangents.", index);
                return m_tangents[index];
            }


            void MeshVertexTangentData::ReserveContainerSpace(size_t numVerts)
            {
                m_tangents.reserve(numVerts);
            }


            void MeshVertexTangentData::Resize(size_t numVerts)
            {
                m_tangents.resize(numVerts);
            }


            void MeshVertexTangentData::AppendTangent(const AZ::Vector4& tangent)
            {
                m_tangents.push_back(tangent);
            }


            void MeshVertexTangentData::SetTangent(size_t vertexIndex, const AZ::Vector4& tangent)
            {
                m_tangents[vertexIndex] = tangent;
            }


            void MeshVertexTangentData::SetTangentSetIndex(size_t setIndex)
            {
                m_setIndex = setIndex;
            }


            size_t MeshVertexTangentData::GetTangentSetIndex() const
            {
                return m_setIndex;
            }


            AZ::SceneAPI::DataTypes::TangentSpace MeshVertexTangentData::GetTangentSpace() const
            { 
                return m_tangentSpace;
            }


            void MeshVertexTangentData::SetTangentSpace(AZ::SceneAPI::DataTypes::TangentSpace space)
            { 
                m_tangentSpace = space;
            }

        } // GraphData
    } // SceneData
} // AZ