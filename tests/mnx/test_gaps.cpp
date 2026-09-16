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
}
