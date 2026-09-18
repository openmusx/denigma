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
#include <vector>

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
    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {{"denigma", "TEST", "abc123"}, {}}));
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
    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {{"denigma", "TEST", "abc123"}, {}}));
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
    checkFixture("pedal_custom_lines.musx");
    checkFixture("glissando.musx");
    checkFixture("smartshape_lines.musx");
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

    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {{"denigma", "TEST", "abc123"}, {}}));
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

    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {{"denigma", "TEST", "abc123"}, {}}));
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
    const auto report = nlohmann::json::parse(denigma::serializeGapReport(collector, {{"denigma", "TEST", "abc123"}, {}}));
    ASSERT_FALSE(report["gaps"].empty());
    EXPECT_EQ(report["gaps"][0]["anchor"], "m1.tempoDef.inci0");
    EXPECT_EQ(report["gaps"][0]["type"], "playback-only");
    EXPECT_EQ(report["gaps"][0]["extent"], "partial");
}

namespace {

/// @brief Converts a musx fixture to MNX with a gap collector and returns the serialized report.
nlohmann::json collectMnxGapReport(const std::string& fileName)
{
    denigma::FileRandomAccessReader input(getInputPath() / fileName);
    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = fileName;
    options.common.validate = false;
    denigma::classify::GapCollector collector;
    options.common.gapCollector = &collector;

    const auto result = denigma::formats::mnx::MusxToMnxJsonConverter{}.convert(input, output, options);
    EXPECT_TRUE(result);
    return nlohmann::json::parse(denigma::serializeGapReport(collector, {{"denigma", "TEST", "abc123"}, {}}));
}

std::vector<nlohmann::json> smartShapeGaps(const nlohmann::json& report)
{
    std::vector<nlohmann::json> result;
    for (const auto& gap : report["gaps"]) {
        if (gap["type"] == "smart-shape") {
            result.push_back(gap);
        }
    }
    return result;
}

} // namespace

TEST(MnxGapReport, MusxToMnxJsonReportsBeatAttachedSmartShapesAsSpans)
{
    setupTestDataPaths();

    const auto report = collectMnxGapReport("pedal_custom_lines.musx");
    const auto gaps = smartShapeGaps(report);
    ASSERT_EQ(gaps.size(), 8);

    // Measure 2 holds a pedal line drawn with the SMuFL pedal glyphs as its start and end text.
    const auto pedal = std::find_if(gaps.begin(), gaps.end(), [](const auto& gap) { return gap["smartShape"]["kind"] == "keyboard-pedal"; });
    ASSERT_NE(pedal, gaps.end());
    EXPECT_EQ((*pedal)["anchor"], "P1.m2");
    EXPECT_EQ((*pedal)["extent"], "complete");
    EXPECT_FALSE((*pedal)["anchor"].contains("staff")) << "a single-staff part names no staff";
    EXPECT_EQ((*pedal)["position"]["numerator"], 0);
    EXPECT_EQ((*pedal)["end"]["anchor"], "P1.m2");
    EXPECT_EQ((*pedal)["end"]["position"]["numerator"], 1);
    EXPECT_EQ((*pedal)["end"]["position"]["denominator"], 1);
    EXPECT_EQ((*pedal)["smartShape"]["shapeType"], "custom-line");
    const auto& keyboardPedal = (*pedal)["smartShape"]["keyboardPedal"];
    EXPECT_EQ(keyboardPedal["startText"]["type"], "pedal-one");
    EXPECT_EQ(keyboardPedal["endText"]["type"], "pedal-up");
    EXPECT_EQ(keyboardPedal["line"]["lineStyle"], "char");
    EXPECT_FALSE(keyboardPedal["line"]["lineVisible"]);
    EXPECT_TRUE(keyboardPedal["line"]["horizontal"]);
    EXPECT_EQ(keyboardPedal["line"]["startText"]["runs"][0]["glyphs"][0], "keyboardPedalPed");

    // Measure 1 holds a custom line whose center text says "Glissando" but which is beat-attached, so it is a general line.
    const auto& general = gaps.front();
    EXPECT_EQ(general["anchor"], "P1.m1");
    EXPECT_EQ(general["smartShape"]["kind"], "general-line");
    EXPECT_EQ(general["smartShape"]["generalLine"]["lineChar"]["glyph"], "wiggleGlissando");
    EXPECT_EQ(general["smartShape"]["generalLine"]["centerFullText"]["plain"], "Glissando");
    EXPECT_EQ(general["smartShape"]["generalLine"]["centerAbbrText"]["plain"], "Gliss.");
    EXPECT_FALSE(report.contains("arrowheads"));
}

TEST(MnxGapReport, MusxToMnxJsonReportsEntryAttachedSmartShapesBetweenNotes)
{
    setupTestDataPaths();

    const auto report = collectMnxGapReport("glissando.musx");
    const auto gaps = smartShapeGaps(report);
    ASSERT_EQ(gaps.size(), 17);
    for (const auto& gap : gaps) {
        EXPECT_EQ(gap["smartShape"]["kind"], "glissando") << gap.dump(4);
        EXPECT_FALSE(gap.contains("position")) << gap.dump(4);
        EXPECT_TRUE(gap["anchor"].get<std::string>().starts_with("ev")) << gap.dump(4);
        EXPECT_TRUE(gap["end"]["anchor"].get<std::string>().starts_with("ev")) << gap.dump(4);
    }
    EXPECT_EQ(gaps[0]["anchor"], "ev45n1");
    EXPECT_EQ(gaps[0]["end"]["anchor"], "ev46n1");
    EXPECT_EQ(gaps[0]["smartShape"]["shapeType"], "glissando");
    EXPECT_EQ(gaps[0]["smartShape"]["glissando"]["line"]["lineChar"]["glyph"], "wiggleGlissando");
    const auto tabSlide = std::find_if(gaps.begin(), gaps.end(), [](const auto& gap) { return gap["smartShape"]["shapeType"] == "tab-slide"; });
    ASSERT_NE(tabSlide, gaps.end());
    EXPECT_EQ((*tabSlide)["smartShape"]["kind"], "glissando");

    // A preset arrowhead is embedded once, drawn in staff spaces with its tip at the origin.
    ASSERT_TRUE(report.contains("arrowheads"));
    ASSERT_EQ(report["arrowheads"].size(), 1);
    const auto& preset = report["arrowheads"]["preset-small-filled"];
    EXPECT_EQ(preset["unit"], "staff-space");
    EXPECT_EQ(preset["origin"], "line-end");
    EXPECT_NE(preset["svg"].get<std::string>().find("<svg "), std::string::npos);
    EXPECT_NE(preset["svg"].get<std::string>().find("M 0 0 L -20 10 L -20 -10 Z"), std::string::npos);
    EXPECT_NE(preset["svg"].get<std::string>().find("matrix(0.0416667"), std::string::npos) << "20 EVPU scale to 5/6 of a space";
    const auto withArrow = std::find_if(
        gaps.begin(), gaps.end(), [](const auto& gap) { return gap["smartShape"]["glissando"]["line"]["endCap"]["type"] == "arrowhead-preset"; });
    ASSERT_NE(withArrow, gaps.end());
    EXPECT_EQ((*withArrow)["smartShape"]["glissando"]["line"]["endCap"]["preset"], "small-filled");
    EXPECT_EQ((*withArrow)["smartShape"]["glissando"]["line"]["endCap"]["arrowhead"], "preset-small-filled");
}

TEST(MnxGapReport, MusxToMnxJsonReportsGlissandoToRestOnItsNote)
{
    setupTestDataPaths();

    // Finale stores a glissando drawn toward a rest with both endpoints on the note it leaves.
    const auto report = collectMnxGapReport("gliss_to_rest.musx");
    const auto gaps = smartShapeGaps(report);
    ASSERT_EQ(gaps.size(), 2);
    EXPECT_EQ(gaps[0]["anchor"], "ev5n1");
    EXPECT_EQ(gaps[0]["end"]["anchor"], "ev5n1");
    EXPECT_EQ(gaps[1]["anchor"], "ev7n1");
    EXPECT_EQ(gaps[1]["smartShape"]["shapeType"], "tab-slide");
}

TEST(MnxGapReport, MusxToMnxJsonReportsBeatAttachedSlursButNotEntryAttachedOnes)
{
    setupTestDataPaths();

    const auto beatAttached = smartShapeGaps(collectMnxGapReport("slurs_beatattached.musx"));
    ASSERT_EQ(beatAttached.size(), 14);
    EXPECT_EQ(beatAttached[0]["anchor"], "P1.m1");
    EXPECT_EQ(beatAttached[0]["position"]["numerator"], 1);
    EXPECT_EQ(beatAttached[0]["position"]["denominator"], 1);
    EXPECT_EQ(beatAttached[0]["end"]["anchor"], "P1.m2");
    EXPECT_EQ(beatAttached[0]["end"]["position"]["numerator"], 0);
    EXPECT_EQ(beatAttached[0]["smartShape"]["shapeType"], "slur-down");
    EXPECT_EQ(beatAttached[0]["smartShape"]["kind"], "slur");
    EXPECT_EQ(beatAttached[0]["smartShape"]["slur"]["contour"], "down");

    EXPECT_TRUE(smartShapeGaps(collectMnxGapReport("slurs_2voices.musx")).empty());
}

TEST(MnxGapReport, MusxToMnxJsonReportsTrillAndVibratoLines)
{
    setupTestDataPaths();

    const auto gaps = smartShapeGaps(collectMnxGapReport("wavy_lines.musx"));
    ASSERT_EQ(gaps.size(), 5);
    EXPECT_EQ(gaps[0]["smartShape"]["shapeType"], "trill-extension");
    EXPECT_EQ(gaps[0]["smartShape"]["kind"], "trill-line");
    EXPECT_FALSE(gaps[0]["smartShape"]["trillLine"]["includesTrSymbol"]);
    EXPECT_EQ(gaps[0]["anchor"], "P1.m1");
    EXPECT_EQ(gaps[0]["end"]["anchor"], "P1.m2");
    EXPECT_EQ(gaps[1]["smartShape"]["kind"], "vibrato-line");
    EXPECT_EQ(gaps[1]["smartShape"]["vibratoLine"]["line"]["lineChar"]["glyph"], "wiggleVibratoLargestSlowest");
    EXPECT_EQ(gaps[2]["smartShape"]["shapeType"], "trill");
    EXPECT_TRUE(gaps[2]["smartShape"]["trillLine"]["includesTrSymbol"]);
}

TEST(MnxGapReport, CliEmbedsCustomArrowheadsOnce)
{
    setupTestDataPaths();

    // smartshape_lines.musx with two of its pedal line styles given Finale's default pedal-change arrowheads
    // (Shape Designer shapes 93 and 94) as custom caps; shape 93 caps three line ends.
    const auto inputPath = getInputPath() / "custom_arrowheads.enigmaxml.zip";
    const auto outputPath = getOutputPath() / "custom_arrowheads.mnx";
    ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx", pathString(outputPath), "--gap-report", "--force"};
    ASSERT_EQ(denigmaTestMain(args.argc(), args.argv()), 0);
    nlohmann::json report;
    openJson(getOutputPath() / "custom_arrowheads.mnx.gaps.json", report);

    const auto gaps = smartShapeGaps(report);
    std::vector<std::string> references;
    for (const auto& gap : gaps) {
        if (gap["smartShape"]["kind"] != "keyboard-pedal") {
            continue;
        }
        for (const auto* end : {"startCap", "endCap"}) {
            const auto& cap = gap["smartShape"]["keyboardPedal"]["line"][end];
            if (cap["type"] == "arrowhead-custom") {
                references.push_back(cap["arrowhead"]);
                EXPECT_TRUE(cap["knownType"].get<std::string>().starts_with("pedal-arrowhead")) << cap.dump(4);
            }
        }
    }
    EXPECT_EQ(references, (std::vector<std::string>{"custom-93", "custom-93", "custom-93", "custom-94"}));

    ASSERT_TRUE(report.contains("arrowheads"));
    ASSERT_EQ(report["arrowheads"].size(), 2);
    for (const auto* key : {"custom-93", "custom-94"}) {
        const auto& arrowhead = report["arrowheads"][key];
        EXPECT_EQ(arrowhead["unit"], "staff-space");
        EXPECT_EQ(arrowhead["origin"], "line-end");
        const auto svg = arrowhead["svg"].get<std::string>();
        EXPECT_NE(svg.find("<svg "), std::string::npos) << key;
        EXPECT_NE(svg.find("viewBox=\"-0.545898"), std::string::npos) << key << ": coordinates in staff spaces";
    }
}
