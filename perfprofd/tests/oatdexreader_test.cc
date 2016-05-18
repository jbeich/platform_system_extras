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
                   uint64_t methodCodeOffset) {
    ss_ << std::dec << " method " << methodIdx
        << " name=" << methodName
        << " codeOffset="
        << methodCodeOffset << "\n";
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
      method 0 name=<init> codeOffset=584
      method 1 name=ifibonacci codeOffset=608
      method 2 name=main codeOffset=656
      method 3 name=rcnm1 codeOffset=1008
      method 4 name=rcnm2 codeOffset=1040
      method 5 name=rfibonacci codeOffset=1072
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
      class Lcom.android.ex.editstyledtext.EditStyledText$EditModeActions$EditModeActionBase nmethods=6 method 100 name=addParams codeOffset=7652 method 101 name=doEndPosIsSelected codeOffset=7676 method 102 name=doNotSelected codeOffset=7704 method 103 name=doSelectionIsFixed codeOffset=7724 method 104 name=doSelectionIsFixedAndWaitingInput codeOffset=7752 method 105 name=doStartPosIsSelected codeOffset=7780
      class Lcom.android.ex.editstyledtext.EditStyledText$EditModeActions nmethods=5 method 107 name=getAction codeOffset=7808 method 106 name=doNext codeOffset=7880 method 108 name=onAction codeOffset=8128 method 109 name=onAction codeOffset=8156 method 110 name=onSelectAction codeOffset=8200
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextNotifier nmethods=3 method 111 name=isButtonsFocused codeOffset=0 method 112 name=onStateChanged codeOffset=0 method 113 name=sendOnTouchEvent codeOffset=0
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$HorizontalLineDrawable nmethods=6 method 114 name=<clinit> codeOffset=8228 method 117 name=getParentSpan codeOffset=8252 method 119 name=renewColor codeOffset=8356 method 120 name=renewColor codeOffset=8512 method 115 name=draw codeOffset=8604 method 118 name=renewBounds codeOffset=8668
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$HorizontalLineSpan nmethods=2 method 122 name=getDrawable codeOffset=8776 method 123 name=resetWidth codeOffset=8800
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$MarqueeSpan nmethods=3 method 124 name=getMarqueeColor codeOffset=8828 method 125 name=resetColor codeOffset=8992 method 126 name=updateDrawState codeOffset=9028
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$RescalableImageSpan nmethods=2 method 128 name=rescaleBigImage codeOffset=9056 method 127 name=getDrawable codeOffset=9248
      class Lcom.android.ex.editstyledtext.EditStyledText$EditorManager nmethods=32 method 131 name=endEdit codeOffset=9668 method 132 name=findLineEnd codeOffset=9772 method 133 name=findLineStart codeOffset=9936 method 134 name=fixSelectionAndDoNextAction codeOffset=10092 method 137 name=handleSelectAll codeOffset=10332 method 151 name=removeImageChar codeOffset=10376 method 152 name=resetEdit codeOffset=10492 method 157 name=unsetSelect codeOffset=10540 method 129 name=blockSoftKey codeOffset=10628 method 130 name=canPaste codeOffset=10676 method 135 name=getBackgroundColor codeOffset=10748 method 136 name=getSelectState codeOffset=10772 method 138 name=hideSoftKey codeOffset=10796 method 139 name=isEditting codeOffset=10960 method 140 name=isSoftKeyBlocked codeOffset=10984 method 141 name=isStyledText codeOffset=11008 method 142 name=isWaitInput codeOffset=11132 method 143 name=onAction codeOffset=11156 method 144 name=onAction codeOffset=11184 method 145 name=onClearStyles codeOffset=11236 method 146 name=onCursorMoved codeOffset=11268 method 147 name=onFixSelectedItem codeOffset=11352 method 148 name=onRefreshStyles codeOffset=11412 method 149 name=onStartSelect codeOffset=11636 method 150 name=onStartSelectAll codeOffset=11736 method 153 name=setBackgroundColor codeOffset=11800 method 154 name=setTextComposingMask codeOffset=11824 method 155 name=showSoftKey codeOffset=12236 method 156 name=unblockSoftKey codeOffset=12436 method 158 name=unsetTextComposingMask codeOffset=12480 method 159 name=updateSpanNextToCursor codeOffset=12552 method 160 name=updateSpanPreviousFromCursor codeOffset=13044
      class Lcom.android.ex.editstyledtext.EditStyledText$MenuHandler nmethods=3 method 161 name=<init> codeOffset=13600 method 162 name=<init> codeOffset=13628 method 163 name=onMenuItemClick codeOffset=13652
      class Lcom.android.ex.editstyledtext.EditStyledText$SavedStyledTextState nmethods=3 method 164 name=<init> codeOffset=13692 method 166 name=toString codeOffset=13716 method 167 name=writeToParcel codeOffset=13832
      class Lcom.android.ex.editstyledtext.EditStyledText$SoftKeyReceiver nmethods=1 method 168 name=onReceiveResult codeOffset=13868
      class Lcom.android.ex.editstyledtext.EditStyledText$StyledTextInputConnection nmethods=3 method 169 name=<init> codeOffset=13920 method 170 name=commitText codeOffset=13952 method 171 name=finishComposingText codeOffset=14016
      class Lcom.android.ex.editstyledtext.EditStyledText nmethods=35 method 172 name=-get1 codeOffset=14120 method 173 name=-wrap13 codeOffset=14144 method 174 name=-wrap6 codeOffset=14168 method 175 name=<clinit> codeOffset=14192 method 192 name=notifyStateChanged codeOffset=14224 method 200 name=onRefreshStyles codeOffset=14296 method 212 name=sendOnTouchEvent codeOffset=14324 method 218 name=stopSelecting codeOffset=14396 method 177 name=drawableStateChanged codeOffset=14424 method 178 name=getBackgroundColor codeOffset=14468 method 180 name=getForegroundColor codeOffset=14500 method 181 name=getSelectState codeOffset=14596 method 187 name=isButtonsFocused codeOffset=14628 method 188 name=isEditting codeOffset=14704 method 190 name=isSoftKeyBlocked codeOffset=14736 method 191 name=isStyledText codeOffset=14768 method 193 name=onClearStyles codeOffset=14800 method 194 name=onCreateContextMenu codeOffset=14828 method 195 name=onCreateInputConnection codeOffset=14996 method 196 name=onEndEdit codeOffset=15040 method 197 name=onFixSelectedItem codeOffset=15072 method 198 name=onFocusChanged codeOffset=15100 method 199 name=onInsertHorizontalLine codeOffset=15156 method 201 name=onRestoreInstanceState codeOffset=15188 method 202 name=onSaveInstanceState codeOffset=15252 method 203 name=onStartCopy codeOffset=15304 method 204 name=onStartCut codeOffset=15336 method 205 name=onStartEdit codeOffset=15368 method 206 name=onStartPaste codeOffset=15400 method 207 name=onStartSelect codeOffset=15432 method 208 name=onStartSelectAll codeOffset=15464 method 209 name=onTextChanged codeOffset=15496 method 210 name=onTextContextMenuItem codeOffset=15656 method 211 name=onTouchEvent codeOffset=15936 method 213 name=setBackgroundColor codeOffset=16152
      class Lcom.android.smspush.WapPushManager$IWapPushManagerStub nmethods=7 method 220 name=<init> codeOffset=16216 method 221 name=addPackage codeOffset=16244 method 222 name=appTypeCheck codeOffset=16652 method 223 name=deletePackage codeOffset=16684 method 224 name=processMessage codeOffset=16920 method 225 name=signatureCheck codeOffset=17468 method 226 name=updatePackage codeOffset=17528
      class Lcom.android.smspush.WapPushManager$WapPushManDBHelper$queryData nmethods=1 method 227 name=<init> codeOffset=17972
      class Lcom.android.smspush.WapPushManager$WapPushManDBHelper nmethods=4 method 228 name=<init> codeOffset=18000 method 231 name=onCreate codeOffset=18040 method 232 name=onUpgrade codeOffset=18072 method 233 name=queryLastApp codeOffset=18108
      class Lcom.android.smspush.WapPushManager nmethods=5 method 234 name=<init> codeOffset=18436 method 235 name=getDatabase codeOffset=18488 method 236 name=isDataExist codeOffset=18532 method 237 name=onBind codeOffset=18592 method 238 name=verifyData codeOffset=18616
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
