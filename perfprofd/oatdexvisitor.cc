/*
**
** Copyright 2016, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/


#include "oatdexvisitor.h"

OatDexVisitor::~OatDexVisitor()
{
}

bool OatDexVisitor::doVisitDex()
{
  return true;
}

void OatDexVisitor::visitOAT(bool /* is64bit */,
                             uint32_t /* adler32_checksum */,
                             uint64_t /* executable_offset */,
                             uint64_t /* base_text */)
{
}

void OatDexVisitor::visitDEX(unsigned char[20] /*sha1sig*/)
{
}

void OatDexVisitor::visitClass(const std::string & /* className */,
                               uint32_t /* nMethods */)
{
}

void OatDexVisitor::visitMethod(const std::string & /* methodName */,
                                unsigned /* methodIdx */,
                                uint32_t /* numInstrs */,
                                uint64_t * /* nativeCodeOffset */,
                                uint32_t * /* nativeCodeSize */)
{
}

OatReaderHooks::~OatReaderHooks()
{
}
