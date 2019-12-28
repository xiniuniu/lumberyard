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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_CRYENTITYSYSTEM_AFFINEPARTS_H
#define CRYINCLUDE_CRYENTITYSYSTEM_AFFINEPARTS_H

#pragma once

struct AffineParts
{
    Vec3 pos;               //!< Translation components
    Quat rot;           //!< Essential rotation.
    Quat rotScale;  //!< Stretch rotation.
    Vec3 scale;         //!< Stretch factors.
    float fDet;         //!< Sign of determinant.

    /** Decompose matrix to its affnie parts.
    */
    void Decompose(const Matrix44& mat);

    /** Decompose matrix to its affnie parts.
            Assume there`s no stretch rotation.
    */
    void SpectralDecompose(const Matrix44& mat);

    /** Decompose matrix to its affnie parts.
    Assume there`s no stretch rotation.
    */
    void SpectralDecompose(const Matrix34& mat);
};

#endif // CRYINCLUDE_CRYENTITYSYSTEM_AFFINEPARTS_H
