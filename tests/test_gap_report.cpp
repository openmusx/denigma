/*
 * Copyright (C) 2026, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <optional>
#include <string>
#include <utility>

#include "gtest/gtest.h"

#include "denigma/classify/gaps.h"
#include "denigma/gap_report.h"

TEST(GapReport, SerializesEmptyAndStructuredReports)
{
    denigma::classify::GapCollector collector;
    const denigma::GapReportProducer producer{"denigma", "TEST", "abc123"};
    const auto empty = denigma::serializeGapReport(collector, producer);
    EXPECT_NE(empty.find("\"gaps\": []"), std::string::npos);

    denigma::classify::ChordSymbolClassification chord;
    chord.suffix.strings.push_back({"6", denigma::classify::chord::SuffixString::Position::Above});
    chord.suffix.strings.push_back({"9", denigma::classify::chord::SuffixString::Position::Below});
    collector.add({"P1.m1", std::nullopt, denigma::classify::GapPosition{0, 1}}, std::move(chord));

    const auto report = denigma::serializeGapReport(collector, producer);
    EXPECT_NE(report.find("\"position\": \"above\""), std::string::npos);
    EXPECT_NE(report.find("\"position\": \"below\""), std::string::npos);
    EXPECT_EQ(report.find("\"quality\": null"), std::string::npos);
}
