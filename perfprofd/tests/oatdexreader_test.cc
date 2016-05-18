/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <regex>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <android-base/file.h>
#include <cutils/properties.h>

#include "perfprofdmockutils.h"
#include "oatmapper.h"
#include "oatreader.h"
#include "oatdexvisitor.h"
#include "dexread.h"

#include "perf_profile.pb.h"
#include "oatmap.pb.h"
#include "google/protobuf/text_format.h"

//
// Set to argv[0] on startup
//
static const char *executable_path;

//
// test_dir is the directory containing the test executable and
// any files associated with the test (will be created by the harness).
//
static std::string test_dir;

class OatDexReadTest : public testing::Test {
 protected:
  virtual void SetUp() {
    mock_perfprofdutils_init();
    if (test_dir == "") {
      ASSERT_TRUE(executable_path != nullptr);
      std::string s(executable_path);
      auto found = s.find_last_of("/");
      test_dir = s.substr(0,found);
    }
  }

  virtual void TearDown() {
    mock_perfprofdutils_finish();
  }
};

static bool bothWhiteSpace(char lhs, char rhs)
{
  return (std::isspace(lhs) && std::isspace(rhs));
}

//
// Squeeze out repeated whitespace from expected/actual logs.
//
static std::string squeezeWhite(const std::string &str,
                                const char *tag,
                                bool dump=false)
{
  if (dump) { fprintf(stderr, "raw %s is %s\n", tag, str.c_str()); }
  std::string result(str);
  std::replace( result.begin(), result.end(), '\n', ' ');
  auto new_end = std::unique(result.begin(), result.end(), bothWhiteSpace);
  result.erase(new_end, result.end());
  while (result.begin() != result.end() && std::isspace(*result.rbegin())) {
    result.pop_back();
  }
  if (dump) { fprintf(stderr, "squeezed %s is %s\n", tag, result.c_str()); }
  return result;
}

#define RAW_RESULT(x) #x

class CaptureDexVisitor : public OatDexVisitor {
 public:
  CaptureDexVisitor() { }
  ~CaptureDexVisitor() { }

  void visitOAT(bool is64bit,
                uint64_t &base_text) {
    ss_ << "OAT is64=" << (is64bit ? "yes" : "no")
        << "base_text=0x" << std::hex << base_text << "\n";
  }

  void visitDEX(unsigned char sha1sig[20]) {
    ss_ << "DEX sha1 ";
    for (unsigned i = 0; i < 20; ++i)
      ss_ << std::hex << (int) sha1sig[i];
    ss_ << "\n";
  }

  void visitClass(const std::string &className, uint32_t nMethods) {
    ss_ << std::dec << " class " << className << " nmethods=" << nMethods << "\n";
  }

  void visitMethod(const std::string &methodName,
                   unsigned methodIdx,
                   uint32_t numInstrs,
                   uint64_t *nativeCodeOffset) {
    ss_ << std::dec << " method " << methodIdx
        << " name=" << methodName
        << " numInstrs="
        << numInstrs << "\n";
  }

  std::string result() { return ss_.str(); }

 private:
  std::stringstream ss_;
};

TEST_F(OatDexReadTest, BasicDexRead)
{
  //
  // Visit a DEX file
  //
  CaptureDexVisitor vis;
  std::string dexfile(test_dir);
  dexfile += "/smallish.dex";
  auto res = examineDexFile(dexfile, vis);
  ASSERT_TRUE(res);

  //
  // Test for correct output
  //
  const std::string dex_expected = RAW_RESULT(
      DEX sha1 fd56aced78355c305a953d6f3dfe1f7ff6ac440
      class Lfibonacci nmethods=6
      method 0 name=<init> numInstrs=4
      method 1 name=ifibonacci numInstrs=16
      method 2 name=main numInstrs=159
      method 3 name=rcnm1 numInstrs=7
      method 4 name=rcnm2 numInstrs=7
      method 5 name=rfibonacci numInstrs=17
                                              );
  std::string sqexp = squeezeWhite(dex_expected, "expected");
  std::string result = vis.result();
  std::string sqact = squeezeWhite(result, "actual");
  EXPECT_STREQ(sqexp.c_str(), sqact.c_str());
}

TEST_F(OatDexReadTest, BasicOatDexRead)
{
  //
  // Read DEX file into memory and visit.
  //
  std::string oatfile(test_dir);
  oatfile += "/../perfprofd_test/smallish.odex";

  //
  // Visit
  //
  CaptureDexVisitor vis;
  examineOatFile(oatfile, vis);

  //
  // Test for correct output (full output
  //
  std::string oat_expected = RAW_RESULT(
      OAT is64=nobase_text=0xe000 DEX sha1 4e3e47a666a0de661f688fd82fc2dfd9dc38e99c
      class Lcom.android.ex.editstyledtext.EditStyledText$EditModeActions$EditModeActionBase nmethods=6
      method 100 name=addParams numInstrs=3
      method 101 name=doEndPosIsSelected numInstrs=5
      method 102 name=doNotSelected numInstrs=2
      method 103 name=doSelectionIsFixed numInstrs=5
      method 104 name=doSelectionIsFixedAndWaitingInput numInstrs=5
      method 105 name=doStartPosIsSelected numInstrs=5
      class Lcom.android.ex.editstyledtext.EditStyledText$EditModeActions nmethods=5
      method 107 name=getAction numInstrs=27
      method 106 name=doNext numInstrs=116
      method 108 name=onAction numInstrs=5
      method 109 name=onAction numInstrs=13
      method 110 name=onSelectAction numInstrs=5
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextNotifier nmethods=3
      method 111 name=isButtonsFocused numInstrs=2789686862
      method 112 name=onStateChanged numInstrs=2789686862
      method 113 name=sendOnTouchEvent numInstrs=2789686862
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$HorizontalLineDrawable nmethods=6
      method 114 name=<clinit> numInstrs=4
      method 117 name=getParentSpan numInstrs=44
      method 119 name=renewColor numInstrs=69
      method 120 name=renewColor numInstrs=38
      method 115 name=draw numInstrs=23
      method 118 name=renewBounds numInstrs=45
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$HorizontalLineSpan nmethods=2
      method 122 name=getDrawable numInstrs=3
      method 123 name=resetWidth numInstrs=6
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$MarqueeSpan nmethods=3
      method 124 name=getMarqueeColor numInstrs=74
      method 125 name=resetColor numInstrs=9
      method 126 name=updateDrawState numInstrs=5
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$RescalableImageSpan nmethods=2
      method 128 name=rescaleBigImage numInstrs=88
      method 127 name=getDrawable numInstrs=190
      class Lcom.android.ex.editstyledtext.EditStyledText$EditorManager nmethods=32
      method 131 name=endEdit numInstrs=44
      method 132 name=findLineEnd numInstrs=73
      method 133 name=findLineStart numInstrs=69
      method 134 name=fixSelectionAndDoNextAction numInstrs=111
      method 137 name=handleSelectAll numInstrs=13
      method 151 name=removeImageChar numInstrs=49
      method 152 name=resetEdit numInstrs=16
      method 157 name=unsetSelect numInstrs=35
      method 129 name=blockSoftKey numInstrs=16
      method 130 name=canPaste numInstrs=27
      method 135 name=getBackgroundColor numInstrs=3
      method 136 name=getSelectState numInstrs=3
      method 138 name=hideSoftKey numInstrs=74
      method 139 name=isEditting numInstrs=3
      method 140 name=isSoftKeyBlocked numInstrs=3
      method 141 name=isStyledText numInstrs=54
      method 142 name=isWaitInput numInstrs=3
      method 143 name=onAction numInstrs=5
      method 144 name=onAction numInstrs=17
      method 145 name=onClearStyles numInstrs=8
      method 146 name=onCursorMoved numInstrs=34
      method 147 name=onFixSelectedItem numInstrs=22
      method 148 name=onRefreshStyles numInstrs=104
      method 149 name=onStartSelect numInstrs=42
      method 150 name=onStartSelectAll numInstrs=24
      method 153 name=setBackgroundColor numInstrs=3
      method 154 name=setTextComposingMask numInstrs=197
      method 155 name=showSoftKey numInstrs=92
      method 156 name=unblockSoftKey numInstrs=13
      method 158 name=unsetTextComposingMask numInstrs=28
      method 159 name=updateSpanNextToCursor numInstrs=237
      method 160 name=updateSpanPreviousFromCursor numInstrs=269
      class Lcom.android.ex.editstyledtext.EditStyledText$MenuHandler nmethods=3
      method 161 name=<init> numInstrs=6
      method 162 name=<init> numInstrs=4
      method 163 name=onMenuItemClick numInstrs=11
      class Lcom.android.ex.editstyledtext.EditStyledText$SavedStyledTextState nmethods=3
      method 164 name=<init> numInstrs=4
      method 166 name=toString numInstrs=49
      method 167 name=writeToParcel numInstrs=9
      class Lcom.android.ex.editstyledtext.EditStyledText$SoftKeyReceiver nmethods=1
      method 168 name=onReceiveResult numInstrs=17
      class Lcom.android.ex.editstyledtext.EditStyledText$StyledTextInputConnection nmethods=3
      method 169 name=<init> numInstrs=7
      method 170 name=commitText numInstrs=23
      method 171 name=finishComposingText numInstrs=44
      class Lcom.android.ex.editstyledtext.EditStyledText nmethods=35
      method 172 name=-get1 numInstrs=3
      method 173 name=-wrap13 numInstrs=4
      method 174 name=-wrap6 numInstrs=4
      method 175 name=<clinit> numInstrs=8
      method 192 name=notifyStateChanged numInstrs=27
      method 200 name=onRefreshStyles numInstrs=6
      method 212 name=sendOnTouchEvent numInstrs=27
      method 218 name=stopSelecting numInstrs=6
      method 177 name=drawableStateChanged numInstrs=13
      method 178 name=getBackgroundColor numInstrs=7
      method 180 name=getForegroundColor numInstrs=39
      method 181 name=getSelectState numInstrs=7
      method 187 name=isButtonsFocused numInstrs=30
      method 188 name=isEditting numInstrs=7
      method 190 name=isSoftKeyBlocked numInstrs=7
      method 191 name=isStyledText numInstrs=7
      method 193 name=onClearStyles numInstrs=6
      method 194 name=onCreateContextMenu numInstrs=75
      method 195 name=onCreateInputConnection numInstrs=14
      method 196 name=onEndEdit numInstrs=8
      method 197 name=onFixSelectedItem numInstrs=6
      method 198 name=onFocusChanged numInstrs=19
      method 199 name=onInsertHorizontalLine numInstrs=8
      method 201 name=onRestoreInstanceState numInstrs=24
      method 202 name=onSaveInstanceState numInstrs=18
      method 203 name=onStartCopy numInstrs=7
      method 204 name=onStartCut numInstrs=7
      method 205 name=onStartEdit numInstrs=8
      method 206 name=onStartPaste numInstrs=7
      method 207 name=onStartSelect numInstrs=7
      method 208 name=onStartSelectAll numInstrs=7
      method 209 name=onTextChanged numInstrs=71
      method 210 name=onTextContextMenuItem numInstrs=132
      method 211 name=onTouchEvent numInstrs=99
      method 213 name=setBackgroundColor numInstrs=23
      class Lcom.android.smspush.WapPushManager$IWapPushManagerStub nmethods=7
      method 220 name=<init> numInstrs=6
      method 221 name=addPackage numInstrs=196
      method 222 name=appTypeCheck numInstrs=8
      method 223 name=deletePackage numInstrs=109
      method 224 name=processMessage numInstrs=258
      method 225 name=signatureCheck numInstrs=21
      method 226 name=updatePackage numInstrs=213
      class Lcom.android.smspush.WapPushManager$WapPushManDBHelper$queryData nmethods=1
      method 227 name=<init> numInstrs=6
      class Lcom.android.smspush.WapPushManager$WapPushManDBHelper nmethods=4
      method 228 name=<init> numInstrs=11
      method 231 name=onCreate numInstrs=7
      method 232 name=onUpgrade numInstrs=10
      method 233 name=queryLastApp numInstrs=155
      class Lcom.android.smspush.WapPushManager nmethods=5
      method 234 name=<init> numInstrs=18
      method 235 name=getDatabase numInstrs=14
      method 236 name=isDataExist numInstrs=21
      method 237 name=onBind numInstrs=3
      method 238 name=verifyData numInstrs=60
                                        );
  std::string sqexp = squeezeWhite(oat_expected, "expected");
  std::string result = vis.result();
  std::string sqact = squeezeWhite(result, "actual");
  EXPECT_STREQ(sqexp.c_str(), sqact.c_str());
}

int main(int argc, char **argv) {
  executable_path = argv[0];
  // switch to / before starting testing (all code to be tested
  // should be location-independent)
  chdir("/");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
