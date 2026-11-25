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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/denigma.h"
#include "core/musx_reader.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "formats/musicxml/musicxml.h"
#include "musicxml_test.h"
#include "mx/api/ScoreData.h"
#include "test_utils.h"
#include "gtest/gtest.h"

using namespace denigma;
using namespace denigma::test::musicxml;

namespace {

// What a measure of a one-staff part says about its staff lines, clefs and note positions.
struct MeasureStaffSummary
{
    int staffLines{};
    std::vector<std::pair<int, bool>> hiddenLines;
    std::vector<std::pair<mx::api::ClefSymbol, std::optional<int>>> clefs; // symbol, and the line when one is written
    std::vector<std::string> notes;                          // "U" or "P", step, octave, alter

    bool operator==(const MeasureStaffSummary&) const = default;
};

std::vector<MeasureStaffSummary> summarizeStaffLines(const mx::api::ScoreData& score)
{
    std::vector<MeasureStaffSummary> result;
    const auto& part = score.parts.front();
    for (const auto& measure : part.measures) {
        auto& summary = result.emplace_back();
        const auto& staff = measure.staves.front();
        summary.staffLines = staff.staffLines;
        for (const auto& lineDetail : staff.lineDetails) {
            summary.hiddenLines.emplace_back(lineDetail.line, lineDetail.printObject == mx::api::Bool::no);
        }
        for (const auto& clef : staff.clefs) {
            summary.clefs.emplace_back(clef.symbol, clef.isLineSpecified ? std::optional<int>(clef.line) : std::nullopt);
        }
        for (const auto& [voiceNumber, voice] : staff.voices) {
            for (const auto& note : voice.notes) {
                if (note.isRest) {
                    continue;
                }
                summary.notes.push_back(std::string(note.isUnpitched ? "U" : "P") + std::to_string(int(note.pitchData.step)) + "/"
                                        + std::to_string(note.pitchData.octave) + "/" + std::to_string(note.pitchData.alter));
            }
        }
    }
    return result;
}

} // namespace

// custom_staves_2.musx changes the staff's lines every measure under a percussion clef. Finale counts each
// staff's lines from the bottom line it draws, and the notes, unpitched under the clef, move with it.
TEST(MusicXmlStaffLines, CustomStavesMatchFinale)
{
    setupTestDataPaths();

    const auto score = createScoreDataFromMusicXmlFixture("custom_staves_2.musx");
    const auto reference = loadScoreData(getInputPath() / "musicxml" / "custom_staves_2-ref.musicxml");
    ASSERT_TRUE(score);
    ASSERT_TRUE(reference);
    const auto actual = summarizeStaffLines(*score);
    const auto expected = summarizeStaffLines(*reference);
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index], expected[index]) << "measure " << index + 1;
    }
}

// A custom staff that leaves lines out between its top and bottom lines keeps the full span in MusicXML and
// hides the lines it leaves out, so its notes sit where the contiguous staff with the same bottom line puts them.
TEST(MusicXmlStaffLines, GappedStaffHidesOmittedLines)
{
    setupTestDataPaths();

    std::filesystem::path musxPath;
    copyInputToOutput("custom_staves_2.musx", musxPath);
    DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = musxPath;
    const auto inputData = formats::enigmaxml::detail::extractMusxInputData(musxPath, denigmaContext);
    std::string xml(inputData.primaryBuffer.begin(), inputData.primaryBuffer.end());

    // Measure 10's style draws lines 12 through 14 and measure 12's draws 12 through 15. Leave out line 13 of
    // the first and lines 13 and 14 of the second, which keeps each style's top and bottom lines.
    const auto replaceStyleLines = [&](const std::string& styleName, const std::vector<int>& lineNumbers) {
        const auto namePosition = xml.find("<styleName>" + styleName + "</styleName>");
        ASSERT_NE(namePosition, std::string::npos) << styleName;
        const auto start = xml.rfind("<customStaff>", namePosition);
        ASSERT_NE(start, std::string::npos) << styleName;
        const std::string endTag = "</customStaff>";
        const auto end = xml.find(endTag, start);
        ASSERT_NE(end, std::string::npos) << styleName;
        std::string lines = "<customStaff>";
        for (const int number : lineNumbers) {
            lines += "<staffLine>" + std::to_string(number) + "</staffLine>";
        }
        xml.replace(start, end + endTag.size() - start, lines + endTag);
    };
    replaceStyleLines("3-lines middle", {12, 14});
    replaceStyleLines("4-lines bottom", {12, 15});

    auto gappedInput = inputData;
    gappedInput.primaryBuffer.assign(xml.begin(), xml.end());
    const auto score = formats::musicxml::detail::createMusicXmlDocument(gappedInput, denigmaContext);
    const auto reference = loadScoreData(getInputPath() / "musicxml" / "custom_staves_2-ref.musicxml");
    ASSERT_TRUE(reference);
    const auto actual = summarizeStaffLines(score);
    auto expected = summarizeStaffLines(*reference);
    ASSERT_EQ(actual.size(), 12u);
    ASSERT_EQ(expected.size(), 12u);

    // Measure 10 is three lines with the middle one hidden. Measure 11 returns to four lines with none hidden,
    // and measure 12 is four lines with the inner two hidden. The notes do not move.
    expected[9].staffLines = 3;
    expected[9].hiddenLines = {{2, true}};
    expected[10].staffLines = 4;
    expected[11].staffLines = 4;
    expected[11].hiddenLines = {{2, true}, {3, true}};
    for (size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index], expected[index]) << "measure " << index + 1;
    }
}

// MusicXML numbers staff lines from the bottom line the staff has, so the same Finale position lands on
// different MusicXML positions as the staff's lines change.
TEST(MusicXmlStaffLines, StaffPositionsCountFromTheBottomLine)
{
    setupTestDataPaths();
    std::vector<char> xml;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), xml);
    const auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
    ASSERT_TRUE(document);

    using musx::dom::others::Staff;
    const auto createStaff = [&](std::optional<int> staffLines, std::optional<std::vector<int>> customStaff) {
        auto staff = std::make_shared<Staff>(document, musx::dom::SCORE_PARTID, Staff::ShareMode::All, 1);
        staff->staffLines = staffLines;
        staff->customStaff = std::move(customStaff);
        return musx::dom::MusxInstance<Staff>(staff);
    };
    using denigma::formats::musicxml::detail::musicXmlStaffPosition;

    const auto standard = createStaff(5, std::nullopt);
    EXPECT_EQ(musicXmlStaffPosition(standard, -8), 2);  // bottom line is line 1
    EXPECT_EQ(musicXmlStaffPosition(standard, -6), 4);  // treble clef's G line is line 2
    EXPECT_EQ(musicXmlStaffPosition(standard, 0), 10);  // top line is line 5
    EXPECT_EQ(musicXmlStaffPosition(standard, -10), 0); // middle C under treble clef is the ledger line below

    // Finale's one-line staff draws the middle line of the five-line staff, which MusicXML calls line 1.
    const auto middleLineOnly = createStaff(std::nullopt, std::vector<int>{13});
    EXPECT_EQ(musicXmlStaffPosition(middleLineOnly, -4), 2);
    EXPECT_EQ(musicXmlStaffPosition(middleLineOnly, -6), 0);

    // A one-line staff from the staff-lines setting draws its line at the reference line.
    const auto oneLine = createStaff(1, std::nullopt);
    EXPECT_EQ(musicXmlStaffPosition(oneLine, 0), 2);

    // Lines left out between the top and bottom lines still count, as hidden lines do in MusicXML.
    const auto outerLines = createStaff(std::nullopt, std::vector<int>{11, 15});
    EXPECT_EQ(musicXmlStaffPosition(outerLines, -8), 2);
    EXPECT_EQ(musicXmlStaffPosition(outerLines, 0), 10);

    // A staff with no lines counts as a standard staff.
    const auto noLines = createStaff(0, std::nullopt);
    EXPECT_EQ(musicXmlStaffPosition(noLines, -8), 2);
}

// custom_staves_1.musx changes the lines of a treble-clef staff every measure, adding lines above and below the
// standard five and leaving lines out between them. Pitches and clefs match Finale's export throughout. Where a
// staff leaves lines out, Finale writes only the lines it draws, which closes the gaps, while Denigma writes the
// full span and hides the lines left out.
TEST(MusicXmlStaffLines, GappedStavesKeepTheirSpan)
{
    setupTestDataPaths();

    const auto score = createScoreDataFromMusicXmlFixture("custom_staves_1.musx");
    const auto reference = loadScoreData(getInputPath() / "musicxml" / "custom_staves_1-ref.musicxml");
    ASSERT_TRUE(score);
    ASSERT_TRUE(reference);
    const auto actual = summarizeStaffLines(*score);
    auto expected = summarizeStaffLines(*reference);
    ASSERT_EQ(actual.size(), 7u);
    ASSERT_EQ(expected.size(), 7u);

    expected[3].staffLines = 7; // lines 9 and 11 through 15
    expected[3].hiddenLines = {{6, true}};
    expected[4].staffLines = 7; // lines 11 through 15 and 17
    expected[4].hiddenLines = {{2, true}};
    expected[5].staffLines = 7; // lines 10, 12 through 14, and 16
    expected[5].hiddenLines = {{2, true}, {6, true}};
    expected[6].staffLines = 5; // lines 11, 12, 14 and 15
    expected[6].hiddenLines = {{3, true}};
    for (size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index], expected[index]) << "measure " << index + 1;
    }
}

// custom_staves_3.musx is a treble clef over a one-line staff that draws the middle line of five. Finale's
// export writes the clef on line 2, which a reader would take to put E4 on the staff's line. Denigma moves the
// clef with the staff's lines instead, to line 0, so the line stays B4 as Finale draws it.
TEST(MusicXmlStaffLines, PitchedClefFollowsTheStaffLines)
{
    setupTestDataPaths();

    const auto score = createScoreDataFromMusicXmlFixture("custom_staves_3.musx");
    const auto reference = loadScoreData(getInputPath() / "musicxml" / "custom_staves_3-ref.musicxml");
    ASSERT_TRUE(score);
    ASSERT_TRUE(reference);
    const auto actual = summarizeStaffLines(*score);
    auto expected = summarizeStaffLines(*reference);
    ASSERT_FALSE(expected.empty());
    ASSERT_EQ(actual.size(), expected.size());
    ASSERT_EQ(expected[0].clefs.size(), 1u);
    ASSERT_EQ(expected[0].clefs[0], std::make_pair(mx::api::ClefSymbol::g, std::optional<int>(2)));
    expected[0].clefs[0].second = 0;
    for (size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index], expected[index]) << "measure " << index + 1;
    }
}

// hidden_staff_lines.musx hides the lines of a standard staff. Finale's export writes the staff as a plain
// five-line staff. Denigma hides each of the five lines, which keeps their positions for the clef and notes.
TEST(MusicXmlStaffLines, HiddenStaffLinesAreHidden)
{
    setupTestDataPaths();

    const auto score = createScoreDataFromMusicXmlFixture("hidden_staff_lines.musx");
    const auto reference = loadScoreData(getInputPath() / "musicxml" / "hidden_staff_lines-ref.musicxml");
    ASSERT_TRUE(score);
    ASSERT_TRUE(reference);
    const auto actual = summarizeStaffLines(*score);
    auto expected = summarizeStaffLines(*reference);
    ASSERT_FALSE(expected.empty());
    ASSERT_EQ(actual.size(), expected.size());
    expected[0].staffLines = 5;
    expected[0].hiddenLines = {{1, true}, {2, true}, {3, true}, {4, true}, {5, true}};
    for (size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index], expected[index]) << "measure " << index + 1;
    }
}
