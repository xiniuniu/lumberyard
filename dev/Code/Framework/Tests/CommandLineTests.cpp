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

#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/CommandLine/CommandLine.h>

using namespace AzFramework;


namespace UnitTest
{
    class CommandLineTests
        : public AllocatorsFixture
    {
    };

    TEST_F(CommandLineTests, CommandLineParser_Sanity)
    {
        CommandLine cmd;
        EXPECT_FALSE(cmd.HasSwitch(""));
        EXPECT_EQ(cmd.GetNumSwitchValues("haha"), 0);
        EXPECT_EQ(cmd.GetSwitchValue("haha", 0), AZStd::string());
        EXPECT_EQ(cmd.GetNumMiscValues(), 0);

        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_EQ(cmd.GetMiscValue(1), AZStd::string());
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
    }

    TEST_F(CommandLineTests, CommandLineParser_Switches_Simple)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", "/switch1", "test", "/switch2", "test2", "/switch3", "tEST3"
        };

        cmd.Parse(7, const_cast<char**>(argValues));

        EXPECT_FALSE(cmd.HasSwitch("switch4"));
        EXPECT_TRUE(cmd.HasSwitch("switch3"));
        EXPECT_TRUE(cmd.HasSwitch("sWITCH2")); // expect case insensitive
        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetNumSwitchValues("switch1"), 1);
        EXPECT_EQ(cmd.GetNumSwitchValues("switch2"), 1);
        EXPECT_EQ(cmd.GetNumSwitchValues("switch3"), 1);

        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "test");
        EXPECT_EQ(cmd.GetSwitchValue("switch2", 0), "test2");
        EXPECT_EQ(cmd.GetSwitchValue("switch3", 0), "tEST3"); // retain case in values.

        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), AZStd::string());
        EXPECT_EQ(cmd.GetSwitchValue("switch2", 1), AZStd::string());
        EXPECT_EQ(cmd.GetSwitchValue("switch3", 1), AZStd::string());
        AZ_TEST_STOP_TRACE_SUPPRESSION(3);
    }

    TEST_F(CommandLineTests, CommandLineParser_MiscValues_Simple)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", "/switch1", "test", "miscvalue1", "miscvalue2"
        };

        cmd.Parse(5, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetNumSwitchValues("switch1"), 1);
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "test");
        EXPECT_EQ(cmd.GetNumMiscValues(), 2);
        EXPECT_EQ(cmd.GetMiscValue(0), "miscvalue1");
        EXPECT_EQ(cmd.GetMiscValue(1), "miscvalue2");
    }

    TEST_F(CommandLineTests, CommandLineParser_Complex)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", "-switch1", "test", "--switch1", "test2", "/switch2", "otherswitch", "miscvalue", "/switch3=abc,def", "miscvalue2", "/switch3", "hij,klm"
        };

        cmd.Parse(12, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_TRUE(cmd.HasSwitch("switch2"));
        EXPECT_EQ(cmd.GetNumMiscValues(), 2);
        EXPECT_EQ(cmd.GetMiscValue(0), "miscvalue");
        EXPECT_EQ(cmd.GetMiscValue(1), "miscvalue2");
        EXPECT_EQ(cmd.GetNumSwitchValues("switch1"), 2);
        EXPECT_EQ(cmd.GetNumSwitchValues("switch2"), 1);
        EXPECT_EQ(cmd.GetNumSwitchValues("switch3"), 4);
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "test");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "test2");
        EXPECT_EQ(cmd.GetSwitchValue("switch2", 0), "otherswitch");
        EXPECT_EQ(cmd.GetSwitchValue("switch3", 0), "abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch3", 1), "def");
        EXPECT_EQ(cmd.GetSwitchValue("switch3", 2), "hij");
        EXPECT_EQ(cmd.GetSwitchValue("switch3", 3), "klm");
    }

    TEST_F(CommandLineTests, CommandLineParser_WhitespaceTolerant)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", "/switch1 ", "test ", " /switch1", " test2", " --switch1", " abc, def ", " /switch1 = abc, def " 
        };

        cmd.Parse(8, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "test");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "test2");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 2), "abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 3), "def");

        // note:  Every switch must appear in the order it is given, even duplicates.
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 4), "abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 5), "def");
    }


    TEST_F(CommandLineTests, CommandLineParser_QuoteBoundNoEqualWithComma_Success)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1 " ,"\"acb,def\""
        };

        cmd.Parse(3, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "acb,def");

    }

    TEST_F(CommandLineTests, CommandLineParser_QuoteBoundEqualWithCommaNoSpace_Success)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\"abc,fde\""
        };

        cmd.Parse(2, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "abc,fde");
    }

    TEST_F(CommandLineTests, CommandLineParser_QuoteBoundEqualWithCommaSpace_Success)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\"abc, def\""
        };

        cmd.Parse(2, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "abc, def");
    }

    TEST_F(CommandLineTests, CommandLineParser_SingleQuoteEqualWithCommaSpace_Tokenized)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\"abc, def"
        };

        cmd.Parse(2, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "\"abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "def");

    }

    TEST_F(CommandLineTests, CommandLineParser_SingleQuoteEqualWithCommaNoSpace_Tokenized)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\"abc,def"
        };

        cmd.Parse(2, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "\"abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "def");

    }


    TEST_F(CommandLineTests, CommandLineParser_SingleQuoteNoEqualWithCommaNoSpace_Tokenized)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1", " \"abc,def" 
        };

        cmd.Parse(3, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "\"abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "def");

    }


    TEST_F(CommandLineTests, CommandLineParser_SingleQuoteNoEqualWithCommaSpace_Tokenized)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1", "\"abc, def"
        };

        cmd.Parse(3, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "\"abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "def");

    }

    TEST_F(CommandLineTests, CommandLineParser_DoubleQuoteNoEqual_Blank)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", "/switch1", "\"\""
        };

        cmd.Parse(3, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "");

    }

    TEST_F(CommandLineTests, CommandLineParser_DoubleQuote_Blank)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\"\"", " /switch2", "\"\""
        };

        cmd.Parse(4, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "");

        EXPECT_TRUE(cmd.HasSwitch("switch2"));
        EXPECT_EQ(cmd.GetSwitchValue("switch2", 0), "");
    }

    TEST_F(CommandLineTests, CommandLineParser_DoubleQuoteEqualComma_Comma)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\",\""
        };

        cmd.Parse(2, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), ",");

    }

    TEST_F(CommandLineTests, CommandLineParser_SingleDoubleQuote_Success)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\"", " /switch2", "\""
        };

        cmd.Parse(4, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "\"");

        EXPECT_TRUE(cmd.HasSwitch("switch2"));
        EXPECT_EQ(cmd.GetSwitchValue("switch2", 0), "\"");
    }

    // Verify things can "work the previous way" - what if my desired parameter starts and ends with quotes?
    // -- You can simply start or end with the token and things will work
    TEST_F(CommandLineTests, CommandLineParser_QuoteCommaStartOrEndWithCommaNoSpace_Tokenized)
    {
        CommandLine cmd;

        const char* argValues[] = {
            "programname.exe", " /switch1=\"abc,fde\",", "/switch2", " ,\"cba, edf\""
        };

        cmd.Parse(4, const_cast<char**>(argValues));

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "\"abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "fde\"");

        EXPECT_TRUE(cmd.HasSwitch("switch1"));
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 0), "\"abc");
        EXPECT_EQ(cmd.GetSwitchValue("switch1", 1), "fde\"");

        EXPECT_TRUE(cmd.HasSwitch("switch2"));
        EXPECT_EQ(cmd.GetSwitchValue("switch2", 0), "\"cba");
        EXPECT_EQ(cmd.GetSwitchValue("switch2", 1), "edf\"");
    }
}   // namespace UnitTest
