/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "timing_logger.h"

#include "common_test.h"

namespace art {

class TimingLoggerTest : public CommonTest {};

// TODO: Negative test cases (improper pairing of EndSplit, etc.)

TEST_F(TimingLoggerTest, StartEnd) {
  const char* split1name = "First Split";
  base::TimingLogger timings("StartEnd", true, false);

  timings.StartSplit(split1name);

  timings.EndSplit();  // Ends split1.

  const base::TimingLogger::SplitTimings& splits = timings.GetSplits();

  EXPECT_EQ(1U, splits.size());
  EXPECT_STREQ(splits[0].second, split1name);
}


TEST_F(TimingLoggerTest, StartNewEnd) {
  const char* split1name = "First Split";
  const char* split2name = "Second Split";
  const char* split3name = "Third Split";
  base::TimingLogger timings("StartNewEnd", true, false);

  timings.StartSplit(split1name);

  timings.NewSplit(split2name);  // Ends split1.

  timings.NewSplit(split3name);  // Ends split2.

  timings.EndSplit();  // Ends split3.

  const base::TimingLogger::SplitTimings& splits = timings.GetSplits();

  EXPECT_EQ(3U, splits.size());
  EXPECT_STREQ(splits[0].second, split1name);
  EXPECT_STREQ(splits[1].second, split2name);
  EXPECT_STREQ(splits[2].second, split3name);
}

TEST_F(TimingLoggerTest, StartNewEndNested) {
  const char* split1name = "First Split";
  const char* split2name = "Second Split";
  const char* split3name = "Third Split";
  const char* split4name = "Fourth Split";
  const char* split5name = "Fifth Split";
  base::TimingLogger timings("StartNewEndNested", true, false);

  timings.StartSplit(split1name);

  timings.NewSplit(split2name);  // Ends split1.

  timings.StartSplit(split3name);

  timings.StartSplit(split4name);

  timings.NewSplit(split5name);  // Ends split4.

  timings.EndSplit();  // Ends split5.

  timings.EndSplit();  // Ends split3.

  timings.EndSplit();  // Ends split2.

  const base::TimingLogger::SplitTimings& splits = timings.GetSplits();

  EXPECT_EQ(5U, splits.size());
  EXPECT_STREQ(splits[0].second, split1name);
  EXPECT_STREQ(splits[1].second, split4name);
  EXPECT_STREQ(splits[2].second, split5name);
  EXPECT_STREQ(splits[3].second, split3name);
  EXPECT_STREQ(splits[4].second, split2name);
}


TEST_F(TimingLoggerTest, Scoped) {
  const char* outersplit = "Outer Split";
  const char* innersplit1 = "Inner Split 1";
  const char* innerinnersplit1 = "Inner Inner Split 1";
  const char* innersplit2 = "Inner Split 2";
  base::TimingLogger timings("Scoped", true, false);

  {
      base::TimingLogger::ScopedSplit outer(outersplit, &timings);

      {
          base::TimingLogger::ScopedSplit inner1(innersplit1, &timings);

          {
              base::TimingLogger::ScopedSplit innerinner1(innerinnersplit1, &timings);
          }  // Ends innerinnersplit1.
      }  // Ends innersplit1.

      {
          base::TimingLogger::ScopedSplit inner2(innersplit2, &timings);
      }  // Ends innersplit2.
  }  // Ends outersplit.

  const base::TimingLogger::SplitTimings& splits = timings.GetSplits();

  EXPECT_EQ(4U, splits.size());
  EXPECT_STREQ(splits[0].second, innerinnersplit1);
  EXPECT_STREQ(splits[1].second, innersplit1);
  EXPECT_STREQ(splits[2].second, innersplit2);
  EXPECT_STREQ(splits[3].second, outersplit);
}


TEST_F(TimingLoggerTest, ScopedAndExplicit) {
  const char* outersplit = "Outer Split";
  const char* innersplit = "Inner Split";
  const char* innerinnersplit1 = "Inner Inner Split 1";
  const char* innerinnersplit2 = "Inner Inner Split 2";
  base::TimingLogger timings("Scoped", true, false);

  timings.StartSplit(outersplit);

  {
      base::TimingLogger::ScopedSplit inner(innersplit, &timings);

      timings.StartSplit(innerinnersplit1);

      timings.NewSplit(innerinnersplit2);  // Ends innerinnersplit1.
  }  // Ends innerinnersplit2, then innersplit.

  timings.EndSplit();  // Ends outersplit.

  const base::TimingLogger::SplitTimings& splits = timings.GetSplits();

  EXPECT_EQ(4U, splits.size());
  EXPECT_STREQ(splits[0].second, innerinnersplit1);
  EXPECT_STREQ(splits[1].second, innerinnersplit2);
  EXPECT_STREQ(splits[2].second, innersplit);
  EXPECT_STREQ(splits[3].second, outersplit);
}

}  // namespace art
