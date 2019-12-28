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

#include <AzCore/IO/FileIO.h>
#include <gmock/gmock.h>

// the following was generated using google's python script to autogenerate mocks.

namespace AZ
{
    namespace IO
    {
        class MockFileIOBase;

        using NiceFileIOBaseMock = ::testing::NiceMock<MockFileIOBase>;

        class MockFileIOBase
            : public FileIOBase
        {
        public:
            static const AZ::IO::HandleType FakeFileHandle = AZ::IO::HandleType(1234);

            MOCK_METHOD3(Open,  Result(const char* filePath, OpenMode mode, HandleType & fileHandle));
            MOCK_METHOD1(Close, Result(HandleType fileHandle));
            MOCK_METHOD2(Tell,  Result(HandleType fileHandle, AZ::u64& offset));
            MOCK_METHOD3(Seek,  Result(HandleType fileHandle, AZ::s64 offset, SeekType type));
            MOCK_METHOD5(Read,  Result(HandleType, void*, AZ::u64, bool, AZ::u64*));
            MOCK_METHOD4(Write, Result(HandleType, const void*, AZ::u64, AZ::u64*));
            MOCK_METHOD1(Flush, Result(HandleType fileHandle));
            MOCK_METHOD1(Eof,   bool(HandleType fileHandle));
            MOCK_METHOD1(ModificationTime, AZ::u64(HandleType fileHandle));
            MOCK_METHOD1(ModificationTime, AZ::u64(const char* filePath));
            MOCK_METHOD2(Size, Result(const char* filePath, AZ::u64& size));
            MOCK_METHOD2(Size, Result(HandleType fileHandle, AZ::u64& size));
            MOCK_METHOD1(Exists, bool(const char* filePath));
            MOCK_METHOD1(IsDirectory, bool(const char* filePath));
            MOCK_METHOD1(IsReadOnly,  bool(const char* filePath));
            MOCK_METHOD1(CreatePath,  Result(const char* filePath));
            MOCK_METHOD1(DestroyPath, Result(const char* filePath));
            MOCK_METHOD1(Remove,      Result(const char* filePath));
            MOCK_METHOD2(Copy,        Result(const char* sourceFilePath, const char* destinationFilePath));
            MOCK_METHOD2(Rename,      Result(const char* originalFilePath, const char* newFilePath));
            MOCK_METHOD3(FindFiles,   Result(const char* filePath, const char* filter, FindFilesCallbackType callback));
            MOCK_METHOD2(SetAlias,    void(const char* alias, const char* path));
            MOCK_METHOD1(ClearAlias,  void(const char* alias));
            MOCK_METHOD1(GetAlias,    const char*(const char* alias));
            MOCK_CONST_METHOD2(ConvertToAlias, AZ::u64(char* inOutBuffer, AZ::u64 bufferLength));
            MOCK_METHOD3(ResolvePath, bool(const char* path, char* resolvedPath, AZ::u64 resolvedPathSize));
            MOCK_CONST_METHOD3(GetFilename, bool(HandleType fileHandle, char* filename, AZ::u64 filenameSize));

            /** Installs some default result values for the above functions.
            *   Note that you can always override these in scope of your test by adding additional ON_CALL / EXPECT_CALL
            *   statements in the body of your test or after calling this function, and yours will take precidence.
            **/ 
            static void InstallDefaultReturns(NiceFileIOBaseMock& target)
            {
                using namespace ::testing;
           
                ON_CALL(target, FindFiles(_, _, _))
                    .WillByDefault(
                    Return(AZ::IO::Result(AZ::IO::ResultCode::Success)));

                // pretend that Open always succeeds, and sets the fake file handle.
                ON_CALL(target, Open(_, _, _))
                    .WillByDefault(
                    DoAll(
                        SetArgReferee<2>(FakeFileHandle),
                        Return(AZ::IO::Result(AZ::IO::ResultCode::Success))));

                ON_CALL(target, CreatePath(_))
                    .WillByDefault(
                    Return(AZ::IO::Result(AZ::IO::ResultCode::Success)));

                ON_CALL(target, Remove(_))
                    .WillByDefault(
                    Return(AZ::IO::Result(AZ::IO::ResultCode::Success)));

                ON_CALL(target, Eof(_))
                    .WillByDefault(
                    Return(true));

                ON_CALL(target, Close(_))
                    .WillByDefault(
                    Return(AZ::IO::Result(AZ::IO::ResultCode::Success)));

                ON_CALL(target, Copy(_, _))
                    .WillByDefault(
                    Return(AZ::IO::Result(AZ::IO::ResultCode::Success)));

                ON_CALL(target, Write(_, _, _, _))
                    .WillByDefault(
                    Return(AZ::IO::Result(AZ::IO::ResultCode::Success)));

                ON_CALL(target, Read(_, _, _, _, _))
                    .WillByDefault(
                    Return(AZ::IO::Result(AZ::IO::ResultCode::Error)));
            }
        };
    } // namespace IO
} // namespace AZ

