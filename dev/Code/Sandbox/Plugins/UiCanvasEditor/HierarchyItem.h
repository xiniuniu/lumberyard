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

#include <QTreeWidgetItem>

#include "EditorCommon.h"

class HierarchyItem
    : public QObject
    , public QTreeWidgetItem
{
    Q_OBJECT

public:

    explicit HierarchyItem(EditorWindow* editWindow,
        QTreeWidgetItem& parent,
        int childIndex,
        const QString label,
        AZ::Entity* optionalElement);
    virtual ~HierarchyItem();

    //! This should NEVER return a nullptr.
    AZ::Entity* GetElement() const;
    AZ::EntityId GetEntityId() const;

    //! This is ONLY ever called when the HierarchyWidget is being destroyed
    void ClearEntityId();

    void SetMouseIsHovering(bool isHovering);

    void SetIsExpanded(bool isExpanded);
    void ApplyElementIsExpanded();
    void SetIsSelectable(bool isSelectable);
    void SetIsSelected(bool isSelected);
    void SetIsVisible(bool isVisible);

    HierarchyItem* Parent() const;
    HierarchyItem* Child(int i) const;

    //! This is a generic marker, for use by any algorithm.
    void SetMark(bool m);
    bool GetMark();

    //! This is ephemeral data used for snapping.
    //@{
    void SetNonSnappedOffsets(UiTransform2dInterface::Offsets offsets);
    UiTransform2dInterface::Offsets GetNonSnappedOffsets();

    void SetNonSnappedZRotation(float rotation);
    float GetNonSnappedZRotation();
    //@}

    //! This is our PREVIOUS parent and childRow.
    //! This is used to undo reparenting.
    void SetPreMove(AZ::EntityId parentId, int childRow);
    AZ::EntityId GetPreMoveParentId();
    int GetPreMoveChildRow();

    void ReplaceElement(const AZStd::string& buffer, const AZStd::unordered_set<AZ::Data::AssetId>& referencedSliceAssets);

    //! Update the visual look of the element to show slice information
    void UpdateSliceInfo();

    //! Update the visual look of the element to show whether it's editor only
    void UpdateEditorOnlyInfo();

signals:

    void SignalItemAdd(HierarchyItem* item);
    void SignalItemRemove(HierarchyItem* item);

private:

    void DeleteElement();

    void UpdateIcon();
    void UpdateChildIcon();

    //! Update the visual look of the element and its descendants to show whether they're editor only
    void UpdateEditorOnlyInfoRecursive();

    EditorWindow* m_editorWindow;

    AZ::EntityId m_elementId;

    // IMPORTANT: This is used for searching and culling items.
    // This ISN'T thread-safe. This ISN'T persistent.
    bool m_mark;

    AZ::EntityId m_preMoveParentId;
    int m_preMoveChildRow;

    bool m_mouseIsHovering;

    UiTransform2dInterface::Offsets m_nonSnappedOffsets;
    float m_nonSnappedZRotation;
};
