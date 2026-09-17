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
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>

#include "gtest/gtest.h"

#include "core/denigma.h"
#include "denigma/classify/gaps.h"
#include "denigma/formats/mnx.h"
#include "denigma/gap_report.h"
#include "denigma/io/random_access_reader.h"
#include "test_utils.h"

TEST(MnxGapReport, MusxToMnxJsonReportsChordSymbolGaps)
{
    setupTestDataPaths();

    denigma::FileRandomAccessReader input(getInputPath() / "chords.musx");
    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "chords.musx";
    options.common.validate = false;
    denigma::classify::GapCollector collector;
    options.common.gapCollector = &collector;

    const auto result = denigma::formats::mnx::MusxToMnxJsonConverter{}.convert(input, output, options);

    EXPECT_TRUE(result);
    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {"denigma", "TEST", "abc123"}));
    EXPECT_EQ(report["schemaVersion"], 1);
    ASSERT_FALSE(report["gaps"].empty());
    const auto& gap = report["gaps"].front();
    EXPECT_EQ(gap["type"], "chord-symbol");
    EXPECT_EQ(gap["anchor"], "P1.m1");
    EXPECT_EQ(gap["position"]["numerator"], 0);
    EXPECT_EQ(gap["position"]["denominator"], 1);
    EXPECT_EQ(gap["chord"]["root"]["step"], "C");
    EXPECT_EQ(gap["chord"]["suffix"]["quality"], "major");
    EXPECT_FALSE(gap.contains("source"));
    EXPECT_FALSE(gap.contains("cause"));
}

TEST(MnxGapReport, MusxToMnxJsonReportsNoteheadGaps)
{
    setupTestDataPaths();

    denigma::FileRandomAccessReader input(getInputPath() / "note_shapes.musx");
    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "note_shapes.musx";
    options.common.validate = false;
    denigma::classify::GapCollector collector;
    options.common.gapCollector = &collector;

    const auto result = denigma::formats::mnx::MusxToMnxJsonConverter{}.convert(input, output, options);

    EXPECT_TRUE(result);
    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {"denigma", "TEST", "abc123"}));
    const auto notehead =
        std::find_if(report["gaps"].begin(), report["gaps"].end(), [](const auto& gap) { return gap.value("type", "") == "notehead"; });
    ASSERT_NE(notehead, report["gaps"].end());
    EXPECT_TRUE(notehead->at("anchor").get<std::string>().starts_with("ev"));
    EXPECT_FALSE(notehead->at("notehead").at("shape").get<std::string>().empty());
    EXPECT_FALSE(notehead->contains("source"));
}

TEST(MnxGapReport, CliWritesReferenceGapReports)
{
    setupTestDataPaths();

    const auto checkFixture = [](const std::string& fileName) {
        const auto inputPath = getInputPath() / fileName;
        auto outputPath = getOutputPath() / std::filesystem::path(fileName).replace_extension(".mnx");
        ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx", pathString(outputPath), "--gap-report", "--force"};
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0);

        outputPath += ".gaps.json";
        nlohmann::json actual;
        openJson(outputPath, actual);
        actual["producer"]["commit"] = "<commit>";

        auto referencePath = getInputPath() / "reference" / std::filesystem::path(fileName).replace_extension(".mnx.gaps.json");
        nlohmann::json expected;
        openJson(referencePath, expected);
        EXPECT_EQ(actual, expected);
    };

    checkFixture("chords.musx");
    checkFixture("note_shapes.musx");
    checkFixture("tempo_varied_staves.musx");
    checkFixture("techniques.musx");
    checkFixture("rehearsal_marks.musx");
}

TEST(MnxGapReport, MusxToMnxJsonReportsStaffListTempoMarksOncePerGroup)
{
    setupTestDataPaths();

    denigma::FileRandomAccessReader input(getInputPath() / "tempo_varied_staves.musx");
    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "tempo_varied_staves.musx";
    options.common.validate = false;
    denigma::classify::GapCollector collector;
    options.common.gapCollector = &collector;

    const auto result = denigma::formats::mnx::MusxToMnxJsonConverter{}.convert(input, output, options);
    EXPECT_TRUE(result);

    const auto mnx = nlohmann::json::parse(output.str());
    const auto& tempos = mnx["global"]["measures"][0]["tempos"];
    ASSERT_EQ(tempos.size(), 3);
    EXPECT_EQ(tempos[0]["id"], "m1.textExp24.inci0");
    EXPECT_EQ(tempos[1]["id"], "m1.textExp54.inci3");
    EXPECT_EQ(tempos[2]["id"], "m1.shapeExp3.inci5");

    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {"denigma", "TEST", "abc123"}));
    const auto& gaps = report["gaps"];
    // Measure 1 draws the tempo mark on the top staff and two other staves through a staff list: one gap, three placements.
    const auto tempoGap = std::find_if(gaps.begin(), gaps.end(), [](const auto& gap) { return gap["anchor"] == "m1.textExp24.inci0"; });
    ASSERT_NE(tempoGap, gaps.end());
    EXPECT_EQ((*tempoGap)["extent"], "partial");
    EXPECT_EQ((*tempoGap)["type"], "expression");
    EXPECT_EQ((*tempoGap)["expression"]["type"], "tempo-mark");
    EXPECT_EQ((*tempoGap)["expression"]["scope"], "top-staff");
    EXPECT_EQ((*tempoGap)["expression"]["text"]["plain"], "Tempo (\u221E=120)");
    const auto& runs = (*tempoGap)["expression"]["text"]["runs"];
    ASSERT_EQ(runs.size(), 5);
    EXPECT_EQ(runs[1]["glyphs"][0], "metNoteQuarterUp");
    EXPECT_EQ(runs[3]["text"], "120");
    EXPECT_EQ(runs[3]["insert"]["kind"], "playback-value");
    EXPECT_EQ(runs[3]["insert"]["command"], "value");
    ASSERT_EQ((*tempoGap)["placements"].size(), 3);
    EXPECT_EQ((*tempoGap)["placements"][0]["kind"], "system-top");
    EXPECT_EQ((*tempoGap)["placements"][0]["anchor"], "m1");
    EXPECT_EQ((*tempoGap)["placements"][1]["kind"], "staff");
    EXPECT_TRUE((*tempoGap)["placements"][1]["anchor"].get<std::string>().ends_with(".m1"));
    EXPECT_EQ(
        std::count_if(gaps.begin(), gaps.end(), [](const nlohmann::json& gap) { return gap["anchor"].get<std::string>().starts_with("m1."); }), 2);

    // The "rit." in measure 4 has no MNX object at all.
    const auto ritGap = std::find_if(gaps.begin(), gaps.end(), [](const auto& gap) { return gap["expression"]["type"] == "tempo-alteration"; });
    ASSERT_NE(ritGap, gaps.end());
    EXPECT_EQ((*ritGap)["anchor"], "m4");
    EXPECT_EQ((*ritGap)["extent"], "complete");
    EXPECT_EQ((*ritGap)["position"]["numerator"], 0);
}

TEST(MnxGapReport, MusxToMnxJsonReportsStaffExpressionsOnThePartMeasure)
{
    setupTestDataPaths();

    denigma::FileRandomAccessReader input(getInputPath() / "techniques.musx");
    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "techniques.musx";
    options.common.validate = false;
    denigma::classify::GapCollector collector;
    options.common.gapCollector = &collector;

    const auto result = denigma::formats::mnx::MusxToMnxJsonConverter{}.convert(input, output, options);
    EXPECT_TRUE(result);

    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {"denigma", "TEST", "abc123"}));
    ASSERT_FALSE(report["gaps"].empty());
    const auto& gap = report["gaps"].front();
    EXPECT_EQ(gap["anchor"], "P1.m1");
    EXPECT_EQ(gap["extent"], "complete");
    EXPECT_FALSE(gap.contains("placements"));
    EXPECT_EQ(gap["expression"]["type"], "technique-text");
    EXPECT_EQ(gap["expression"]["scope"], "staff");
    EXPECT_EQ(gap["expression"]["technique"]["type"], "pizzicato");
    EXPECT_EQ(gap["expression"]["text"]["plain"], "pizz.");
}

TEST(MnxGapReport, MusxToMnxJsonReportsTempoToolChangesAsPlaybackOnly)
{
    setupTestDataPaths();

    denigma::FileRandomAccessReader input(getInputPath() / "tempo_changes.musx");
    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "tempo_changes.musx";
    options.common.validate = false;
    options.includeTempoTool = true;
    denigma::classify::GapCollector collector;
    options.common.gapCollector = &collector;

    const auto result = denigma::formats::mnx::MusxToMnxJsonConverter{}.convert(input, output, options);
    EXPECT_TRUE(result);

    const auto mnx = nlohmann::json::parse(output.str());
    EXPECT_EQ(mnx["global"]["measures"][0]["tempos"][0]["id"], "m1.tempoDef.inci0");
    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {"denigma", "TEST", "abc123"}));
    ASSERT_FALSE(report["gaps"].empty());
    EXPECT_EQ(report["gaps"][0]["anchor"], "m1.tempoDef.inci0");
    EXPECT_EQ(report["gaps"][0]["type"], "playback-only");
    EXPECT_EQ(report["gaps"][0]["extent"], "partial");
}
