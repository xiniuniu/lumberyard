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

#include <AzCore/std/containers/unordered_map.h>

#include <GraphCanvas/Widgets/GraphCanvasTreeItem.h>

namespace GraphCanvas
{
    class CategorizerInterface
    {
    public:
        virtual GraphCanvas::GraphCanvasTreeItem* CreateCategoryNode(AZStd::string_view categoryPath, AZStd::string_view categoryName, GraphCanvasTreeItem* parent) const = 0;
    };

    // Gives some utils for creating pathed categories inside of the tree.
    // i.e. "Order/Family/Genus/Species"
    // Order, Family, Genus are all categories as they all might have more then one element. Where as Species would be a leaf
    // node as there is a single species.
    //
    // This class just provides an easy interface to pass in the full category path and have it manage all of the elements.
    // Only caveat is that this must be used when removing elements so it can maintain the internal state.
    class GraphCanvasTreeCategorizer
    {
    private:
        typedef AZStd::pair<GraphCanvas::GraphCanvasTreeItem*, AZStd::string> CategoryKey;
        typedef AZStd::unordered_map<CategoryKey, GraphCanvas::GraphCanvasTreeItem*> CategoryRootsMap;
    public:
        GraphCanvasTreeCategorizer(const CategorizerInterface& nodePaletteModel);

        GraphCanvas::GraphCanvasTreeItem* GetCategoryNode(const char* categoryPath, GraphCanvas::GraphCanvasTreeItem* parentRoot);

        void PruneEmptyNodes();
        void PruneNode(GraphCanvasTreeItem* treeItem);

        void EnableDebug(bool enabled)
        {
            m_debugEnabled = enabled;
        }

    private:

        void PruneNodes(AZStd::unordered_set< GraphCanvas::GraphCanvasTreeItem*> recursiveSet);

        const CategorizerInterface& m_categorizerInterface;
        CategoryRootsMap m_rootMaps;

        bool m_debugEnabled;
    };    
}