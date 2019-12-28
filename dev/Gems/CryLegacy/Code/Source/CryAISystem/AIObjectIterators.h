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

#ifndef CRYINCLUDE_CRYAISYSTEM_AIOBJECTITERATORS_H
#define CRYINCLUDE_CRYAISYSTEM_AIOBJECTITERATORS_H
#pragma once


//TODO get rid of everything in this file and replace it with something better!  Yuk!



// Little helper template
// T should be in practice a CWeakRef<CAIObject> , CCountedRef<CAIObject>, etc.
template < template <typename> class T >
struct ObjMapIter
{
    typedef typename std::multimap < short, T < CAIObject> >::iterator Type;
};


//====================================================================
// SAIObjectMapIter
// Specialized next for iterating over the whole container.
//====================================================================
template < template <typename> class T >
struct SAIObjectMapIter
    : public IAIObjectIter
{
    typedef typename ObjMapIter<T>::Type Iter;

    SAIObjectMapIter(Iter first, Iter end)
        : m_it(first)
        , m_end(end)
        , m_ai(0)
    {
    }

    virtual IAIObject* GetObject()
    {
        if (!m_ai && m_it != m_end)
        {
            Next();
        }
        return m_ai;
    }

    virtual void Release()
    {
        delete this;
    }

    // (MATT) (was) Update the pointer from the current iterator if it's valid.
    //        (now) That, and keep advancing if the ref is not valid {2009/03/27}
    virtual void    Next()
    {
        for (m_ai = NULL; !m_ai && m_it != m_end; ++m_it)
        {
            m_ai = m_it->second.GetAIObject();
        }
    }

    static SAIObjectMapIter* Allocate(Iter first, Iter end)
    {
        return new SAIObjectMapIter(first, end);
    }

    IAIObject*  m_ai;
    Iter    m_it;
    Iter    m_end;
};



//====================================================================
// SAIObjectMapIterOfType
// Iterator base for iterating over 'AIObjects' container.
// Returns only objects of specified type.
//====================================================================
template < template <typename> class T >
struct SAIObjectMapIterOfType
    : public SAIObjectMapIter <T>
{
    typedef typename SAIObjectMapIter<T>::Iter Iter;

    SAIObjectMapIterOfType(short n, Iter first, Iter end)
        : m_n(n)
        , SAIObjectMapIter<T>(first, end) {}

    virtual void Release()
    {
        delete this;
    }

    static SAIObjectMapIterOfType* Allocate(short n, Iter first, Iter end)
    {
        return new SAIObjectMapIterOfType(n, first, end);
    }

    virtual void    Next()
    {
        for (this->m_ai = NULL; !this->m_ai && this->m_it != this->m_end; ++this->m_it)
        {
            // Constraint to type
            if (this->m_it->first != this->m_n)
            {
                this->m_it = this->m_end;
                break;
            }
            else
            {
                this->m_ai = this->m_it->second.GetAIObject();
            }
        }
    }

    short   m_n;
};



//====================================================================
// SAIObjectMapIterOfTypeInRange
// Specialized next for iterating over the whole container.
// Returns only objects which are enclosed by the specified sphere and of specified type.
//====================================================================
template < template <typename> class T >
struct SAIObjectMapIterOfTypeInRange
    : public SAIObjectMapIter <T>
{
    typedef typename SAIObjectMapIter<T>::Iter Iter;

    using SAIObjectMapIter<T>::m_ai;
    using SAIObjectMapIter<T>::m_it;
    using SAIObjectMapIter<T>::m_end;


    SAIObjectMapIterOfTypeInRange(short n, Iter first, Iter end, const Vec3& center, float rad, bool check2D)
        : m_n(n)
        , m_center(center)
        , m_rad(rad)
        , m_check2D(check2D)
        , SAIObjectMapIter<T>(first, end) {}

    virtual void Release()
    {
        delete this;
    }

    static SAIObjectMapIterOfTypeInRange* Allocate(short n, Iter first, Iter end, const Vec3& center, float rad, bool check2D)
    {
        return new SAIObjectMapIterOfTypeInRange(n, first, end, center, rad, check2D);
    }

    virtual void    Next()
    {
        for (m_ai = NULL; !m_ai && m_it != m_end; ++m_it)
        {
            // Constraint to type
            if (m_it->first != m_n)
            {
                m_it = m_end;
                break;
            }

            CAIObject* pObj = m_it->second.GetAIObject();
            if (!pObj)
            {
                continue;
            }

            // Constraint to sphere
            if (m_check2D)
            {
                if (Distance::Point_Point2DSq(m_center, pObj->GetPos()) < sqr(pObj->GetRadius() + m_rad))
                {
                    m_ai = pObj;
                }
            }
            else
            {
                if (Distance::Point_PointSq(m_center, pObj->GetPos()) < sqr(pObj->GetRadius() + m_rad))
                {
                    m_ai = pObj;
                }
            }
        }
    }

    short   m_n;
    Vec3    m_center;
    float   m_rad;
    bool    m_check2D;
};




//====================================================================
// SAIObjectMapIterInRange
// Iterator base for iterating over 'AIObjects' container.
// Returns only objects which are enclosed by the specified sphere.
//====================================================================
template < template <typename> class T >
struct SAIObjectMapIterInRange
    : public SAIObjectMapIter <T>
{
    typedef typename ObjMapIter<T>::Type Iter;

    using SAIObjectMapIter<T>::m_ai;
    using SAIObjectMapIter<T>::m_it;
    using SAIObjectMapIter<T>::m_end;
    
    SAIObjectMapIterInRange(Iter first, Iter end, const Vec3& center, float rad, bool check2D)
        : m_center(center)
        , m_rad(rad)
        , m_check2D(check2D)
        , SAIObjectMapIter<T>(first, end) {}

    virtual void Release()
    {
        delete this;
    }

    static SAIObjectMapIterInRange* Allocate(Iter first, Iter end, const Vec3& center, float rad, bool check2D)
    {
        return new SAIObjectMapIterInRange(first, end, center, rad, check2D);
    }

    virtual void    Next()
    {
        for (m_ai = NULL; !m_ai && m_it != m_end; ++m_it)
        {
            CAIObject* pObj = m_it->second.GetAIObject();
            if (!pObj)
            {
                continue;
            }

            // Constraint to sphere
            if (m_check2D)
            {
                if (Distance::Point_Point2DSq(m_center, pObj->GetPos()) < sqr(pObj->GetRadius() + m_rad))
                {
                    m_ai = pObj;
                }
            }
            else
            {
                if (Distance::Point_PointSq(m_center, pObj->GetPos()) < sqr(pObj->GetRadius() + m_rad))
                {
                    m_ai = pObj;
                }
            }
        }
    }

    Vec3    m_center;
    float   m_rad;
    bool    m_check2D;
};


//====================================================================
// SAIObjectMapIterInShape
// Iterator base for iterating over 'AIObjects' container.
// Returns only objects which are enclosed by the specified shape.
//====================================================================
template < template <typename> class T >
struct SAIObjectMapIterInShape
    : public SAIObjectMapIter <T>
{
    typedef typename SAIObjectMapIter<T>::Iter Iter;

    using SAIObjectMapIter<T>::m_ai;
    using SAIObjectMapIter<T>::m_it;
    using SAIObjectMapIter<T>::m_end;

    SAIObjectMapIterInShape(Iter first, Iter end, const SShape& shape, bool checkHeight)
        : m_shape(shape)
        , m_checkHeight(checkHeight)
        , SAIObjectMapIter<T>(first, end) {}

    virtual void Release()
    {
        delete this;
    }

    static SAIObjectMapIterInShape* Allocate(Iter first, Iter end, const SShape& shape, bool checkHeight)
    {
        return new SAIObjectMapIterInShape(first, end, shape, checkHeight);
    }

    virtual void    Next()
    {
        for (m_ai = NULL; !m_ai && m_it != m_end; ++m_it)
        {
            CAIObject* pObj = m_it->second.GetAIObject();
            if (!pObj)
            {
                continue;
            }

            // Constraint to shape
            if (m_shape.IsPointInsideShape(pObj->GetPos(), m_checkHeight))
            {
                m_ai = pObj;
            }
        }
    }

    const SShape& m_shape;
    bool    m_checkHeight;
};

//====================================================================
// SAIObjectMapIterOfTypeInRange
// Specialized next for iterating over the whole container.
// Returns only objects which are enclosed by the specified shape and of specified type.
//====================================================================
template < template <typename> class T >
struct SAIObjectMapIterOfTypeInShape
    : public SAIObjectMapIter <T>
{
    typedef typename SAIObjectMapIter<T>::Iter Iter;

    using SAIObjectMapIter<T>::m_ai;
    using SAIObjectMapIter<T>::m_it;
    using SAIObjectMapIter<T>::m_end;

    SAIObjectMapIterOfTypeInShape(short n, Iter first, Iter end, const SShape& shape, bool checkHeight)
        : m_n(n)
        , m_shape(shape)
        , m_checkHeight(checkHeight)
        , SAIObjectMapIter<T>(first, end) {}

    virtual void Release()
    {
        delete this;
    }

    static SAIObjectMapIterOfTypeInShape* Allocate(short n, Iter first, Iter end, const SShape& shape, bool checkHeight)
    {
        return new SAIObjectMapIterOfTypeInShape(n, first, end, shape, checkHeight);
    }

    virtual void    Next()
    {
        for (m_ai = NULL; !m_ai && m_it != m_end; ++m_it)
        {
            // Constraint to type
            if (m_it->first != m_n)
            {
                m_it = m_end;
                break;
            }

            CAIObject* pObj = m_it->second.GetAIObject();
            if (!pObj)
            {
                continue;
            }

            // Constraint to shape
            if (m_shape.IsPointInsideShape(pObj->GetPos(), m_checkHeight))
            {
                m_ai = pObj;
            }
        }
    }

    short   m_n;
    const SShape& m_shape;
    bool    m_checkHeight;
};



// (MATT) Iterators now have their destructors called before they enter the pool - so we only need to free the memory here {2008/12/04}
template < template <typename> class T>
void DeleteAIObjectMapIter(SAIObjectMapIter<T>* ptr) { operator delete(ptr); }

//===================================================================
// ClearAIObjectIteratorPools
//===================================================================
void ClearAIObjectIteratorPools();


#endif // CRYINCLUDE_CRYAISYSTEM_AIOBJECTITERATORS_H
