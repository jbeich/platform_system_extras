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
                uint32_t adler32_checksum,
                uint64_t executable_offset,
                uint64_t base_text) {
    ss_ << "OAT is64=" << (is64bit ? "yes" : "no")
        << " checksum=" << std::hex << adler32_checksum
        << " executable_offset=0x" << std::hex << executable_offset
        << " base_text=0x" << std::hex << base_text << "\n";
  }

  void visitDEX(unsigned char sha1sig[20]) {
    ss_ << "DEX sha1 ";
    for (unsigned i = 0; i < 20; ++i)
      ss_ << std::hex << (int) sha1sig[i];
    ss_ << "\n";
  }

  void visitClass(const std::string &className, uint32_t nMethods) {
    ss_ << std::dec << " class " << className
        << " nmethods=" << nMethods << "\n";
  }

  void visitMethod(const std::string &methodName,
                   unsigned dexMethodIdx,
                   uint32_t numDexInstrs,
                   uint64_t *nativeCodeOffset,
                   uint32_t *nativeCodeSize) {
    ss_ << " method " << std::dec << dexMethodIdx
        << " name=" << methodName
        << " numDexInstrs=" << numDexInstrs;
    if (nativeCodeOffset)
      ss_ << " nativeCodeOffset=" << std::hex << *nativeCodeOffset;
    if (nativeCodeSize)
      ss_ << " nativeCodeSize=" << std::dec << *nativeCodeSize;
    ss_ << "\n";
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
      method 0 name=<init> numDexInstrs=4
      method 1 name=ifibonacci numDexInstrs=16
      method 2 name=main numDexInstrs=159
      method 3 name=rcnm1 numDexInstrs=7
      method 4 name=rcnm2 numDexInstrs=7
      method 5 name=rfibonacci numDexInstrs=17
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
      OAT is64=no checksum=772a7b8f base_text=0xe000 DEX sha1 4e3e47a666a0de661f688fd82fc2dfd9dc38e99c
      class Lcom.android.ex.editstyledtext.EditStyledText$EditModeActions$EditModeActionBase nmethods=6
      method 100 name=addParams numDexInstrs=3 nativeCodeOffset=d015 nativeCodeSize=14
      method 101 name=doEndPosIsSelected numDexInstrs=5 nativeCodeOffset=d03d nativeCodeSize=54
      method 102 name=doNotSelected numDexInstrs=2 nativeCodeOffset=d08d nativeCodeSize=4
      method 103 name=doSelectionIsFixed numDexInstrs=5 nativeCodeOffset=d0a5 nativeCodeSize=54
      method 104 name=doSelectionIsFixedAndWaitingInput numDexInstrs=5 nativeCodeOffset=d0a5 nativeCodeSize=54
      method 105 name=doStartPosIsSelected numDexInstrs=5 nativeCodeOffset=d0f5 nativeCodeSize=54
      class Lcom.android.ex.editstyledtext.EditStyledText$EditModeActions nmethods=5
      method 107 name=getAction numDexInstrs=27 nativeCodeOffset=d145 nativeCodeSize=192
      method 106 name=doNext numDexInstrs=116 nativeCodeOffset=d21d nativeCodeSize=628
      method 108 name=onAction numDexInstrs=5 nativeCodeOffset=d4a5 nativeCodeSize=60
      method 109 name=onAction numDexInstrs=13 nativeCodeOffset=d4f5 nativeCodeSize=232
      method 110 name=onSelectAction numDexInstrs=5 nativeCodeOffset=d5f5 nativeCodeSize=56
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextNotifier nmethods=3
      method 111 name=isButtonsFocused numDexInstrs=2789686862
      method 112 name=onStateChanged numDexInstrs=2789686862
      method 113 name=sendOnTouchEvent numDexInstrs=2789686862
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$HorizontalLineDrawable nmethods=6
      method 114 name=<clinit> numDexInstrs=4
      method 117 name=getParentSpan numDexInstrs=44 nativeCodeOffset=da9d nativeCodeSize=138
      method 119 name=renewColor numDexInstrs=69 nativeCodeOffset=db3d nativeCodeSize=272
      method 120 name=renewColor numDexInstrs=38 nativeCodeOffset=8 nativeCodeSize=3749680
      method 115 name=draw numDexInstrs=23 nativeCodeOffset=dc65 nativeCodeSize=4
      method 118 name=renewBounds numDexInstrs=45 nativeCodeOffset=dc7d nativeCodeSize=62
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$HorizontalLineSpan nmethods=2
      method 122 name=getDrawable numDexInstrs=3 nativeCodeOffset=dc65 nativeCodeSize=4
      method 123 name=resetWidth numDexInstrs=6 nativeCodeOffset=dc7d nativeCodeSize=62
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$MarqueeSpan nmethods=3
      method 124 name=getMarqueeColor numDexInstrs=74 nativeCodeOffset=dcd5 nativeCodeSize=244
      method 125 name=resetColor numDexInstrs=9 nativeCodeOffset=dddd nativeCodeSize=72
      method 126 name=updateDrawState numDexInstrs=5 nativeCodeOffset=de3d nativeCodeSize=14
      class Lcom.android.ex.editstyledtext.EditStyledText$EditStyledTextSpans$RescalableImageSpan nmethods=2
      method 128 name=rescaleBigImage numDexInstrs=88 nativeCodeOffset=de65 nativeCodeSize=408
      method 127 name=getDrawable numDexInstrs=190 nativeCodeOffset=e015 nativeCodeSize=960
      class Lcom.android.ex.editstyledtext.EditStyledText$EditorManager nmethods=32
      method 131 name=endEdit numDexInstrs=44 nativeCodeOffset=e3ed nativeCodeSize=196
      method 132 name=findLineEnd numDexInstrs=73 nativeCodeOffset=e4c5 nativeCodeSize=408
      method 133 name=findLineStart numDexInstrs=69 nativeCodeOffset=e675 nativeCodeSize=388
      method 134 name=fixSelectionAndDoNextAction numDexInstrs=111 nativeCodeOffset=e80d nativeCodeSize=616
      method 137 name=handleSelectAll numDexInstrs=13 nativeCodeOffset=ea8d nativeCodeSize=70
      method 151 name=removeImageChar numDexInstrs=49 nativeCodeOffset=eaed nativeCodeSize=460
      method 152 name=resetEdit numDexInstrs=16 nativeCodeOffset=eccd nativeCodeSize=148
      method 157 name=unsetSelect numDexInstrs=35 nativeCodeOffset=ed75 nativeCodeSize=240
      method 129 name=blockSoftKey numDexInstrs=16 nativeCodeOffset=ee7d nativeCodeSize=144
      method 130 name=canPaste numDexInstrs=27 nativeCodeOffset=ef25 nativeCodeSize=122
      method 135 name=getBackgroundColor numDexInstrs=3 nativeCodeOffset=efb5 nativeCodeSize=4
      method 136 name=getSelectState numDexInstrs=3 nativeCodeOffset=efcd nativeCodeSize=4
      method 138 name=hideSoftKey numDexInstrs=74 nativeCodeOffset=efe5 nativeCodeSize=364
      method 139 name=isEditting numDexInstrs=3 nativeCodeOffset=f165 nativeCodeSize=6
      method 140 name=isSoftKeyBlocked numDexInstrs=3 nativeCodeOffset=f185 nativeCodeSize=6
      method 141 name=isStyledText numDexInstrs=54 nativeCodeOffset=f1a5 nativeCodeSize=454
      method 142 name=isWaitInput numDexInstrs=3 nativeCodeOffset=f385 nativeCodeSize=6
      method 143 name=onAction numDexInstrs=5 nativeCodeOffset=f3a5 nativeCodeSize=60
      method 144 name=onAction numDexInstrs=17 nativeCodeOffset=f3f5 nativeCodeSize=160
      method 145 name=onClearStyles numDexInstrs=8 nativeCodeOffset=f4ad nativeCodeSize=60
      method 146 name=onCursorMoved numDexInstrs=34 nativeCodeOffset=f4fd nativeCodeSize=212
      method 147 name=onFixSelectedItem numDexInstrs=22 nativeCodeOffset=f5e5 nativeCodeSize=200
      method 148 name=onRefreshStyles numDexInstrs=104 nativeCodeOffset=f6c5 nativeCodeSize=720
      method 149 name=onStartSelect numDexInstrs=42 nativeCodeOffset=f9ad nativeCodeSize=260
      method 150 name=onStartSelectAll numDexInstrs=24 nativeCodeOffset=fac5 nativeCodeSize=220
      method 153 name=setBackgroundColor numDexInstrs=3 nativeCodeOffset=fbb5 nativeCodeSize=4
      method 154 name=setTextComposingMask numDexInstrs=197 nativeCodeOffset=fbcd nativeCodeSize=824
      method 155 name=showSoftKey numDexInstrs=92 nativeCodeOffset=ff1d nativeCodeSize=444
      method 156 name=unblockSoftKey numDexInstrs=13 nativeCodeOffset=100ed nativeCodeSize=128
      method 158 name=unsetTextComposingMask numDexInstrs=28 nativeCodeOffset=10185 nativeCodeSize=172
      method 159 name=updateSpanNextToCursor numDexInstrs=237 nativeCodeOffset=10245 nativeCodeSize=1412
      method 160 name=updateSpanPreviousFromCursor numDexInstrs=269 nativeCodeOffset=107dd nativeCodeSize=1592
      class Lcom.android.ex.editstyledtext.EditStyledText$MenuHandler nmethods=3
      method 161 name=<init> numDexInstrs=6 nativeCodeOffset=10e2d nativeCodeSize=18
      method 162 name=<init> numDexInstrs=4 nativeCodeOffset=10e2d nativeCodeSize=18
      method 163 name=onMenuItemClick numDexInstrs=11 nativeCodeOffset=10e55 nativeCodeSize=84
      class Lcom.android.ex.editstyledtext.EditStyledText$SavedStyledTextState nmethods=3
      method 164 name=<init> numDexInstrs=4 nativeCodeOffset=10ebd nativeCodeSize=64
      method 166 name=toString numDexInstrs=49 nativeCodeOffset=10f15 nativeCodeSize=272
      method 167 name=writeToParcel numDexInstrs=9 nativeCodeOffset=1103d nativeCodeSize=86
      class Lcom.android.ex.editstyledtext.EditStyledText$SoftKeyReceiver nmethods=1
      method 168 name=onReceiveResult numDexInstrs=17 nativeCodeOffset=110ad nativeCodeSize=100
      class Lcom.android.ex.editstyledtext.EditStyledText$StyledTextInputConnection nmethods=3
      method 169 name=<init> numDexInstrs=7 nativeCodeOffset=11125 nativeCodeSize=34
      method 170 name=commitText numDexInstrs=23 nativeCodeOffset=1115d nativeCodeSize=216
      method 171 name=finishComposingText numDexInstrs=44 nativeCodeOffset=1124d nativeCodeSize=224
      class Lcom.android.ex.editstyledtext.EditStyledText nmethods=35
      method 172 name=-get1 numDexInstrs=3 nativeCodeOffset=1166d nativeCodeSize=82
      method 173 name=-wrap13 numDexInstrs=4 nativeCodeOffset=116d5 nativeCodeSize=82
      method 174 name=-wrap6 numDexInstrs=4 nativeCodeOffset=1173d nativeCodeSize=60
      method 175 name=<clinit> numDexInstrs=8
      method 192 name=notifyStateChanged numDexInstrs=27 nativeCodeOffset=1178d nativeCodeSize=270
      method 200 name=onRefreshStyles numDexInstrs=6 nativeCodeOffset=118b5 nativeCodeSize=60
      method 212 name=sendOnTouchEvent numDexInstrs=27 nativeCodeOffset=11905 nativeCodeSize=226
      method 218 name=stopSelecting numDexInstrs=6 nativeCodeOffset=119fd nativeCodeSize=60
      method 177 name=drawableStateChanged numDexInstrs=13 nativeCodeOffset=11a4d nativeCodeSize=60
      method 178 name=getBackgroundColor numDexInstrs=7 nativeCodeOffset=11a9d nativeCodeSize=60
      method 180 name=getForegroundColor numDexInstrs=39 nativeCodeOffset=11aed nativeCodeSize=60
      method 181 name=getSelectState numDexInstrs=7 nativeCodeOffset=11b3d nativeCodeSize=378
      method 187 name=isButtonsFocused numDexInstrs=30 nativeCodeOffset=11ccd nativeCodeSize=142
      method 188 name=isEditting numDexInstrs=7 nativeCodeOffset=11d75 nativeCodeSize=62
      method 190 name=isSoftKeyBlocked numDexInstrs=7 nativeCodeOffset=11dcd nativeCodeSize=60
      method 191 name=isStyledText numDexInstrs=7 nativeCodeOffset=11e1d nativeCodeSize=134
      method 193 name=onClearStyles numDexInstrs=6 nativeCodeOffset=11ebd nativeCodeSize=62
      method 194 name=onCreateContextMenu numDexInstrs=75 nativeCodeOffset=11f15 nativeCodeSize=186
      method 195 name=onCreateInputConnection numDexInstrs=14 nativeCodeOffset=11fe5 nativeCodeSize=120
      method 196 name=onEndEdit numDexInstrs=8 nativeCodeOffset=12075 nativeCodeSize=62
      method 197 name=onFixSelectedItem numDexInstrs=6 nativeCodeOffset=120cd nativeCodeSize=62
      method 198 name=onFocusChanged numDexInstrs=19 nativeCodeOffset=12125 nativeCodeSize=62
      method 199 name=onInsertHorizontalLine numDexInstrs=8 nativeCodeOffset=1217d nativeCodeSize=62
      method 201 name=onRestoreInstanceState numDexInstrs=24 nativeCodeOffset=121d5 nativeCodeSize=62
      method 202 name=onSaveInstanceState numDexInstrs=18 nativeCodeOffset=1222d nativeCodeSize=62
      method 203 name=onStartCopy numDexInstrs=7 nativeCodeOffset=12285 nativeCodeSize=340
      method 204 name=onStartCut numDexInstrs=7 nativeCodeOffset=123ed nativeCodeSize=590
      method 205 name=onStartEdit numDexInstrs=8 nativeCodeOffset=12655 nativeCodeSize=460
      method 206 name=onStartPaste numDexInstrs=7 nativeCodeOffset=12835 nativeCodeSize=138
      method 207 name=onStartSelect numDexInstrs=7 nativeCodeOffset=8 nativeCodeSize=3749680
      method 208 name=onStartSelectAll numDexInstrs=7 nativeCodeOffset=128d5 nativeCodeSize=82
      method 209 name=onTextChanged numDexInstrs=71 nativeCodeOffset=1293d nativeCodeSize=1092
      method 210 name=onTextContextMenuItem numDexInstrs=132 nativeCodeOffset=12d95 nativeCodeSize=16
      method 211 name=onTouchEvent numDexInstrs=99 nativeCodeOffset=12dbd nativeCodeSize=564
      method 213 name=setBackgroundColor numDexInstrs=23 nativeCodeOffset=13005 nativeCodeSize=1160
      class Lcom.android.smspush.WapPushManager$IWapPushManagerStub nmethods=7
      method 220 name=<init> numDexInstrs=6 nativeCodeOffset=128d5 nativeCodeSize=82
      method 221 name=addPackage numDexInstrs=196 nativeCodeOffset=1293d nativeCodeSize=1092
      method 222 name=appTypeCheck numDexInstrs=8 nativeCodeOffset=12d95 nativeCodeSize=16
      method 223 name=deletePackage numDexInstrs=109 nativeCodeOffset=12dbd nativeCodeSize=564
      method 224 name=processMessage numDexInstrs=258 nativeCodeOffset=13005 nativeCodeSize=1160
      method 225 name=signatureCheck numDexInstrs=21 nativeCodeOffset=134a5 nativeCodeSize=114
      method 226 name=updatePackage numDexInstrs=213 nativeCodeOffset=1352d nativeCodeSize=1076
      class Lcom.android.smspush.WapPushManager$WapPushManDBHelper$queryData nmethods=1
      method 227 name=<init> numDexInstrs=6 nativeCodeOffset=13975 nativeCodeSize=18
      class Lcom.android.smspush.WapPushManager$WapPushManDBHelper nmethods=4
      method 228 name=<init> numDexInstrs=11 nativeCodeOffset=1399d nativeCodeSize=152
      method 231 name=onCreate numDexInstrs=7 nativeCodeOffset=13a4d nativeCodeSize=98
      method 232 name=onUpgrade numDexInstrs=10 nativeCodeOffset=13ac5 nativeCodeSize=116
      method 233 name=queryLastApp numDexInstrs=155 nativeCodeOffset=13b4d nativeCodeSize=896
      class Lcom.android.smspush.WapPushManager nmethods=5
      method 234 name=<init> numDexInstrs=18 nativeCodeOffset=13ee5 nativeCodeSize=148
      method 235 name=getDatabase numDexInstrs=14 nativeCodeOffset=13f8d nativeCodeSize=180
      method 236 name=isDataExist numDexInstrs=21 nativeCodeOffset=14055 nativeCodeSize=142
      method 237 name=onBind numDexInstrs=3 nativeCodeOffset=140fd nativeCodeSize=4
      method 238 name=verifyData numDexInstrs=60 nativeCodeOffset=14115 nativeCodeSize=300
                                        );
  std::string sqexp = squeezeWhite(oat_expected, "expected");
  std::string result = vis.result();
  std::string sqact = squeezeWhite(result, "actual");
  EXPECT_STREQ(sqexp.c_str(), sqact.c_str());
}

TEST_F(OatDexReadTest, BitStuff)
{
  { unsigned bv[] = { 0, 0 };
    EXPECT_EQ(numBitsSet(bv, sizeof(bv)/sizeof(unsigned), 63), 0);
    EXPECT_EQ(isBitSet(bv, sizeof(bv)/sizeof(unsigned), 0), 0);
  }
  { unsigned char bbv[] = { 0x77, 0x77, 0x77, 0x70, 0x0, 0x0, 0x0, 0x7 };
    unsigned *bv = reinterpret_cast<unsigned*>(&bbv[0]);
    ASSERT_EQ(numBitsSet(bv, sizeof(bbv)/sizeof(unsigned), 63), 24);
    ASSERT_EQ(numBitsSet(bv, sizeof(bbv)/sizeof(unsigned), 31), 21);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 0), true);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 1), true);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 2), true);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 3), false);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 4), true);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 31), false);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 30), true);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 32), false);
    ASSERT_EQ(isBitSet(bv, sizeof(bbv)/sizeof(unsigned), 63), false);
  }
  { unsigned bv[] = { 0x77777777, 0 };
    ASSERT_EQ(numBitsSet(bv, sizeof(bv)/sizeof(unsigned), 8), 6);
  }
}

int main(int argc, char **argv) {
  executable_path = argv[0];
  // switch to / before starting testing (all code to be tested
  // should be location-independent)
  chdir("/");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
