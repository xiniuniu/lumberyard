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

#include <PhysX_precompiled.h>
#include <NameConstants.h>

namespace PhysX
{
    namespace UXNameConstants
    {
        const AZStd::string& GetPhysXDocsRoot()
        {
            static const AZStd::string val = "https://docs.aws.amazon.com/console/lumberyard/physx/";
            return val;
        }
    }

    namespace NameConstants
    {
        // Some of these constants include UTF-8 hex values which generate unicode characters needed for some units such
        // as superscripts when QT strings are created from them.  To add more unicode characters, find them on a site
        // such as https://www.fileformat.info/info/unicode/char/search.htm and look for the UTF-8 (hex) values.
        const AZStd::string& GetSuperscriptMinus()
        {
            static const AZStd::string superscriptMinus = "\xE2\x81\xBB"; // equivalent to U+207B
            return superscriptMinus;
        }

        const AZStd::string& GetSuperscriptOne()
        {
            static const AZStd::string superscriptOne = "\xC2\xB9"; // equivalent to U+00B9
            return superscriptOne;
        }

        const AZStd::string& GetSuperscriptTwo()
        {
            static const AZStd::string superscriptTwo = "\xC2\xB2"; // equivalent to U+00B2
            return superscriptTwo;
        }

        const AZStd::string& GetInterpunct()
        {
            static const AZStd::string interpunct = "\xC2\xB7"; // equivalent to U+00B7, also known as middle dot
            return interpunct;
        }

        const AZStd::string& GetSpeedUnit()
        {
            static const AZStd::string speedUnit = AZStd::string::format("m%ss%s%s",
                GetInterpunct().c_str(), GetSuperscriptMinus().c_str(), GetSuperscriptOne().c_str());
            return speedUnit;
        }

        const AZStd::string& GetAngularVelocityUnit()
        {
            static const AZStd::string angularVelocityUnit = AZStd::string::format("rad%ss%s%s",
                GetInterpunct().c_str(), GetSuperscriptMinus().c_str(), GetSuperscriptOne().c_str());
            return angularVelocityUnit;
        }

        const AZStd::string& GetLengthUnit()
        {
            static const AZStd::string lengthUnit = "m";
            return lengthUnit;
        }

        const AZStd::string& GetMassUnit()
        {
            static const AZStd::string massUnit = "kg";
            return massUnit;
        }

        const AZStd::string& GetInertiaUnit()
        {
            static const AZStd::string inertiaUnit = AZStd::string::format("kg%sm%s",
                GetInterpunct().c_str(), GetSuperscriptTwo().c_str());
            return inertiaUnit;
        }

        const AZStd::string& GetSleepThresholdUnit()
        {
            static const AZStd::string sleepThresholdUnit = AZStd::string::format("m%ss%s%s",
                GetSuperscriptTwo().c_str(), GetInterpunct().c_str(), GetSuperscriptTwo().c_str());
            return sleepThresholdUnit;
        }
    } // namespace NameConstants
} // namespace PhysX
