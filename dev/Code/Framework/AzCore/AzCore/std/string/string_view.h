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

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/iterator.h>
#include <AzCore/std/hash.h>

namespace AZStd
{
    namespace StringInternal
    {
        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count)
        {
            AZ_Assert(ptr != NULL, "Invalid input!");

            // look for [ptr, ptr + count) beginning at or after offset
            if (count == 0 && offset <= size)
            {
                return offset;  // null string always matches (if inside string)
            }
            
            SizeT nm;
            if (offset < size && count <= (nm = size - offset))
            {   // room for match, look for it
                nm -= count - 1;
                const CharT* vptr = data + offset;
                const CharT* uptr = Traits::find(vptr, nm, *ptr);
                for (; uptr != nullptr; nm -= uptr - vptr + 1, vptr = uptr + 1)
                {
                    if (Traits::compare(uptr, ptr, count) == 0)
                    {
                        return (uptr - data);   // found a match
                    }

                    uptr = Traits::find(vptr, nm, *ptr);
                }
            }

            return (npos);  // no match
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find(const CharT* data, SizeT size, CharT c, SizeT offset)
        {
            if (offset < size)
            {   // room for match, look for it
                const CharT* vptr = data + offset;
                const CharT* uptr = Traits::find(vptr, size - offset, c);
                if (uptr)
                {
                    return uptr - data;
                }
            }

            return (npos);  // no match
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT rfind(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count)
        {   // look for [ptr, ptr + count) beginning before offset
            if (count == 0)
            {
                return (offset < size ? offset : size); // null always matches
            }
            if (count <= size)
            {   // room for match, look for it
                const CharT* uptr = data + (offset < size - count ? offset : size - count);
                for (;; --uptr)
                {
                    if (Traits::eq(*uptr, *ptr) && Traits::compare(uptr, ptr, count) == 0)
                    {
                        return (uptr - data);   // found a match
                    }
                    else if (uptr == data)
                    {
                        break;  // at beginning, no more chance for match
                    }
                }
            }

            return npos;    // no match
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT rfind(const CharT* data, SizeT size, CharT c, SizeT offset)
        {
            if (size != 0)
            {   // room for match, look for it
                const CharT* uptr = data + (offset < size ? offset + 1 : size);
                for (; data != uptr;)
                {
                    if (Traits::eq(*--uptr, c))
                    {
                        return (uptr - data);   // found a match
                    }
                }
            }

            return npos;    // no match
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find_first_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count)
        {   // look for one of [ptr, ptr + count) at or after offset
            if (0 < count && offset < size)
            {   // room for match, look for it
                const CharT* const vptr = data + size;
                for (const CharT* uptr = data + offset; uptr < vptr; ++uptr)
                {
                    if (Traits::find(ptr, count, *uptr) != nullptr)
                    {
                        return uptr - data; // found a match
                    }
                }
            }
            return npos;    // no match
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find_last_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count)
        {   // look for one of [ptr, ptr + count) before offset
            if (0 < count && 0 < size)
            {
                for (const CharT* uptr = data + (offset < size ? offset : size - 1);; --uptr)
                {
                    if (Traits::find(ptr, count, *uptr) != nullptr)
                    {
                        return uptr - data; // found a match
                    }
                    else if (uptr == data)
                    {
                        break;  // at beginning, no more chance for match
                    }
                }
            }

            return npos;    // no match
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find_first_not_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count)
        {
            // look for none of [ptr, ptr + count) at or after offset
            if (offset < size)
            {   // room for match, look for it
                const CharT* const vptr = data + size;
                for (const CharT* uptr = data + offset; uptr < vptr; ++uptr)
                {
                    if (Traits::find(ptr, count, *uptr) == nullptr)
                    {
                        return uptr - data;
                    }
                }
            }
            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find_first_not_of(const CharT* data, SizeT size, CharT c, SizeT offset)
        {
            // look for none of [ptr, ptr + count) at or after offset
            if (offset < size)
            {   // room for match, look for it
                const CharT* const vptr = data + size;
                for (const CharT* uptr = data + offset; uptr < vptr; ++uptr)
                {
                    if (!Traits::eq(*uptr, c))
                    {
                        return uptr - data;
                    }
                }
            }
            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find_last_not_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count)
        {   // look for none of [ptr, ptr + count) before offset
            if (0 < size)
            {
                for (const CharT* uptr = data + (offset < size ? offset : size - 1);; --uptr)
                {
                    if (Traits::find(ptr, count, *uptr) == nullptr)
                    {
                        return uptr - data;
                    }
                    else if (uptr == data)
                    {
                        break;
                    }
                }
            }
            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        SizeT find_last_not_of(const CharT* data, SizeT size, CharT c, SizeT offset)
        {   // look for none of [ptr, ptr + count) before offset
            if (0 != size)
            {
                for (const CharT* uptr = data + (offset < size ? offset + 1 : size); data != uptr;)
                {
                    if (!Traits::eq(*--uptr, c))
                    {
                        return uptr - data;
                    }
                }
            }
            return npos;
        }
    }

    template<class Element>
    struct char_traits;
    /**
    * \ref C++0x (21.2.3.1)
    * char Traits (todo move to a separate file)
    */
    template<>
    struct char_traits<char>
    {
        // properties of a string or stream char element
        typedef char        char_type;
        typedef int         int_type;
        //typedef AZSTD_STL::streampos  pos_type;
        //typedef AZSTD_STL::streamoff  off_type;
        //typedef AZSTD_STL::mbstate_t  state_type;

        inline static void assign(char_type& left, const char_type& right) { left = right; }
        inline static bool eq(char_type left, char_type right) { return left == right; }
        inline static bool lt(char_type left, char_type right) { return left < right; }
        inline static int compare(const char_type* s1, const char_type* s2, AZStd::size_t count) { AZ_Assert(s1 != NULL && s2 != NULL, "Invalid input!"); return ::memcmp(s1, s2, count); }
        inline static AZStd::size_t length(const char_type* s) { AZ_Assert(s != NULL, "Invalid input!"); return ::strlen(s); }
        inline static const char_type* find(const char_type* s, AZStd::size_t count, const char_type& ch) { AZ_Assert(s != NULL, "Invalid input!"); return (const char_type*)::memchr(s, ch, count); }
        inline static char_type* move(char_type* dest, const char_type* src, AZStd::size_t count) { AZ_Assert(dest != NULL && src != NULL, "Invalid input!"); return (char_type*)::memmove(dest, src, count); }
        inline static char_type* copy(char_type* dest, const char_type* src, AZStd::size_t count) { AZ_Assert(dest != NULL && src != NULL, "Invalid input!"); return (char_type*)::memcpy(dest, src, count); }
        inline static char_type* assign(char_type* dest, AZStd::size_t count, char_type ch) { AZ_Assert(dest != NULL, "Invalid input!"); return (char_type*)::memset(dest, ch, count); }
        inline static char_type to_char_type(int_type c) { return (char_type)c; }
        inline static int_type      to_int_type(char_type c) { return ((unsigned char)c); }
        inline static bool          eq_int_type(int_type left, int_type right) { return left == right; }
        inline static int_type      eof() { return (int_type)-1; }
        inline static int_type      not_eof(const int_type c) { return (c != (int_type)-1 ? c : !eof()); }
    };

    template<>
    struct char_traits<wchar_t>
    {
        // properties of a string or stream char element
        typedef wchar_t     char_type;
#ifdef _WCTYPE_T_DEFINED
        typedef wint_t      int_type;
#else
        typedef unsigned short int_type;
#endif
        //typedef AZSTD_STL::streampos  pos_type;
        //typedef AZSTD_STL::streamoff  off_type;
        //typedef AZSTD_STL::mbstate_t  state_type;

        inline static void assign(char_type& left, const char_type& right) { left = right; }
        inline static bool eq(char_type left, char_type right) { return left == right; }
        inline static bool lt(char_type left, char_type right) { return left < right; }
        inline static int compare(const char_type* s1, const char_type* s2, AZStd::size_t count) { AZ_Assert(s1 != NULL && s2 != NULL, "Invalid input!"); return ::wmemcmp(s1, s2, count); }
        inline static AZStd::size_t length(const char_type* s) { AZ_Assert(s != NULL, "Invalid input!"); return ::wcslen(s); }
        inline static const char_type* find(const char_type* s, AZStd::size_t count, const char_type& ch) { AZ_Assert(s != NULL, "Invalid input!"); return (const char_type*)::wmemchr(s, ch, count); }
        inline static char_type* move(char_type* dest, const char_type* src, AZStd::size_t count) { AZ_Assert(dest != NULL && src != NULL, "Invalid input!"); return (char_type*)::wmemmove(dest, src, count); }
        inline static char_type* copy(char_type* dest, const char_type* src, AZStd::size_t count) { AZ_Assert(dest != NULL && src != NULL, "Invalid input!"); return (char_type*)::wmemcpy(dest, src, count); }
        inline static char_type* assign(char_type* dest, AZStd::size_t count, char_type ch) { AZ_Assert(dest != NULL, "Invalid input!"); return (char_type*)::wmemset(dest, ch, count); }
        inline static char_type to_char_type(int_type c) { return (char_type)c; }
        inline static int_type      to_int_type(char_type c) { return ((unsigned char)c); }
        inline static bool          eq_int_type(int_type left, int_type right) { return left == right; }
        inline static int_type      eof() { return (int_type)-1; }
        inline static int_type      not_eof(const int_type c) { return (c != (int_type)-1 ? c : !eof()); }
    };

    template<class Element, class Traits, class Allocator>
    class basic_string;

    /**
     * Immutable string wrapper based on boost::const_string and std::string_view. When we operate on
     * const char* we don't know if this points to NULL terminated string or just a char array.
     * to have a clear distinction between them we provide this wrapper.
     */
    template <class Element, class Traits = AZStd::char_traits<Element>>
    class basic_string_view
    {
    public:
        using traits_type = Traits;
        using value_type = Element;

        using pointer = value_type*;
        using const_pointer = const value_type*;

        using reference = value_type&;
        using const_reference = const value_type&;

        using size_type = AZStd::size_t;
        using difference_type = AZStd::ptrdiff_t;

        static const size_type npos = size_type(-1);

        using iterator = const value_type*;
        using const_iterator = const value_type*;
        using reverse_iterator = AZStd::reverse_iterator<iterator>;
        using const_reverse_iterator = AZStd::reverse_iterator<const_iterator>;

        basic_string_view()
            : m_begin(nullptr)
            , m_end(nullptr)
        { }

        basic_string_view(const_pointer s)
            : m_begin(s)
            , m_end(s ? s + traits_type::length(s) : nullptr)
        { }

        basic_string_view(const_pointer s, size_type length)
            : m_begin(s)
            , m_end(m_begin + length)
        { }

        basic_string_view(const_pointer first, const_pointer last)
            : m_begin(first)
            , m_end(last)
        { }

        basic_string_view(const basic_string_view&) = default;
        basic_string_view(basic_string_view&& other)
            : basic_string_view()
        {
            swap(other);
        }

        const_reference operator[](size_type index) const { return m_begin[index]; }
        /// Returns value, not reference. If index is out of bounds, 0 is returned (can't be reference).
        value_type at(size_type index) const
        {
            AZ_Assert(index < size(), "pos value is out of range");
            return index >= size()
                ? value_type(0)
                : m_begin[index];
        }

        const_reference front() const
        {
            AZ_Assert(!empty(), "string_view::front(): string is empty");
            return m_begin[0];
        }

        const_reference back() const
        {
            AZ_Assert(!empty(), "string_view::back(): string is empty");
            return m_end[-1];
        }

        const_pointer data() const { return m_begin; }

        size_type length() const { return m_end - m_begin; }
        size_type size() const { return m_end - m_begin; }
        size_type max_size() const { return (std::numeric_limits<size_type>::max)(); } //< Wrapping the numeric_limits<size_type>::max function in parenthesis to get around the issue with windows.h defining max as a macro
        bool      empty() const { return m_end == m_begin; }

        void erase() { m_begin = m_end = nullptr; }
        void resize(size_type new_len) { if (m_begin + new_len < m_end) m_end = m_begin + new_len; }
        void rshorten(size_type shift = 1) { m_end -= shift; if (m_end <= m_begin) erase(); }
        void lshorten(size_type shift = 1) { m_begin += shift; if (m_end <= m_begin) erase(); }
        void remove_prefix(size_type n)
        {
            AZ_Assert(n <= size(), "Attempting to remove prefix larger than string size");
            lshorten(n);
        }

        void remove_suffix(size_type n)
        {
            AZ_Assert(n <= size(), "Attempting to remove suffix larger than string size");
            rshorten(n);
        }

        basic_string_view& operator=(const basic_string_view& s) { if (&s != this) { m_begin = s.m_begin; m_end = s.m_end; } return *this; }
        basic_string_view& operator=(const_pointer s) { return *this = basic_string_view(s); }

        basic_string_view& assign(const basic_string_view& s) { return *this = s; }
        basic_string_view& assign(const_pointer s) { return *this = basic_string_view(s); }
        basic_string_view& assign(const_pointer s, size_type len) { return *this = basic_string_view(s, len); }
        basic_string_view& assign(const_pointer f, const_pointer l) { return *this = basic_string_view(f, l); }

        void swap(basic_string_view& s)
        {
            const_pointer tmp1 = m_begin;
            const_pointer tmp2 = m_end;
            m_begin = s.m_begin;
            m_end = s.m_end;
            s.m_begin = tmp1;
            s.m_end = tmp2;
        }
        friend bool operator==(basic_string_view s1, basic_string_view s2)
        {
            return s1.length() == s2.length() && (s1.length() == 0 || traits_type::compare(s1.m_begin, s2.m_begin, s1.length()) == 0);
        }

        friend bool operator!=(basic_string_view s1, basic_string_view s2) { return !(s1 == s2); }
        friend bool operator<(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) < 0; }
        friend bool operator>(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) > 0; }
        friend bool operator<=(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) <= 0; }
        friend bool operator>=(basic_string_view lhs, basic_string_view rhs) { return lhs.compare(rhs) >= 0; }

        iterator         begin() const { return m_begin; }
        iterator         end() const { return m_end; }
        const_iterator   cbegin() const { return m_begin; }
        const_iterator   cend() const { return m_end; }
        reverse_iterator rbegin() const { return reverse_iterator(m_end); }
        reverse_iterator rend() const { return reverse_iterator(m_begin); }
        const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
        const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

        // [string.view.modifiers], modifiers:
        size_type copy(pointer dest, size_type count, size_type pos = 0) const
        {
            AZ_Assert(pos <= size(), "Position of bytes to copy to destination is out of string_view range");
            if (pos > size())
            {
                return 0;
            }
            size_type rlen = AZ::GetMin<size_type>(count, size() - pos);
            Traits::copy(dest, data() + pos, rlen);
            return rlen;
        }

        basic_string_view substr(size_type pos = 0, size_type count = npos) const
        {
            AZ_Assert(pos <= size(), "Cannot create substring where position is larger than size");
            return pos > size() ? basic_string_view() : basic_string_view(data() + pos, AZ::GetMin<size_type>(count, size() - pos));
        }

        int compare(basic_string_view other) const
        {
            size_t cmpSize = AZ::GetMin<size_type>(size(), other.size());
            int cmpval = cmpSize == 0 ? 0 : Traits::compare(data(), other.data(), cmpSize);
            if (cmpval == 0)
            {
                if (size() == other.size())
                {
                    return 0;
                }
                else if (size() < other.size())
                {
                    return -1;
                }

                return 1;
            }

            return cmpval;
        }

        int compare(size_type pos1, size_type count1, basic_string_view other) const
        {
            return substr(pos1, count1).compare(other);
        }

        int compare(size_type pos1, size_type count1, basic_string_view sv, size_type pos2, size_type count2) const
        {
            return substr(pos1, count1).compare(sv.substr(pos2, count2));
        }

        int compare(const_pointer s) const
        {
            return compare(basic_string_view(s));
        }

        int compare(size_type pos1, size_type count1, const_pointer s) const
        {
            return substr(pos1, count1).compare(basic_string_view(s));
        }

        int compare(size_type pos1, size_type count1, const_pointer s, size_type count2) const
        {
            return substr(pos1, count1).compare(basic_string_view(s, count2));
        }

        // starts_with
        bool starts_with(basic_string_view prefix) const
        {
            return size() >= prefix.size() && compare(0, prefix.size(), prefix) == 0;
        }

        bool starts_with(value_type prefix) const
        {
            return starts_with(basic_string_view(&prefix, 1));
        }

        bool starts_with(const_pointer prefix) const
        {
            return starts_with(basic_string_view(prefix));
        }

        // ends_with
        bool ends_with(basic_string_view suffix) const
        {
            return size() >= suffix.size() && compare(size() - suffix.size(), npos, suffix) == 0;
        }

        bool ends_with(value_type suffix) const
        {
            return ends_with(basic_string_view(&suffix, 1));
        }

        bool ends_with(const_pointer suffix) const
        {
            return ends_with(basic_string_view(suffix));
        }

        // find
        size_type find(basic_string_view other, size_type pos = 0) const
        {
            AZ_Assert(other.size() == 0 || other.data(), "other string is not valid");
            return StringInternal::find<value_type, size_type, traits_type, npos>(data(), size(), other.data(), pos, other.size());
        }

        AZ_INLINE size_type find(value_type c, size_type pos = 0) const
        {
            return StringInternal::find<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        AZ_INLINE size_type find(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find(): received nullptr");
            return StringInternal::find<value_type, size_type, traits_type, npos>(data(), size(), s, pos, count);
        }

        AZ_INLINE size_type find(const_pointer s, size_type pos = 0) const
        {
            AZ_Assert(s, "string_view::find(): received nullptr");
            return StringInternal::find<value_type, size_type, traits_type, npos>(data(), size(), s, pos, traits_type::length(s));
        }

        // rfind
        size_type rfind(basic_string_view s, size_type pos = npos) const
        {
            AZ_Assert(s.size() == 0 || s.data(), "string_view::find(): received nullptr");
            return StringInternal::rfind<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        size_type rfind(value_type c, size_type pos = npos) const
        {
            return StringInternal::rfind<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        size_type rfind(const_pointer s, size_type pos, size_type n)
        {
            AZ_Assert(n == 0 || s, "string_view::rfind(): received nullptr");
            return StringInternal::rfind<value_type, size_type, traits_type, npos>(data(), size(), s, pos, n);
        }

        size_type rfind(const_pointer s, size_type pos = npos) const
        {
            AZ_Assert(s, "string_view::rfind(): received nullptr");
            return StringInternal::rfind<value_type, size_type, traits_type, npos>(data(), size(), s, pos, traits_type::length(s));
        }

        // find_first_of
        size_type find_first_of(basic_string_view s, size_type pos = 0) const
        {
            AZ_Assert(s.size() == 0 || s.data(), "string_view::find_first_of(): received nullptr");
            return StringInternal::find_first_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        size_type find_first_of(value_type c, size_type pos = 0) const
        {
            return find(c, pos);
        }

        size_type find_first_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_first_of(): received nullptr");
            return StringInternal::find_first_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, count);
        }

        size_type find_first_of(const_pointer s, size_type pos = 0) const
        {
            AZ_Assert(s, "string_view::find_first_of(): received nullptr");
            return StringInternal::find_first_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, traits_type::length(s));
        }

        // find_last_of
        size_type find_last_of(basic_string_view s, size_type pos = npos) const
        {
            AZ_Assert(s.size() == 0 || s.data(), "string_view::find_last_of(): received nullptr");
            return StringInternal::find_last_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        size_type find_last_of(value_type c, size_type pos = npos) const
        {
            return rfind(c, pos);
        }

        size_type find_last_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_last_of(): received nullptr");
            return StringInternal::find_last_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, count);
        }

        size_type find_last_of(const_pointer s, size_type pos = npos) const
        {
            AZ_Assert(s, "string_view::find_last_of(): received nullptr");
            return StringInternal::find_last_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, traits_type::length(s));
        }

        // find_first_not_of
        size_type find_first_not_of(basic_string_view s, size_type pos = 0) const
        {
            AZ_Assert(s.size() == 0 || s.data(), "string_view::find_first_not_of(): received nullptr");
            return StringInternal::find_first_not_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        size_type find_first_not_of(value_type c, size_type pos = 0) const
        {
            return StringInternal::find_first_not_of<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        size_type find_first_not_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_first_not_of(): received nullptr");
            return StringInternal::find_first_not_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, count);
        }

        size_type find_first_not_of(const_pointer s, size_type pos = 0) const
        {
            AZ_Assert(s, "string_view::find_first_not_of(): received nullptr");
            return StringInternal::find_first_not_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, traits_type::length(s));
        }

        // find_last_not_of
        size_type find_last_not_of(basic_string_view s, size_type pos = npos) const
        {
            AZ_Assert(s.size() == 0 || s.data(), "string_view::find_last_not_of(): received nullptr");
            return StringInternal::find_last_not_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        size_type find_last_not_of(value_type c, size_type pos = npos) const
        {
            return StringInternal::find_last_not_of<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        size_type find_last_not_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_last_not_of(): received nullptr");
            return StringInternal::find_last_not_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, count);
        }

        size_type find_last_not_of(const_pointer s, size_type pos = npos) const
        {
            AZ_Assert(s, "string_view::find_last_not_of(): received nullptr");
            return StringInternal::find_last_not_of<value_type, size_type, traits_type, npos>(data(), size(), s, pos, traits_type::length(s));
        }

    private:
        const_pointer m_begin;
        const_pointer m_end;
    };

    template <class Element, class Traits>
    const typename basic_string_view<Element, Traits>::size_type basic_string_view<Element, Traits>::npos;

    using string_view = basic_string_view<char>;
    using wstring_view = basic_string_view<wchar_t>;

    template<class Element, class Traits = AZStd::char_traits<Element>>
    using basic_const_string = basic_string_view<Element, Traits>;
    using const_string = string_view;
    using const_wstring = wstring_view;

    /// For string hashing we are using FNV-1a algorithm with 32 and 64 bit versions.
    template<class RandomAccessIterator>
    AZ_FORCE_INLINE AZStd::size_t hash_string(RandomAccessIterator first, AZStd::size_t length)
    {
        size_t hash = 14695981039346656037ULL;
        const size_t fnvPrime = 1099511628211ULL;
        const char* cptr = reinterpret_cast<const char*>(&(*first));
        for (; length; --length)
        {
            hash ^= static_cast<size_t>(*cptr++);
            hash *= fnvPrime;
        }
        return hash;
    }

    template<class Element, class Traits>
    struct hash<basic_string_view<Element, Traits>>
    {
        using argument_type = basic_string_view<Element, Traits>;
        using result_type = AZStd::size_t;

        inline result_type operator()(const argument_type& value) const
        {
            // from the string class
            return hash_string(value.begin(), value.length());
        }
    };
} // namespace AZStd
