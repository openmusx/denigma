/*
 * Copyright (C) 2025, Robert Patterson
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
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <set>
#include <string>

#include "core/denigma.h"
#include "denigma/classify/articulations.h"
#include "denigma/classify/noteheads.h"
#include "formats/mnx/mnx.h"
#include "formats/mnx/mnx_noteheads.h"
#include "test_utils.h"
#include "gtest/gtest.h"

using namespace denigma;

namespace {

nlohmann::json exportMnxFixture(const std::string& fileName)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput(fileName, inputPath);
    ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx"};
    checkStderr({"Processing", pathString(inputPath.filename()), "!validation error"},
        [&]() { EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath); });

    nlohmann::json mnx;
    openJson(inputPath.parent_path() / (inputPath.stem().string() + ".mnx"), mnx);
    return mnx;
}

// The lyric lines of the event with @p eventId, or an empty object when the event has none.
nlohmann::json lyricLinesOf(const nlohmann::json& mnx, const std::string& eventId)
{
    for (const auto& part : mnx["parts"]) {
        for (const auto& measure : part["measures"]) {
            for (const auto& sequence : measure["sequences"]) {
                for (const auto& content : sequence["content"]) {
                    if (content.value("id", "") == eventId) {
                        return content.contains("lyrics") ? content["lyrics"].value("lines", nlohmann::json::object()) : nlohmann::json::object();
                    }
                }
            }
        }
    }
    return nlohmann::json::object();
}

} // namespace

TEST(MnxSequences, CalcPointing)
{
    using formats::mnx::detail::enumConvert;

    using Placement = classify::GlyphStyle::Placement;
    EXPECT_EQ(enumConvert<mnxdom::MarkingUpDownAuto>(Placement::Above), mnxdom::MarkingUpDownAuto::Up);
    EXPECT_EQ(enumConvert<mnxdom::MarkingUpDownAuto>(Placement::Below), mnxdom::MarkingUpDownAuto::Down);
    EXPECT_EQ(enumConvert<mnxdom::MarkingUpDownAuto>(Placement::Automatic), mnxdom::MarkingUpDownAuto::Auto);
    EXPECT_EQ(enumConvert<mnxdom::MarkingUpDownAuto>(classify::GlyphStyle{}.placement), mnxdom::MarkingUpDownAuto::Auto);
}

TEST(MnxSequences, NoteheadShapesUseSchemaShapes)
{
    using formats::mnx::detail::enumConvert;
    using Shape = classify::notehead::Shape;

    EXPECT_EQ(enumConvert<mnxdom::NoteheadShape>(Shape::Circled), mnxdom::NoteheadShape::Circled);
    EXPECT_EQ(enumConvert<mnxdom::NoteheadShape>(Shape::LargeCircled), mnxdom::NoteheadShape::Circled);
    EXPECT_EQ(enumConvert<mnxdom::NoteheadShape>(Shape::Round), mnxdom::NoteheadShape::Custom);
    EXPECT_EQ(enumConvert<mnxdom::NoteheadShape>(Shape::Slashed), mnxdom::NoteheadShape::Slashed);
    EXPECT_EQ(enumConvert<mnxdom::NoteheadShape>(Shape::BackSlashed), mnxdom::NoteheadShape::BackSlashed);
}

namespace {

classify::NoteheadClassification makeNoteheadClassification(
    classify::notehead::Shape shape, classify::notehead::Fill fill, std::string glyphName, bool glyphPreferred = false)
{
    classify::NoteheadClassification result;
    result.shape = shape;
    result.fill = fill;
    result.glyphName = std::move(glyphName);
    result.glyphPreferred = glyphPreferred;
    return result;
}

classify::PercussionNoteheadClassification makeKit(const classify::NoteheadClassification& quarter, const classify::NoteheadClassification& half)
{
    classify::PercussionNoteheadClassification result;
    result.family = quarter.shape;
    result.quarterAndShorter = quarter;
    result.half = half;
    result.whole = half;
    result.breveAndLonger = half;
    return result;
}

} // namespace

TEST(MnxSequences, NoteheadsAreComparedAsMnxWritesThem)
{
    using namespace formats::mnx::detail;
    using Shape = classify::notehead::Shape;
    using Fill = classify::notehead::Fill;
    using musx::dom::NoteType;

    // A circle's size is not written, so a large circle inherits its component's regular circle.
    const auto circled = makeNoteheadClassification(Shape::Circled, Fill::Filled, "noteheadCircledBlack");
    const auto largeCircled = makeNoteheadClassification(Shape::LargeCircled, Fill::Filled, "noteheadCircledBlackLarge");
    const auto circledKit = calcKitComponentNotehead(makeKit(circled, circled));
    EXPECT_EQ(circledKit, (MnxNotehead{mnxdom::NoteheadShape::Circled, std::nullopt}));
    EXPECT_EQ(calcNoteNotehead(largeCircled, NoteType::Quarter), circledKit);

    // A standard slash inherits its component's slash; a slash whose glyph is preferred does not.
    const auto slash = makeNoteheadClassification(Shape::LargeSlash, Fill::Filled, "noteheadSlashVerticalEnds");
    const auto smallSlash = makeNoteheadClassification(Shape::SmallSlash, Fill::Filled, "noteheadSlashVerticalEndsSmall", true);
    const auto slashKit = calcKitComponentNotehead(makeKit(slash, slash));
    EXPECT_EQ(calcNoteNotehead(slash, NoteType::Quarter), slashKit);
    EXPECT_NE(calcNoteNotehead(smallSlash, NoteType::Quarter), slashKit);
    EXPECT_EQ(calcNoteNotehead(smallSlash, NoteType::Quarter).glyph, std::optional<std::string>("noteheadSlashVerticalEndsSmall"));

    // A custom component names its quarter glyph, so a half note that draws a different glyph writes it.
    const auto roundQuarter = makeNoteheadClassification(Shape::Round, Fill::Filled, "noteheadRoundBlack");
    const auto roundHalf = makeNoteheadClassification(Shape::Round, Fill::Unfilled, "noteheadRoundWhite");
    const auto roundKit = calcKitComponentNotehead(makeKit(roundQuarter, roundHalf));
    EXPECT_EQ(calcNoteNotehead(roundQuarter, NoteType::Quarter), roundKit);
    EXPECT_NE(calcNoteNotehead(roundHalf, NoteType::Half), roundKit);

    // A regular component writes nothing, so its notes inherit the reader's default oval.
    const auto black = makeNoteheadClassification(Shape::Regular, Fill::Filled, "noteheadBlack");
    const auto regularKit = calcKitComponentNotehead(makeKit(black, makeNoteheadClassification(Shape::Regular, Fill::Unfilled, "noteheadHalf")));
    EXPECT_EQ(regularKit, MnxNotehead{});
    EXPECT_EQ(calcNoteNotehead(black, NoteType::Quarter), regularKit);
    // A filled notehead on a half note names its glyph, because MNX would otherwise draw it hollow.
    EXPECT_NE(calcNoteNotehead(black, NoteType::Half), regularKit);
}

TEST(MnxSequences, Voice2TripletAtEnd)
{
    // test file contains voice 2 triplet at end of bar. The triplet was being overfilled.

    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("v2triplet_at_end.musx", inputPath);
    ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx"};
    checkStderr({"Processing", pathString(inputPath.filename()), "!validation error"},
        [&]() { EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath); });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "v2triplet_at_end.mnx");
}

TEST(MnxSequences, Voice2TupletIncomplete)
{
    // test file contains incomplete v2 triplet that should be reported without additional adding compensationg spacer to main sequence

    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("v2tuplet_incomplete.musx", inputPath);
    ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx"};
    checkStderr({"Processing", pathString(inputPath.filename()), "less than the expected", "!more than the expected"},
        [&]() { EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath); });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "v2tuplet_incomplete.mnx");
}

TEST(MnxSequences, NestedTuplets)
{
    // test file contains a nested tuplets ending on the same note.

    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tuplets_nested.musx", inputPath);
    ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx"};
    checkStderr({"Processing", pathString(inputPath.filename()), "!validation error"},
        [&]() { EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath); });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "tuplets_nested.mnx");
}

TEST(MnxSequences, NestedTrailingSingletonTuplet)
{
    // test file contains a trailing singleton tuplet nested inside another tuplet.

    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tuplet-nested-singleton.musx", inputPath);
    ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx"};
    checkStderr({"Processing", pathString(inputPath.filename()), "!validation error"},
        [&]() { EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath); });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "tuplet-nested-singleton.mnx");
}

TEST(MnxSequences, CrossStaffNotesUseNoteLevelStaffOverride)
{
    const auto mnx = exportMnxFixture("cross_staffs.musx");

    ASSERT_TRUE(mnx.contains("parts"));
    ASSERT_FALSE(mnx["parts"].empty());
    const auto& measures = mnx["parts"][0]["measures"];

    int crossedEventCount = 0;
    for (const auto& measure : measures) {
        if (!measure.contains("sequences")) {
            continue;
        }
        for (const auto& sequence : measure["sequences"]) {
            if (!sequence.contains("content")) {
                continue;
            }
            for (const auto& item : sequence["content"]) {
                if (!item.contains("notes")) {
                    continue;
                }
                int crossedNotesInEvent = 0;
                for (const auto& note : item["notes"]) {
                    if (note.contains("staff")) {
                        ++crossedNotesInEvent;
                        EXPECT_EQ(note["staff"], 2) << item.dump(4);
                    }
                }
                if (crossedNotesInEvent > 0) {
                    ++crossedEventCount;
                    ASSERT_TRUE(sequence.contains("staff")) << sequence.dump(4);
                    EXPECT_EQ(sequence["staff"], 1) << sequence.dump(4);
                    EXPECT_FALSE(item.contains("staff")) << item.dump(4);
                    EXPECT_EQ(crossedNotesInEvent, 1) << item.dump(4);
                }
            }
        }
    }

    EXPECT_EQ(crossedEventCount, 4);
}

TEST(MnxSequences, GraceBeamsRetainSlashState)
{
    const auto mnx = exportMnxFixture("grace_beamed.musx");

    ASSERT_TRUE(mnx.contains("parts"));
    ASSERT_FALSE(mnx["parts"].empty());
    const auto& content = mnx["parts"][0]["measures"][0]["sequences"][0]["content"];

    std::vector<std::reference_wrapper<const nlohmann::json>> graceGroups;
    for (const auto& item : content) {
        if (item.value("type", "") == "grace") {
            graceGroups.emplace_back(std::cref(item));
        }
    }

    ASSERT_EQ(graceGroups.size(), 3u);

    const auto& firstGrace = graceGroups[0].get();
    const auto& beamedGrace = graceGroups[1].get();
    const auto& lastGrace = graceGroups[2].get();

    EXPECT_FALSE(firstGrace.contains("slash")) << firstGrace.dump(4); // omitted means slashed
    EXPECT_EQ(firstGrace["content"].size(), 1u);

    ASSERT_TRUE(beamedGrace.contains("slash")) << beamedGrace.dump(4);
    EXPECT_FALSE(beamedGrace["slash"]) << beamedGrace.dump(4);
    EXPECT_EQ(beamedGrace["content"].size(), 2u);

    EXPECT_FALSE(lastGrace.contains("slash")) << lastGrace.dump(4); // omitted means slashed
    EXPECT_EQ(lastGrace["content"].size(), 1u);
}

namespace {

// Summarizes a measure's sequences as (staff, isFullMeasureRest) pairs in document order.
std::vector<std::pair<int, bool>> summarizeSequences(const nlohmann::json& measure)
{
    std::vector<std::pair<int, bool>> result;
    for (const auto& sequence : measure.value("sequences", nlohmann::json::array())) {
        result.emplace_back(sequence.value("staff", 1), sequence.contains("fullMeasure"));
    }
    return result;
}

} // namespace

TEST(MnxSequences, EmptyMeasuresGetFullMeasureRestPerStaff)
{
    const auto mnx = exportMnxFixture("voices_keyboard.musx");
    ASSERT_TRUE(mnx.contains("parts"));
    ASSERT_EQ(mnx["parts"].size(), 1u);
    const auto& measures = mnx["parts"][0]["measures"];
    ASSERT_EQ(measures.size(), 7u);

    using Summary = std::vector<std::pair<int, bool>>;
    // Measure 3 is empty on both staves of the piano part.
    EXPECT_EQ(summarizeSequences(measures[2]), (Summary{{1, true}, {2, true}})) << measures[2].dump(4);
    // Measure 6 holds real whole-rest entries, which are exported as full-measure rests in their own right.
    EXPECT_EQ(summarizeSequences(measures[5]), (Summary{{1, true}, {2, true}})) << measures[5].dump(4);
    // Measure 7 is empty, but staff 1 has "Display Rests in Empty Measures" off through a staff style.
    // Staff 2 hides entered rests through "Display Rests", which does not suppress the empty-measure rest.
    EXPECT_EQ(summarizeSequences(measures[6]), (Summary{{2, true}})) << measures[6].dump(4);
}

TEST(MnxSequences, EmptyMeasureRestOmitsStaffOnSingleStaffPart)
{
    const auto mnx = exportMnxFixture("slurs_beatattached.musx");
    ASSERT_TRUE(mnx.contains("parts"));
    ASSERT_EQ(mnx["parts"].size(), 2u);

    // Part 1 blanks measures 2 through 4 through a staff style, so they stay empty.
    const auto& firstPartMeasures = mnx["parts"][0]["measures"];
    ASSERT_EQ(firstPartMeasures.size(), 4u);
    for (size_t x = 1; x < firstPartMeasures.size(); x++) {
        EXPECT_TRUE(firstPartMeasures[x]["sequences"].empty()) << "part 1 measure " << (x + 1) << firstPartMeasures[x].dump(4);
    }

    // Part 2 is a single-staff part, so its empty measures 3 and 4 get a rest that names no staff.
    const auto& secondPartMeasures = mnx["parts"][1]["measures"];
    ASSERT_EQ(secondPartMeasures.size(), 4u);
    for (size_t x = 2; x < secondPartMeasures.size(); x++) {
        const auto& sequences = secondPartMeasures[x]["sequences"];
        ASSERT_EQ(sequences.size(), 1u) << "part 2 measure " << (x + 1) << secondPartMeasures[x].dump(4);
        EXPECT_FALSE(sequences[0].contains("staff")) << sequences[0].dump(4);
        EXPECT_FALSE(sequences[0].contains("voice")) << sequences[0].dump(4);
        EXPECT_TRUE(sequences[0].contains("fullMeasure")) << sequences[0].dump(4);
        EXPECT_TRUE(sequences[0]["content"].empty()) << sequences[0].dump(4);
    }
}

TEST(MnxSequences, DirectionHintsFollowLayerStemSettings)
{
    const auto mnx = exportMnxFixture("voices.musx");
    const auto& measures = mnx["parts"][0]["measures"];
    ASSERT_GE(measures.size(), 2u);

    const auto countStemDirections = [](const nlohmann::json& sequence) {
        size_t count = 0;
        const std::function<void(const nlohmann::json&)> walk = [&](const nlohmann::json& node) {
            if (node.is_object()) {
                count += node.contains("stemDirection") ? 1 : 0;
                for (const auto& [key, value] : node.items()) {
                    walk(value);
                }
            } else if (node.is_array()) {
                for (const auto& value : node) {
                    walk(value);
                }
            }
        };
        walk(sequence);
        return count;
    };

    // Measure 1 has one layer per staff, so the layer settings are not in effect. Staff 1 uses v2,
    // which freezes every stem in the layer.
    for (const auto& sequence : measures[0]["sequences"]) {
        EXPECT_FALSE(sequence.contains("directionHint")) << sequence.dump(4);
        if (sequence["staff"] == 1) {
            EXPECT_GT(countStemDirections(sequence), 0u) << sequence.dump(4);
        } else {
            EXPECT_EQ(countStemDirections(sequence), 0u) << sequence.dump(4);
        }
    }

    // Measure 2 has two layers on each staff, and Finale's defaults freeze layer 1 up and layer 2 down.
    // The hint carries the direction, so no event needs its own.
    const auto& sequences = measures[1]["sequences"];
    ASSERT_EQ(sequences.size(), 4u) << measures[1].dump(4);
    for (const auto& sequence : sequences) {
        const std::string voice = sequence["voice"];
        const std::string expected = voice.ends_with("layer1") ? "upper" : "lower";
        EXPECT_EQ(sequence.value("directionHint", ""), expected) << sequence.dump(4);
        EXPECT_EQ(countStemDirections(sequence), 0u) << sequence.dump(4);
    }
}

TEST(MnxSequences, BeamSideTupletsNameTheStemSide)
{
    const auto mnx = exportMnxFixture("tuplets_nested.musx");
    std::vector<std::string> placements;
    const std::function<void(const nlohmann::json&)> walk = [&](const nlohmann::json& node) {
        if (node.is_object()) {
            if (node.value("type", "") == "tuplet") {
                placements.push_back(node.value("placement", "auto"));
            }
            for (const auto& [key, value] : node.items()) {
                walk(value);
            }
        } else if (node.is_array()) {
            for (const auto& value : node) {
                walk(value);
            }
        }
    };
    walk(mnx["parts"]);
    // Every tuplet in the fixture uses beam-side positioning, so each follows its first entry's stem.
    EXPECT_EQ(placements, (std::vector<std::string>{"below", "below", "above", "below"}));
}

TEST(MnxSequences, FullMeasureRestStatesWholeGlyphInBreveMeasure)
{
    const auto mnx = exportMnxFixture("whole_rests.musx");
    const auto& measures = mnx["parts"][0]["measures"];
    ASSERT_GE(measures.size(), 7u);
    for (size_t x = 0; x < measures.size(); x++) {
        for (const auto& sequence : measures[x]["sequences"]) {
            if (!sequence.contains("fullMeasure")) {
                continue;
            }
            // The fixture is in 4/2, where readers would otherwise draw a breve rest. Measure 7 holds
            // its only full-measure rest; the others hold whole-note events.
            if (x == 6) {
                ASSERT_TRUE(sequence["fullMeasure"].contains("visualDuration")) << sequence.dump(4);
                EXPECT_EQ(sequence["fullMeasure"]["visualDuration"]["base"], "whole");
            } else {
                EXPECT_FALSE(sequence["fullMeasure"].contains("visualDuration")) << "measure " << (x + 1) << sequence.dump(4);
            }
        }
    }
}

TEST(MnxSequences, HiddenFullMeasureRestSpansTheMeasure)
{
    // Measures 6 (3/4) and 7 (4/2) each hold only a hidden whole rest, which Finale enters in any meter.
    const auto mnx = exportMnxFixture("hidden_full_measure_rests.musx");
    const auto& measures = mnx["parts"][0]["measures"];
    ASSERT_EQ(measures.size(), 7u);

    const auto expectSingleSpace = [&](size_t measureIndex, int numerator, int denominator) {
        const auto& sequences = measures[measureIndex]["sequences"];
        ASSERT_EQ(sequences.size(), 1u) << measures[measureIndex].dump(4);
        EXPECT_FALSE(sequences[0].contains("fullMeasure")) << sequences[0].dump(4);
        const auto& content = sequences[0]["content"];
        ASSERT_EQ(content.size(), 1u) << sequences[0].dump(4);
        EXPECT_EQ(content[0]["type"], "space") << content[0].dump(4);
        EXPECT_EQ(content[0]["duration"], nlohmann::json::array({numerator, denominator})) << content[0].dump(4);
    };
    expectSingleSpace(5, 3, 4);
    expectSingleSpace(6, 2, 1);
}

TEST(MnxSequences, EntryOverfillingATupletIsShortenedToFit)
{
    // Each of measures 1-3 and 5 ends a tuplet with an entry whose written duration overfills it.
    const auto mnx = exportMnxFixture("tuplet_completion.enigmaxml");
    ASSERT_TRUE(mnx.contains("parts"));
    const auto& measures = mnx["parts"][0]["measures"];
    ASSERT_GE(measures.size(), 5u);

    const auto findEvent = [](const nlohmann::json& measure, const std::string& eventId) -> nlohmann::json {
        std::function<nlohmann::json(const nlohmann::json&)> search = [&](const nlohmann::json& content) -> nlohmann::json {
            for (const auto& item : content) {
                if (item.value("id", "") == eventId) {
                    return item;
                }
                if (item.contains("content")) {
                    if (auto found = search(item["content"]); !found.is_null()) {
                        return found;
                    }
                }
            }
            return nullptr;
        };
        for (const auto& sequence : measure["sequences"]) {
            if (auto found = search(sequence["content"]); !found.is_null()) {
                return found;
            }
        }
        return nullptr;
    };

    // 5:4 eighths: a written dotted half becomes the dotted quarter that ends the tuplet.
    EXPECT_EQ(findEvent(measures[0], "ev7")["duration"], nlohmann::json::parse(R"({"base": "quarter", "dots": 1})")) << measures[0].dump(4);
    // Triplet nested in 5:4: a written quarter becomes the eighth that ends the triplet.
    EXPECT_EQ(findEvent(measures[1], "ev12")["duration"], nlohmann::json::parse(R"({"base": "eighth"})")) << measures[1].dump(4);
    // A written quarter that ends both a triplet and its enclosing tuplet becomes an eighth.
    EXPECT_EQ(findEvent(measures[2], "ev21")["duration"], nlohmann::json::parse(R"({"base": "eighth"})")) << measures[2].dump(4);
    // A written eighth that ends a 3-sixteenths tuplet becomes a sixteenth.
    EXPECT_EQ(findEvent(measures[4], "ev31")["duration"], nlohmann::json::parse(R"({"base": "16th"})")) << measures[4].dump(4);
}

TEST(MnxSequences, WordExtensionTargetsTheEventItEndsOn)
{
    const auto mnx = exportMnxFixture("verse_chorus_section.musx");
    EXPECT_EQ(mnx["mnx"]["support"]["useLyricExtenders"], true);

    const auto extended = lyricLinesOf(mnx, "ev20");
    for (const auto* lineId : {"s1", "s2"}) {
        ASSERT_TRUE(extended.contains(lineId)) << extended.dump(4);
        EXPECT_EQ(extended[lineId]["extender"], nlohmann::json::parse(R"({"target": "ev24"})")) << lineId;
        EXPECT_FALSE(extended[lineId].contains("id")) << lineId;
    }
    const auto plain = lyricLinesOf(mnx, "ev19");
    ASSERT_TRUE(plain.contains("s1"));
    EXPECT_FALSE(plain["s1"].contains("extender"));
    EXPECT_FALSE(plain["s1"].contains("id"));
}

TEST(MnxSequences, LyricLineOrderListsEveryLineByBaseline)
{
    const auto mnx = exportMnxFixture("verse_chorus_section.musx");
    const auto& lyrics = mnx["global"]["lyrics"];
    EXPECT_EQ(lyrics["lineOrder"], nlohmann::json::parse(R"(["v1", "c1", "s1", "v2", "c2", "s2"])")) << lyrics.dump(4);
    std::set<std::string> ordered;
    for (const auto& lineId : lyrics["lineOrder"]) {
        EXPECT_TRUE(ordered.insert(lineId.get<std::string>()).second) << lineId;
    }
    std::set<std::string> described;
    for (const auto& [lineId, metadata] : lyrics["lineMetadata"].items()) {
        described.insert(lineId);
    }
    EXPECT_EQ(ordered, described);
}

TEST(MnxSequences, WordExtensionEndingOnItsOwnEventHasNoTarget)
{
    const auto mnx = exportMnxFixture("for_health_and_strength.musx");
    const auto line = lyricLinesOf(mnx, "ev137")["v2"];
    EXPECT_EQ(line["extender"], nlohmann::json::object()) << line.dump(4);
    EXPECT_FALSE(line.contains("id"));
}

TEST(MnxSequences, LegacyWordExtensionTargetsItsInferredEnd)
{
    // Smart word extensions are off, so each extension stores only a drawn length; the end is inferred from
    // the music.
    const auto mnx = exportMnxFixture("lyric_legacy_wext.musx");
    EXPECT_EQ(lyricLinesOf(mnx, "ev23")["v1"]["extender"], nlohmann::json::parse(R"({"target": "ev24"})"));
    EXPECT_EQ(lyricLinesOf(mnx, "ev25")["v1"]["extender"], nlohmann::json::parse(R"({"target": "ev26"})"));
    // A rest follows, so the extension ends on its own event.
    EXPECT_EQ(lyricLinesOf(mnx, "ev27")["v1"]["extender"], nlohmann::json::object());

    // The chorus on the same entries types its extensions as underscores, which stay in the text.
    const auto chorus = lyricLinesOf(mnx, "ev23")["c1"];
    EXPECT_EQ(chorus["text"], "strength_");
    EXPECT_FALSE(chorus.contains("extender"));
}

TEST(MnxSequences, PlaceholderAndDuplicateSyllables)
{
    // lyric_placeholders.enigmaxml adds a Verse 2 of lone non-breaking spaces and one real syllable to
    // lyric_legacy_wext.musx. See tests/test_lyrics.cpp for its assignments.
    const auto mnx = exportMnxFixture("lyric_placeholders.enigmaxml");

    // A blank duplicate does not replace the real syllable.
    EXPECT_EQ(lyricLinesOf(mnx, "ev17")["v2"], nlohmann::json::parse(R"({"text": "as", "type": "whole"})"));
    // A blank syllable without an extension writes no line.
    EXPECT_FALSE(lyricLinesOf(mnx, "ev18").contains("v2")) << lyricLinesOf(mnx, "ev18").dump(4);
    // A blank syllable with an extension keeps the extension and no text.
    EXPECT_EQ(lyricLinesOf(mnx, "ev23")["v2"], nlohmann::json::parse(R"({"text": "", "extender": {"target": "ev24"}})"));
    EXPECT_EQ(lyricLinesOf(mnx, "ev25")["v2"], nlohmann::json::parse(R"({"text": "", "extender": {"target": "ev27"}})"));
}

TEST(MnxSequences, StrippedUnderscoresWriteAnUntargetedExtender)
{
    // See tests/test_lyrics.cpp for lyric_underscores.enigmaxml.
    const auto mnx = exportMnxFixture("lyric_underscores.enigmaxml");
    const auto lines = lyricLinesOf(mnx, "ev13");
    EXPECT_EQ(lines["v1"], nlohmann::json::parse(R"({"text": "It", "type": "whole", "extender": {}})")) << lines.dump(4);
    EXPECT_EQ(lines["v2"], nlohmann::json::parse(R"({"text": "", "extender": {}})")) << lines.dump(4);
}
