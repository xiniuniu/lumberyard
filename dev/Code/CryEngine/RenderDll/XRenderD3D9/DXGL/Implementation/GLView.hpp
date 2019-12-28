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

// Description : Declares the resource view types and related functions


#ifndef __GLVIEW__
#define __GLVIEW__

#include "GLResource.hpp"

namespace NCryOpenGL
{
    struct SShaderTextureViewConfiguration
    {
        EGIFormat m_eFormat;
        GLenum m_eTarget;
        uint32 m_uMinMipLevel;
        uint32 m_uNumMipLevels;
        uint32 m_uMinLayer;
        uint32 m_uNumLayers;

        SShaderTextureViewConfiguration(EGIFormat eFormat, GLenum eTarget, uint32 uMinMipLevel, uint32 uNumMipLevels, uint32 uMinLayer, uint32 uNumLayers)
            : m_eFormat(eFormat)
            , m_eTarget(eTarget)
            , m_uMinMipLevel(uMinMipLevel)
            , m_uNumMipLevels(uNumMipLevels)
            , m_uMinLayer(uMinLayer)
            , m_uNumLayers(uNumLayers)
        {
        }

        bool operator==(const SShaderTextureViewConfiguration& kOther)
        {
            return
                m_eFormat       == kOther.m_eFormat &&
                m_eTarget       == kOther.m_eTarget &&
                m_uMinMipLevel  == kOther.m_uMinMipLevel &&
                m_uNumMipLevels == kOther.m_uNumMipLevels &&
                m_uMinLayer     == kOther.m_uMinLayer &&
                m_uNumLayers    == kOther.m_uNumLayers;
        }
    };

    struct SShaderTextureBufferViewConfiguration
    {
        EGIFormat m_eFormat;
        uint32 m_uFirstElement;
        uint32 m_uNumElements;

        SShaderTextureBufferViewConfiguration(EGIFormat eFormat, uint32 uFirstElement, uint32 uNumElements)
            : m_eFormat(eFormat)
            , m_uFirstElement(uFirstElement)
            , m_uNumElements(uNumElements)
        {
        }

        bool operator==(const SShaderTextureBufferViewConfiguration& kOther)
        {
            return
                m_eFormat        == kOther.m_eFormat &&
                m_uFirstElement  == kOther.m_uFirstElement &&
                m_uNumElements   == kOther.m_uNumElements;
        }
    };

    struct SShaderImageViewConfiguration
    {
        GLint m_iLevel;
        GLint m_iLayer; // -1 for all layers
        GLenum m_eAccess;
        GLenum m_eFormat;

        SShaderImageViewConfiguration()
            : m_iLevel(0)
            , m_iLayer(-1)
            , m_eAccess(GL_NONE)
            , m_eFormat(GL_NONE)
        {
        }

        SShaderImageViewConfiguration(GLenum eFormat, GLint iLevel, GLint iLayer, GLenum eAccess)
            : m_eFormat(eFormat)
            , m_iLevel(iLevel)
            , m_iLayer(iLayer)
            , m_eAccess(eAccess)
        {
        }

        bool operator==(const SShaderImageViewConfiguration& kOther)
        {
            return
                m_iLevel  == kOther.m_iLevel &&
                m_iLayer  == kOther.m_iLayer &&
                m_eAccess == kOther.m_eAccess &&
                m_eFormat == kOther.m_eFormat;
        }

        bool operator!=(const SShaderImageViewConfiguration& kOther)
        {
            return !operator==(kOther);
        }
    };

    struct SOutputMergerTextureViewConfiguration
    {
        EGIFormat m_eFormat;
        uint32 m_uMipLevel;
        uint32 m_uMinLayer;
        uint32 m_uNumLayers;

        SOutputMergerTextureViewConfiguration(EGIFormat eFormat, uint32 uMipLevel, uint32 uMinLayer, uint32 uNumLayers)
            : m_eFormat(eFormat)
            , m_uMipLevel(uMipLevel)
            , m_uMinLayer(uMinLayer)
            , m_uNumLayers(uNumLayers)
        {
        }

        bool operator==(const SOutputMergerTextureViewConfiguration& kOther)
        {
            return
                m_eFormat == kOther.m_eFormat &&
                m_uMipLevel == kOther.m_uMipLevel &&
                m_uMinLayer == kOther.m_uMinLayer &&
                m_uNumLayers == kOther.m_uNumLayers;
        }
    };

    DXGL_DECLARE_REF_COUNTED(struct, SShaderView)
    {
        enum Type
        {
            eSVT_Buffer,
            eSVT_Texture,
            eSVT_Image
        };

        SShaderView(Type eType);
        virtual ~SShaderView();

        CResourceName m_kName;
        SSharingFence m_kCreationFence;
        Type m_eType;
    };

    struct SShaderBufferView
        : SShaderView
    {
        SShaderBufferView(const SBufferRange& kRange);
        virtual ~SShaderBufferView();

        void Init(SBuffer* pBuffer, CContext* pContext);

        SBufferRange m_kRange;
    };

    struct SShaderTextureBasedView
        : SShaderView
    {
        SShaderTextureBasedView(Type eType);
        virtual ~SShaderTextureBasedView();

        virtual bool BindTextureUnit(SSamplerState* pSamplerState, STextureUnitContext& kContext, CContext* pContext, const STextureUnitCache& kCurrentUnitCache);
        virtual bool GenerateMipmaps(CContext* pContext);

        bool m_bTextureOwned;

        bool m_bFormatRequiresUniqueView;
    };

    struct SShaderTextureBufferView
        : SShaderTextureBasedView
    {
        SShaderTextureBufferView(const SShaderTextureBufferViewConfiguration& kConfiguration);
        virtual ~SShaderTextureBufferView();

        bool Init(SBuffer* pBuffer, CContext* pContext);

        virtual bool BindTextureUnit(SSamplerState* pSamplerState, STextureUnitContext& kContext, CContext* pContext, const STextureUnitCache& kCurrentUnitCache);

        SBuffer* m_pBuffer;
        SShaderTextureBufferViewConfiguration m_kConfiguration;
        SShaderTextureBufferView* m_pNextView;
    };

    struct SShaderTextureView
        : SShaderTextureBasedView
    {
        SShaderTextureView(const SShaderTextureViewConfiguration& kConfiguration);
        virtual ~SShaderTextureView();

        bool Init(STexture* pTexture, CContext* pContext);
        bool CreateUniqueView(CContext* pContext);

        virtual bool BindTextureUnit(SSamplerState* pSamplerState, STextureUnitContext& kContext, CContext* pContext, const STextureUnitCache& kCurrentUnitCache);
        virtual bool GenerateMipmaps(CContext* pContext);

        STexture* m_pTexture;
        SShaderTextureViewConfiguration m_kConfiguration;
        STextureState m_kViewState;
        SShaderTextureView* m_pNextView;
    };

    struct SShaderImageView
        : SShaderTextureBasedView
    {
        SShaderImageView(const SShaderImageViewConfiguration& kConfiguration);
        virtual ~SShaderImageView();

        bool Init(STexture* pTexture, CContext* pContext);
        bool Init(SShaderTextureBasedView* pTextureBufferView, CContext* pContext);

        SShaderImageViewConfiguration m_kConfiguration;
        SShaderViewPtr m_spAuxiliaryView;
    };

    struct SOutputMergerTextureView;

    DXGL_DECLARE_REF_COUNTED(struct, SOutputMergerView)
    {
        SOutputMergerView(const CResourceName&kUniqueView, EGIFormat eFormat);
        virtual ~SOutputMergerView();

        virtual bool AttachFrameBuffer(SFrameBuffer * pFrameBuffer, GLenum eAttachmentID, CContext * pContext);
        virtual void DetachFrameBuffer(SFrameBuffer * pFrameBuffer);

        virtual bool FrameBufferTexture(GLuint uFrameBuffer, GLenum eAttachmentID);

        virtual void Bind(const SFrameBuffer&kFrameBuffer, CContext * pContext);

        struct SFrameBufferRef
        {
            SFrameBufferPtr m_spFrameBuffer;
            uint32 m_uNumBindings;

            SFrameBufferRef(SFrameBuffer* pFrameBuffer, uint32 uNumBindings)
                : m_spFrameBuffer(pFrameBuffer)
                , m_uNumBindings(uNumBindings)
            {
            }

            bool operator==(const SFrameBuffer* pOther)
            {
                return m_spFrameBuffer == pOther;
            }
        };

        typedef std::vector<SFrameBufferRef> TFrameBufferRefs;

        struct SContextData
        {
            TFrameBufferRefs m_kBoundFrameBuffers;
        };

        typedef SContextData* TContextMap[MAX_NUM_CONTEXT_PER_DEVICE];

        CResourceName m_kUniqueView;
        EGIFormat m_eFormat;
        TContextMap m_kContextMap;
        SSharingFence m_kCreationFence;

#if defined(ANDROID) || defined(DXGL_USE_LAZY_CLEAR)
        virtual SOutputMergerTextureView* AsSOutputMergerTextureView() { return NULL; }
#endif
    };

    struct SOutputMergerTextureView
        : SOutputMergerView
    {
        SOutputMergerTextureView(STexture* pTexture, const SOutputMergerTextureViewConfiguration& kConfiguration);
        virtual ~SOutputMergerTextureView();

        bool Init(CContext* pContext);

        virtual bool FrameBufferTexture(GLuint uFrameBuffer, GLenum eAttachmentID);
        virtual bool CreateUniqueView(const SGIFormatInfo* pFormatInfo, CContext* pContext);

        STexture* m_pTexture;
        SOutputMergerTextureViewConfiguration m_kConfiguration;
        SOutputMergerTextureView* m_pNextView;
        GLint m_iMipLevel;
        GLint m_iLayer; //INVALID_LAYER for all layers

        static const GLint INVALID_LAYER;

#if defined(ANDROID) || defined(DXGL_USE_LAZY_CLEAR)
        virtual SOutputMergerTextureView* AsSOutputMergerTextureView() { return this; }
#endif
    };

    struct SDefaultFrameBufferOutputMergerView
        : SOutputMergerTextureView
    {
        SDefaultFrameBufferOutputMergerView(SDefaultFrameBufferTexture* pTexture, const SOutputMergerTextureViewConfiguration& kConfiguration);
        virtual ~SDefaultFrameBufferOutputMergerView();

        virtual bool AttachFrameBuffer(SFrameBuffer* pFrameBuffer, GLenum eAttachmentID, CContext* pContext);
        virtual void Bind(const SFrameBuffer& kFrameBuffer, CContext* pContext);
        virtual bool CreateUniqueView(const SGIFormatInfo* pFormatInfo, CContext* pContext);

        bool m_bUsesTexture;
    };

    struct SDefaultFrameBufferShaderTextureView
        : SShaderTextureView
    {
        SDefaultFrameBufferShaderTextureView(const SShaderTextureViewConfiguration& kConfiguration);
        virtual ~SDefaultFrameBufferShaderTextureView();

        virtual bool BindTextureUnit(SSamplerState* pSamplerState, STextureUnitContext& kContext, CContext* pContext, const STextureUnitCache& kCurrentUnitCache);

        bool m_bUsesTexture;
    };


    SShaderViewPtr CreateShaderResourceView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_SHADER_RESOURCE_VIEW_DESC& kViewDesc, CContext* pContext);
    SShaderViewPtr CreateUnorderedAccessView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_UNORDERED_ACCESS_VIEW_DESC& kViewDesc, CContext* pContext);
    SOutputMergerViewPtr CreateRenderTargetView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_RENDER_TARGET_VIEW_DESC& kViewDesc, CContext* pContext);
    SOutputMergerViewPtr CreateDepthStencilView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_DEPTH_STENCIL_VIEW_DESC& kViewDesc, CContext* pContext);
}

#endif //__GLVIEW__

