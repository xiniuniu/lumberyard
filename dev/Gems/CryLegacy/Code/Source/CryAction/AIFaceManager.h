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

// Description : take care of playing proper face expresion sequence

#ifndef CRYINCLUDE_CRYACTION_AIFACEMANAGER_H
#define CRYINCLUDE_CRYACTION_AIFACEMANAGER_H
#pragma once

//
//---------------------------------------------------------------------------------------------------

class CAIFaceManager
{
public:
    enum e_ExpressionEvent
    {
        EE_NONE,
        EE_IDLE,
        EE_ALERT,
        EE_COMBAT,
        EE_Count
    };

    CAIFaceManager(IEntity* pEntity);
    ~CAIFaceManager(void);

    static bool LoadStatic();
    static bool Load(const char* szFileName);
    static e_ExpressionEvent StringToEnum(const char* str);

    void    SetExpression(e_ExpressionEvent expression, bool forceChange = false);
    void    Update();
    void    Reset();
    void PrecacheSequences();

    void OnReused(IEntity* pEntity);

protected:

    typedef std::vector<string> TExprState;
    static StaticInstance<TExprState> s_Expressions[EE_Count + 1];

    void MakeFace(const char* pFaceName);
    int SelectExpressionTime() const;

    e_ExpressionEvent   m_CurrentState;
    int m_CurrentExprIdx;
    CTimeValue  m_ExprStartTime;
    int m_ChangeExpressionTimeMs;

    IEntity*    m_pEntity;
};









#endif // CRYINCLUDE_CRYACTION_AIFACEMANAGER_H
