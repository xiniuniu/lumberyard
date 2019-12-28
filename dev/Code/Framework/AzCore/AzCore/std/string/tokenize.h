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
#ifndef AZSTD_TOKENIZE_CONVERSIONS_H
#define AZSTD_TOKENIZE_CONVERSIONS_H

#include <AzCore/std/string/string.h>

namespace AZStd
{
    template<class S>
    void tokenize(const S& str, const S& delimiters, vector<S>& tokens)
    {
        tokens.clear();
        if (str.empty())
        {
            return;
        }

        // skip delimiters at beginning.
        string::size_type lastPos = str.find_first_not_of(delimiters, 0);

        // find first "non-delimiter".
        string::size_type pos = str.find_first_of(delimiters, lastPos);

        while (string::npos != pos || string::npos != lastPos)
        {
            // found a token, add it to the vector.
            tokens.push_back(str.substr(lastPos, pos - lastPos));

            // skip delimiters.  Note the "not_of"
            lastPos = str.find_first_not_of(delimiters, pos);

            // find next "non-delimiter"
            pos = str.find_first_of(delimiters, lastPos);
        }
    }

    template<class S>
    void tokenize_keep_empty(const S& str, const S& delimiters, vector<S>& tokens)
    {
        tokens.clear();
        if (str.empty())
        {
            return;
        }

        string::size_type delimPos = 0, tokenPos = 0, pos = 0;

        while (true)
        {
            delimPos = str.find_first_of(delimiters, pos);
            tokenPos = str.find_first_not_of(delimiters, pos);

            if (string::npos != delimPos)
            {
                if (string::npos != tokenPos)
                {
                    if (tokenPos < delimPos)
                    {
                        tokens.push_back(str.substr(pos, delimPos - pos));
                    }
                    else
                    {
                        tokens.push_back("");
                    }
                }
                else
                {
                    tokens.push_back("");
                }
                pos = delimPos + 1;
            }
            else
            {
                if (string::npos != tokenPos)
                {
                    tokens.push_back(str.substr(pos));
                }
                else
                {
                    tokens.push_back("");
                }
                break;
            }
        }
    }
}



#endif // AZSTD_TOKENIZE_CONVERSIONS_H
#pragma once
