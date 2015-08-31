/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "gtest/gtest.h"
#include "histogram-inl.h"
#include "UniquePtr.h"

#include <sstream>

namespace art {

// Simple usage:
//   Histogram *hist(new Histogram("SimplePercentiles"));
//   Percentile PerValue
//   hist->AddValue(121);
//   hist->AddValue(132);
//   hist->AddValue(140);
//   hist->AddValue(145);
//   hist->AddValue(155);
//   hist->CreateHistogram();
//   PerValue = hist->PercentileVal(0.50); finds the 50th percentile(median).

TEST(Histtest, MeanTest) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("MeanTest", 5));

  double mean;
  for (size_t Idx = 0; Idx < 90; Idx++) {
    hist->AddValue(static_cast<uint64_t>(50));
  }
  mean = hist->Mean();
  EXPECT_EQ(mean, 50);
  hist->Reset();
  hist->AddValue(9);
  hist->AddValue(17);
  hist->AddValue(28);
  hist->AddValue(28);
  mean = hist->Mean();
  EXPECT_EQ(20.5, mean);
}

TEST(Histtest, VarianceTest) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("VarianceTest", 5));

  double variance;
  hist->AddValue(9);
  hist->AddValue(17);
  hist->AddValue(28);
  hist->AddValue(28);
  variance = hist->Variance();
  EXPECT_EQ(64.25, variance);
}

TEST(Histtest, Percentile) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("Percentile", 5));
  Histogram<uint64_t>::CumulativeData data;

  double PerValue;

  hist->AddValue(20);
  hist->AddValue(31);
  hist->AddValue(42);
  hist->AddValue(50);
  hist->AddValue(60);
  hist->AddValue(70);

  hist->AddValue(98);

  hist->AddValue(110);
  hist->AddValue(121);
  hist->AddValue(132);
  hist->AddValue(140);
  hist->AddValue(145);
  hist->AddValue(155);

  hist->CreateHistogram(data);
  PerValue = hist->Percentile(0.50, data);
  EXPECT_EQ(875, static_cast<int>(PerValue * 10));
}

TEST(Histtest, UpdateRange) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("UpdateRange", 5));
  Histogram<uint64_t>::CumulativeData data;

  double PerValue;

  hist->AddValue(15);
  hist->AddValue(17);
  hist->AddValue(35);
  hist->AddValue(50);
  hist->AddValue(68);
  hist->AddValue(75);
  hist->AddValue(93);
  hist->AddValue(110);
  hist->AddValue(121);
  hist->AddValue(132);
  hist->AddValue(140);  // Median  value
  hist->AddValue(145);
  hist->AddValue(155);
  hist->AddValue(163);
  hist->AddValue(168);
  hist->AddValue(175);
  hist->AddValue(182);
  hist->AddValue(193);
  hist->AddValue(200);
  hist->AddValue(205);
  hist->AddValue(212);
  hist->CreateHistogram(data);
  PerValue = hist->Percentile(0.50, data);

  std::string text;
  std::stringstream stream;
  std::string expected("UpdateRange:\t99% C.I. 15us-212us Avg: 126.380us Max: 212us\n");
  hist->PrintConfidenceIntervals(stream, 0.99, data);

  EXPECT_EQ(expected, stream.str());
  EXPECT_GE(PerValue, 132);
  EXPECT_LE(PerValue, 145);
}

TEST(Histtest, Reset) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("Reset", 5));
  Histogram<uint64_t>::CumulativeData data;

  double PerValue;
  hist->AddValue(0);
  hist->AddValue(189);
  hist->AddValue(389);
  hist->Reset();
  hist->AddValue(15);
  hist->AddValue(17);
  hist->AddValue(35);
  hist->AddValue(50);
  hist->AddValue(68);
  hist->AddValue(75);
  hist->AddValue(93);
  hist->AddValue(110);
  hist->AddValue(121);
  hist->AddValue(132);
  hist->AddValue(140);  // Median  value
  hist->AddValue(145);
  hist->AddValue(155);
  hist->AddValue(163);
  hist->AddValue(168);
  hist->AddValue(175);
  hist->AddValue(182);
  hist->AddValue(193);
  hist->AddValue(200);
  hist->AddValue(205);
  hist->AddValue(212);
  hist->CreateHistogram(data);
  PerValue = hist->Percentile(0.50, data);

  std::string text;
  std::stringstream stream;
  std::string expected("Reset:\t99% C.I. 15us-212us Avg: 126.380us Max: 212us\n");
  hist->PrintConfidenceIntervals(stream, 0.99, data);

  EXPECT_EQ(expected, stream.str());
  EXPECT_GE(PerValue, 132);
  EXPECT_LE(PerValue, 145);
}

TEST(Histtest, MultipleCreateHist) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("MultipleCreateHist", 5));
  Histogram<uint64_t>::CumulativeData data;

  double PerValue;
  hist->AddValue(15);
  hist->AddValue(17);
  hist->AddValue(35);
  hist->AddValue(50);
  hist->AddValue(68);
  hist->AddValue(75);
  hist->AddValue(93);
  hist->CreateHistogram(data);
  hist->AddValue(110);
  hist->AddValue(121);
  hist->AddValue(132);
  hist->AddValue(140);  // Median  value
  hist->AddValue(145);
  hist->AddValue(155);
  hist->AddValue(163);
  hist->AddValue(168);
  hist->CreateHistogram(data);
  hist->AddValue(175);
  hist->AddValue(182);
  hist->AddValue(193);
  hist->AddValue(200);
  hist->AddValue(205);
  hist->AddValue(212);
  hist->CreateHistogram(data);
  PerValue = hist->Percentile(0.50, data);
  std::stringstream stream;
  std::string expected("MultipleCreateHist:\t99% C.I. 15us-212us Avg: 126.380us Max: 212us\n");
  hist->PrintConfidenceIntervals(stream, 0.99, data);

  EXPECT_EQ(expected, stream.str());
  EXPECT_GE(PerValue, 132);
  EXPECT_LE(PerValue, 145);
}

TEST(Histtest, SingleValue) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("SingleValue", 5));
  Histogram<uint64_t>::CumulativeData data;

  hist->AddValue(1);
  hist->CreateHistogram(data);
  std::stringstream stream;
  std::string expected = "SingleValue:\t99% C.I. 1us-1us Avg: 1us Max: 1us\n";
  hist->PrintConfidenceIntervals(stream, 0.99, data);
  EXPECT_EQ(expected, stream.str());
}

TEST(Histtest, CappingPercentiles) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("CappingPercentiles", 5));
  Histogram<uint64_t>::CumulativeData data;

  double per_995;
  double per_005;
  // All values are similar.
  for (uint64_t idx = 0ull; idx < 150ull; idx++) {
    hist->AddValue(0);
  }
  hist->CreateHistogram(data);
  per_995 = hist->Percentile(0.995, data);
  EXPECT_EQ(per_995, 0);
  hist->Reset();
  for (size_t idx = 0; idx < 200; idx++) {
    for (uint64_t val = 1ull; val <= 4ull; val++) {
      hist->AddValue(val);
    }
  }
  hist->CreateHistogram(data);
  per_005 = hist->Percentile(0.005, data);
  per_995 = hist->Percentile(0.995, data);
  EXPECT_EQ(1, per_005);
  EXPECT_EQ(4, per_995);
}

TEST(Histtest, SpikyValues) {
  UniquePtr<Histogram<uint64_t> > hist(new Histogram<uint64_t>("SpikyValues", 5, 4096));
  Histogram<uint64_t>::CumulativeData data;

  for (uint64_t idx = 0ull; idx < 30ull; idx++) {
    for (uint64_t idx_inner = 0ull; idx_inner < 5ull; idx_inner++) {
      hist->AddValue(idx * idx_inner);
    }
  }
  hist->AddValue(10000);
  hist->CreateHistogram(data);
  std::stringstream stream;
  std::string expected = "SpikyValues:\t99% C.I. 0.089us-2541.825us Avg: 95.033us Max: 10000us\n";
  hist->PrintConfidenceIntervals(stream, 0.99, data);
  EXPECT_EQ(expected, stream.str());
}

}  // namespace art
