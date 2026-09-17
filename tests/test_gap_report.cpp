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

#include "nlohmann/json.hpp"
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

TEST(GapReport, SerializesExtentPlacementsAndExpressionPayloads)
{
    denigma::classify::GapCollector collector;
    const denigma::GapReportProducer producer{"denigma", "TEST", "abc123"};

    denigma::classify::ExpressionClassification tempo;
    tempo.type = denigma::classify::ExpressionType::TempoMark;
    tempo.basis = denigma::classify::ClassificationBasis::FinaleCategory;
    tempo.scope = denigma::classify::ExpressionScope::TopStaff;
    tempo.value = denigma::classify::expression::TempoText{{"Allegro", 120, 1024}};
    collector.add({"m1.textExp24.inci0", std::nullopt, std::nullopt}, tempo, denigma::classify::GapExtent::Partial,
        {
            {denigma::classify::GapPlacement::Kind::SystemTop, {"m1", std::nullopt, std::nullopt}},
            {denigma::classify::GapPlacement::Kind::Staff, {"P2.m1", 2, std::nullopt}},
        });
    collector.add({"m1.tempoDef.inci0", std::nullopt, std::nullopt}, denigma::classify::PlaybackOnly{}, denigma::classify::GapExtent::Partial);
    denigma::classify::FormattedText text;
    text.runs.push_back({"x", {}, {"metNoteQuarterUp"}, std::nullopt});
    text.runs.push_back({"7", {}, {}, denigma::classify::text::Insert{denigma::classify::text::Insert::Kind::Other, "page", {"1"}}});
    text.plainText = "x7";
    collector.add({"m2", std::nullopt, denigma::classify::GapPosition{1, 4}}, text);

    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, producer));
    ASSERT_EQ(report["gaps"].size(), 3);
    const auto& tempoGap = report["gaps"][0];
    EXPECT_EQ(tempoGap["extent"], "partial");
    EXPECT_EQ(tempoGap["type"], "expression");
    EXPECT_EQ(tempoGap["expression"]["type"], "tempo-mark");
    EXPECT_EQ(tempoGap["expression"]["basis"], "finale-category");
    EXPECT_EQ(tempoGap["expression"]["scope"], "top-staff");
    EXPECT_EQ(tempoGap["expression"]["tempo"]["beatsPerMinute"], 120);
    EXPECT_FALSE(tempoGap["expression"].contains("text")); // no source text context
    ASSERT_EQ(tempoGap["placements"].size(), 2);
    EXPECT_EQ(tempoGap["placements"][0]["kind"], "system-top");
    EXPECT_EQ(tempoGap["placements"][0]["anchor"], "m1");
    EXPECT_EQ(tempoGap["placements"][1]["kind"], "staff");
    EXPECT_EQ(tempoGap["placements"][1]["anchor"], "P2.m1");
    EXPECT_EQ(tempoGap["placements"][1]["staff"], 2);

    EXPECT_EQ(report["gaps"][1]["type"], "playback-only");
    EXPECT_EQ(report["gaps"][1]["extent"], "partial");
    EXPECT_FALSE(report["gaps"][1].contains("placements"));

    const auto& textGap = report["gaps"][2];
    EXPECT_EQ(textGap["type"], "formatted-text");
    EXPECT_EQ(textGap["extent"], "complete");
    EXPECT_EQ(textGap["text"]["plain"], "x7");
    EXPECT_EQ(textGap["text"]["runs"][0]["glyphs"][0], "metNoteQuarterUp");
    EXPECT_FALSE(textGap["text"]["runs"][0].contains("insert"));
    EXPECT_EQ(textGap["text"]["runs"][1]["insert"]["kind"], "other");
    EXPECT_EQ(textGap["text"]["runs"][1]["insert"]["command"], "page");
    EXPECT_EQ(textGap["text"]["runs"][1]["insert"]["parameters"][0], "1");
}
