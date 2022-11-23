#include "RegEx.h"

#include <gtest/gtest.h>

using namespace simpleperf;

TEST(RegEx, smoke) {
  auto re = RegEx::Create("b+");
  ASSERT_EQ(re->GetPattern(), "b+");
  ASSERT_FALSE(re->Search("aaa"));
  ASSERT_TRUE(re->Search("aba"));
  ASSERT_FALSE(re->Match("aba"));
  ASSERT_TRUE(re->Match("bbb"));
  auto match = re->SearchAll("aaa");
  ASSERT_FALSE(match->IsValid());

  match = re->SearchAll("ababb");
  ASSERT_TRUE(match->IsValid());
  ASSERT_EQ(match->GetField(0), "b");
  match->MoveToNextMatch();
  ASSERT_TRUE(match->IsValid());
  ASSERT_EQ(match->GetField(0), "bb");
  match->MoveToNextMatch();
  ASSERT_FALSE(match->IsValid());
}

TEST(RegEx, invalid_pattern) {
  ASSERT_TRUE(RegEx::Create("?hello") == nullptr);
}
