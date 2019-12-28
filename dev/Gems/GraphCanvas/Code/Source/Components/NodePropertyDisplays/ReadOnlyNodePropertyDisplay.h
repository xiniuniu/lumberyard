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

#include <GraphCanvas/Components/NodePropertyDisplay/NodePropertyDisplay.h>
#include <GraphCanvas/Components/NodePropertyDisplay/ReadOnlyDataInterface.h>

namespace GraphCanvas
{
    class GraphCanvasLabel;
    
    class ReadOnlyNodePropertyDisplay
        : public NodePropertyDisplay 
    {    
    public:
        AZ_CLASS_ALLOCATOR(ReadOnlyNodePropertyDisplay, AZ::SystemAllocator, 0);
		
        ReadOnlyNodePropertyDisplay(ReadOnlyDataInterface* dataInterface);
        virtual ~ReadOnlyNodePropertyDisplay();
    
        // NodePropertyDisplay
        void RefreshStyle() override;
        void UpdateDisplay() override;
        
        QGraphicsLayoutItem* GetDisabledGraphicsLayoutItem() override;
        QGraphicsLayoutItem* GetDisplayGraphicsLayoutItem() override;
        QGraphicsLayoutItem* GetEditableGraphicsLayoutItem() override;
        ////
    
    private:
    
        ReadOnlyDataInterface*  m_dataInterface;
    
        GraphCanvasLabel*       m_disabledLabel;
        GraphCanvasLabel*       m_displayLabel;        
    };
}