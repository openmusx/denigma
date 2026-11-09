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
#include <cctype>
#include <cstddef>
#include <cstring>
#include <functional>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "pugixml.hpp"
#include "gtest/gtest.h"

#include "denigma/formats/musicxml.h"
#include "denigma/io/random_access_reader.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "musicxml_test.h"
#include "test_utils.h"

TEST(ConverterApi, EnigmaXmlToMusicXmlWritesToStream)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), input);

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findMultiOutput(denigma::FormatId::EnigmaXml, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "notAscii-其れ.enigmaxml";
    const auto result = converter->convert(
        std::as_bytes(std::span<const char>(input.data(), input.size())),
        [&](std::string_view, std::span<const std::byte> data) { xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_FALSE(xmlText.empty());

    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_TRUE(document.child("score-partwise"));
}

TEST(ConverterApi, MusxToMusicXmlWritesToStream)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    denigma::FileRandomAccessReader input(getInputPath() / utils::utf8ToPath("notAscii-其れ.musx"));
    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "notAscii-其れ.musx";
    const auto result = converter->convert(
        input, [&](std::string_view, std::span<const std::byte> data) { xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_FALSE(xmlText.empty());

    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_TRUE(document.child("score-partwise"));
}

TEST(ConverterApi, MusxToMusicXmlPreservesTextWhenAllFontsAreAvailable)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    denigma::FileRandomAccessReader input(getInputPath() / "tempo_varied_staves.musx");
    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "tempo_varied_staves.musx";
    options.common.allFontsAvailable = true;
    const auto result = converter->convert(
        input, [&](std::string_view, std::span<const std::byte> data) { xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();

    const auto tempoDirections = document.select_nodes("//direction[sound[@tempo='120']]");
    ASSERT_FALSE(tempoDirections.empty());
    for (const auto& tempoDirection : tempoDirections) {
        const auto direction = tempoDirection.node();
        EXPECT_FALSE(direction.select_node("direction-type/symbol"));
        const auto words = direction.select_node("direction-type/words").node();
        ASSERT_TRUE(words);
        EXPECT_STREQ(words.child_value(), "Tempo (∞=120)");
        EXPECT_STREQ(words.attribute("font-family").value(), "Patmm, text");
    }
}

TEST(ConverterApi, MusxToMusicXmlInvokesOutputCallbackForParts)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    struct Output
    {
        std::string suggestedName;
        std::string data;
    };

    std::vector<Output> outputs;
    denigma::FileRandomAccessReader input(getInputPath() / utils::utf8ToPath("notAscii-其れ.musx"));
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "notAscii-其れ.musx";
    options.allPartsAndScore = true;
    const auto result = converter->convert(
        input,
        [&](std::string_view suggestedName, std::span<const std::byte> data) {
            std::string outputData;
            outputData.resize(data.size());
            std::memcpy(outputData.data(), data.data(), data.size());
            outputs.push_back(Output{std::string(suggestedName), std::move(outputData)});
        },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_GE(outputs.size(), 2);
    EXPECT_EQ(outputs.front().suggestedName, "");

    bool foundNamedPart = false;
    for (const auto& output : outputs) {
        pugi::xml_document document;
        const auto parseResult = document.load_string(output.data.c_str());
        ASSERT_TRUE(parseResult) << parseResult.description();
        EXPECT_TRUE(document.child("score-partwise"));
        foundNamedPart = foundNamedPart || output.suggestedName == "オボえ";
    }
    EXPECT_TRUE(foundNamedPart);
}

TEST(ConverterApi, MusxToMusicXmlEmitsCuesOnlyWhereVisible)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::vector<std::string> outputs;
    denigma::FileRandomAccessReader input(getInputPath() / "multimeas_cue.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "multimeas_cue.musx";
    options.allPartsAndScore = true;
    const auto result = converter->convert(
        input,
        [&](std::string_view, std::span<const std::byte> data) { outputs.emplace_back(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_EQ(outputs.size(), 3u);

    size_t outputsWithCues{};
    for (size_t outputIndex = 0; outputIndex < outputs.size(); ++outputIndex) {
        pugi::xml_document document;
        const auto parseResult = document.load_string(outputs[outputIndex].c_str());
        ASSERT_TRUE(parseResult) << parseResult.description();
        const auto cueNotes = document.select_nodes("//note[cue]");
        if (outputIndex == 0) {
            EXPECT_TRUE(cueNotes.empty()) << "Cues hidden in the score context must be suppressed.";
        }
        if (!cueNotes.empty()) {
            ++outputsWithCues;
            EXPECT_EQ(cueNotes.size(), 8u);
            EXPECT_EQ(document.select_nodes("//note[cue and rest]").size(), 1u);
            EXPECT_EQ(document.select_nodes("//note[cue and pitch]").size(), 7u);
            const auto cueNoteSize = document.select_node("//defaults/appearance/note-size[@type='cue']").node();
            ASSERT_TRUE(cueNoteSize);
            EXPECT_EQ(cueNoteSize.text().as_int(), 81);
        }
    }
    EXPECT_EQ(outputsWithCues, 1u);
}

TEST(ConverterApi, MusxToMusicXmlPlumbsForcedCueLayer)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::FileRandomAccessReader input(getInputPath() / "forced_bass_clef.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "forced_bass_clef.musx";
    options.cueLayer = 1;
    const auto result = converter->convert(
        input, [&](std::string_view, std::span<const std::byte> data) { xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_EQ(document.select_nodes("//note[cue]").size(), 1u);
    EXPECT_EQ(document.select_nodes("//note[cue and rest]").size(), 1u);
}

TEST(ConverterApi, MusxToMusicXmlRetainsForcedCueLayerExpressions)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::FileRandomAccessReader input(getInputPath() / "techniques.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "techniques.musx";
    options.cueLayer = 1;
    const auto result = converter->convert(
        input, [&](std::string_view, std::span<const std::byte> data) { xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_EQ(document.select_nodes("//note[cue]").size(), 27u);
    EXPECT_EQ(document.select_nodes("//direction[voice='1']").size(), 5u);
    EXPECT_EQ(document.select_nodes("//direction[voice='1']/direction-type/words").size(), 5u);
}

TEST(ConverterApi, MusxToMusicXmlRetainsForcedCueLayerTiesAsNotationOnly)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::FileRandomAccessReader input(getInputPath() / "tie_target_types.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "tie_target_types.musx";
    options.cueLayer = 1;
    const auto result = converter->convert(
        input, [&](std::string_view, std::span<const std::byte> data) { xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_FALSE(document.select_nodes("//note[cue]/notations/tied[@type='start']").empty());
    EXPECT_FALSE(document.select_nodes("//note[cue]/notations/tied[@type='stop']").empty());
    EXPECT_TRUE(document.select_nodes("//note[cue]/tie").empty());
}

TEST(ConverterApi, EnigmaXmlToMusicXmlRetainsForcedGraceCueTieAsNotationOnly)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), input);

    pugi::xml_document inputDocument;
    const auto inputParseResult = inputDocument.load_buffer(input.data(), input.size());
    ASSERT_TRUE(inputParseResult) << inputParseResult.description();
    auto graceTieStartNote = inputDocument.select_node("//entry[@entnum='17']/note").node();
    auto tieEndNote = inputDocument.select_node("//entry[@entnum='5']/note").node();
    ASSERT_TRUE(graceTieStartNote);
    ASSERT_TRUE(tieEndNote);
    graceTieStartNote.append_child("tieStart");
    tieEndNote.append_child("tieEnd");

    std::ostringstream modifiedInput;
    inputDocument.save(modifiedInput);
    const auto modifiedInputText = modifiedInput.str();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findMultiOutput(denigma::FormatId::EnigmaXml, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "grace-cue-tie.enigmaxml";
    options.cueLayer = 1;
    const auto result = converter->convert(
        std::as_bytes(std::span<const char>(modifiedInputText.data(), modifiedInputText.size())),
        [&](std::string_view, std::span<const std::byte> data) { xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size()); },
        denigma::ConversionRequest{&options});

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_EQ(document.select_nodes("//note[grace and cue]/notations/tied[@type='start']").size(), 1u);
    EXPECT_TRUE(document.select_nodes("//note[grace and cue]/tie").empty());
}

TEST(MusicXmlChordFixture, ExportsChordsForInspection)
{
    setupTestDataPaths();

    const auto outputPath = denigma::test::musicxml::exportMusicXmlFixture("chords.musx");
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    pugi::xml_document document;
    ASSERT_TRUE(document.load_file(outputPath.c_str()));
    const auto firstKind = document.select_node("//harmony[1]/kind").node();
    ASSERT_TRUE(firstKind);
    EXPECT_STREQ(firstKind.child_value(), "major");
    ASSERT_TRUE(firstKind.attribute("text"));
    EXPECT_STREQ(firstKind.attribute("text").value(), "");

    const auto firstFrame = document.select_node("//harmony[1]/frame").node();
    ASSERT_TRUE(firstFrame);
    EXPECT_EQ(firstFrame.child("frame-strings").text().as_int(), 6);
    EXPECT_EQ(firstFrame.child("frame-frets").text().as_int(), 4);
    // The mx writer now uses the first supplied frame note instead of serializing its default
    // string-1/fret-0 placeholder.
    EXPECT_EQ(firstFrame.select_nodes("frame-note").size(), 5u);
    EXPECT_EQ(document.select_nodes("//measure[@number='15']//frame").size(), 1u);
    EXPECT_EQ(document.select_nodes("//measure[@number='16']//frame").size(), 2u);
    EXPECT_EQ(document.select_nodes("//measure[@number='20']//frame").size(), 0u);
    EXPECT_EQ(document.select_nodes("//measure[@number='21']//frame").size(), 1u);
    EXPECT_EQ(document.select_nodes("//measure[@number='22']//frame").size(), 2u);

    const auto expectBarre = [](const pugi::xml_node& frame, int startString, int startFret, int stopString, int stopFret) {
        const auto start = frame.select_node("frame-note[barre/@type='start']").node();
        ASSERT_TRUE(start);
        EXPECT_EQ(start.child("string").text().as_int(), startString);
        EXPECT_EQ(start.child("fret").text().as_int(), startFret);
        const auto stop = frame.select_node("frame-note[barre/@type='stop']").node();
        ASSERT_TRUE(stop);
        EXPECT_EQ(stop.child("string").text().as_int(), stopString);
        EXPECT_EQ(stop.child("fret").text().as_int(), stopFret);
    };

    // Finale's start string is the lowest-numbered string, which is the last frame note emitted, so it
    // carries the stop rather than the start.
    const auto barredFrame = document.select_node("//measure[@number='17']//frame[1]").node();
    ASSERT_TRUE(barredFrame);
    EXPECT_EQ(barredFrame.child("first-fret").text().as_int(), 6);
    expectBarre(barredFrame, 6, 6, 1, 6);

    // A barre anchors to its endpoint strings whatever fret is played there. Here string 1 is stopped at
    // fret 10, two frets above the barre, and still carries the stop.
    const auto highStopFrame = document.select_node("//measure[@number='24']//frame[1]").node();
    ASSERT_TRUE(highStopFrame);
    EXPECT_EQ(highStopFrame.child("first-fret").text().as_int(), 8);
    expectBarre(highStopFrame, 6, 8, 1, 10);

    // The same, for a barre that spans only part of the fretboard.
    const auto partialFrame = document.select_node("//measure[@number='25']//frame[1]").node();
    ASSERT_TRUE(partialFrame);
    EXPECT_EQ(partialFrame.child("first-fret").text().as_int(), 3);
    expectBarre(partialFrame, 5, 3, 1, 5);
}

TEST(MusicXmlChordFixture, WritesDegreeLayoutAttributesFromTheSuffix)
{
    setupTestDataPaths();

    const auto outputPath = denigma::test::musicxml::exportMusicXmlFixture("chords.musx");
    pugi::xml_document document;
    ASSERT_TRUE(document.load_file(outputPath.c_str()));

    // The parentheses in "m(maj7)" belong to the chord kind, not to a degree group, so the suffix
    // must not claim parenthesized degrees. Finale's own export of this fixture agrees.
    const auto majorMinor = document.select_node("//kind[@text='m(maj7)']").node();
    ASSERT_TRUE(majorMinor);
    EXPECT_FALSE(majorMinor.attribute("parentheses-degrees"));
    EXPECT_FALSE(majorMinor.attribute("stack-degrees"));
    EXPECT_FALSE(majorMinor.parent().child("degree"));

    // A parenthesized alteration is a degree group, whether or not the parentheses wrap the whole
    // suffix, but only vertically offset degrees are stacked.
    const auto parenthesized = document.select_node("//kind[@parentheses-degrees='yes']").node();
    ASSERT_TRUE(parenthesized);
    EXPECT_TRUE(parenthesized.parent().child("degree"));

    const auto stacked = document.select_node("//kind[@stack-degrees='yes']").node();
    ASSERT_TRUE(stacked);
    EXPECT_STREQ(stacked.attribute("parentheses-degrees").value(), "yes");
    EXPECT_TRUE(stacked.parent().child("degree"));

    const auto inlineDegrees = document.select_node("//kind[@text='+(add♯9add♭9)']").node();
    ASSERT_TRUE(inlineDegrees);
    EXPECT_STREQ(inlineDegrees.attribute("parentheses-degrees").value(), "yes");
    EXPECT_FALSE(inlineDegrees.attribute("stack-degrees"));

    // The seventh of "7sus4" sounds but is already spelled out by the kind text, so it must not
    // print a second time. Degrees the suffix does show carry no print-object at all.
    const auto suspended = document.select_node("//kind[@text='7sus4-3']").node();
    ASSERT_TRUE(suspended);
    const auto impliedDegree = suspended.parent().child("degree");
    ASSERT_TRUE(impliedDegree);
    EXPECT_STREQ(impliedDegree.child_value("degree-value"), "7");
    EXPECT_STREQ(impliedDegree.attribute("print-object").value(), "no");
    EXPECT_FALSE(parenthesized.parent().child("degree").attribute("print-object"));
}

TEST(ConverterApi, MusicXmlPitchOverrideMatchesFinaleDisplaySetting)
{
    setupTestDataPaths();
    using namespace denigma::formats::musicxml;
    using denigma::PitchDisplayMode;
    for (const auto* fixture : {"transposition.musx", "instchange_concert.musx", "large_orchestra.musx"}) {
        SCOPED_TRACE(fixture);
        denigma::DenigmaContext context(DENIGMA_NAME);
        context.inputFilePath = getInputPath() / fixture;
        const auto source = denigma::formats::enigmaxml::detail::extractMusxInputData(context.inputFilePath, context);
        for (const bool retainOctaves : {false, true}) {
            for (const bool sourceWritten : {false, true}) {
                for (const bool targetWritten : {false, true}) {
                    SCOPED_TRACE(::testing::Message() << retainOctaves << sourceWritten << targetWritten);
                    const auto convert = [&](bool storedWritten, PitchDisplayMode mode) {
                        pugi::xml_document document;
                        EXPECT_TRUE(document.load_buffer(source.primaryBuffer.data(), source.primaryBuffer.size()));
                        for (const auto& selected : document.select_nodes("//partGlobals")) {
                            auto node = selected.node();
                            node.remove_child("showTransposed");
                            if (storedWritten) {
                                node.append_child("showTransposed");
                            }
                        }
                        auto misc = document.select_node("//miscOptions").node();
                        EXPECT_TRUE(misc);
                        misc.remove_child("retainOctaveTransInConcertPitch");
                        if (retainOctaves) {
                            misc.append_child("retainOctaveTransInConcertPitch");
                        }
                        std::ostringstream stream;
                        document.save(stream);
                        const auto xml = stream.str();
                        Options options;
                        options.pitchDisplayMode = mode;
                        options.allPartsAndScore = true;
                        options.common.sourceName = fixture;
                        std::vector<std::pair<std::string, std::string>> outputs;
                        const auto result = EnigmaXmlToMusicXmlMultiOutputConverter{}.convert(
                            std::as_bytes(std::span(xml)),
                            [&](std::string_view name, std::span<const std::byte> bytes) {
                                outputs.emplace_back(name, std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
                            },
                            options);
                        EXPECT_FALSE(result.hasError());
                        EXPECT_FALSE(outputs.empty());
                        for (const auto& [name, output] : outputs) {
                            pugi::xml_document exported;
                            EXPECT_TRUE(exported.load_string(output.c_str()));
                            EXPECT_EQ(bool(exported.select_node("//concert-score")), !targetWritten) << name;
                            if (targetWritten) {
                                EXPECT_FALSE(exported.select_node("//for-part")) << name;
                            }
                        }
                        return outputs;
                    };
                    EXPECT_EQ(convert(sourceWritten, targetWritten ? PitchDisplayMode::Written : PitchDisplayMode::Concert),
                        convert(targetWritten, PitchDisplayMode::Preserve));
                }
            }
        }
    }
}

TEST(ConverterApi, MusicXmlWrittenOverridePreservesSoundingPitch)
{
    setupTestDataPaths();
    std::vector<char> input;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), input);
    pugi::xml_document source;
    ASSERT_TRUE(source.load_buffer(input.data(), input.size()));
    auto staff = source.select_node("//staffSpec[@cmper='1']").node();
    ASSERT_TRUE(staff);
    staff.remove_child("transposition");
    auto transposition = staff.append_child("transposition");
    transposition.append_child("setToClef");
    auto key = transposition.append_child("keysig");
    key.append_child("interval").text().set(3);
    key.append_child("adjust").text().set(-1);
    std::ostringstream stream;
    source.save(stream);
    const auto inputXml = stream.str();
    const auto convert = [&](denigma::PitchDisplayMode mode) {
        denigma::formats::musicxml::Options options;
        options.pitchDisplayMode = mode;
        std::string xml;
        const auto result = denigma::formats::musicxml::EnigmaXmlToMusicXmlMultiOutputConverter{}.convert(
            std::as_bytes(std::span(inputXml)),
            [&](std::string_view, std::span<const std::byte> bytes) { xml.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size()); },
            options);
        EXPECT_FALSE(result.hasError());
        return xml;
    };
    pugi::xml_document written;
    ASSERT_TRUE(written.load_string(convert(denigma::PitchDisplayMode::Written).c_str()));
    pugi::xml_document concert;
    ASSERT_TRUE(concert.load_string(convert(denigma::PitchDisplayMode::Concert).c_str()));
    EXPECT_FALSE(written.select_node("//for-part"));
    EXPECT_FALSE(written.select_node("//concert-score"));
    EXPECT_EQ(written.select_node("//transpose/chromatic").node().text().as_int(), -5);
    EXPECT_EQ(written.select_node("//transpose/diatonic").node().text().as_int(), -3);
    EXPECT_TRUE(concert.select_node("//concert-score"));
    const auto writtenNotes = written.select_nodes("//part[1]//pitch");
    const auto concertNotes = concert.select_nodes("//part[1]//pitch");
    ASSERT_GT(writtenNotes.size(), 0u);
    ASSERT_EQ(writtenNotes.size(), concertNotes.size());
    const auto semitones = [](pugi::xml_node pitch) {
        const std::map<std::string, int> steps{{"C", 0}, {"D", 2}, {"E", 4}, {"F", 5}, {"G", 7}, {"A", 9}, {"B", 11}};
        constexpr int semitonesPerOctave = 12;
        return semitonesPerOctave * pitch.child("octave").text().as_int() + steps.at(pitch.child("step").text().as_string())
               + pitch.child("alter").text().as_int();
    };
    for (size_t index = 0; index < writtenNotes.size(); ++index) {
        EXPECT_EQ(semitones(writtenNotes[index].node()) - 5, semitones(concertNotes[index].node()));
    }
}

namespace {

std::string convertEditedMusicXmlFixture(const std::filesystem::path& fixture, const std::function<void(pugi::xml_document&)>& edit)
{
    denigma::DenigmaContext context(DENIGMA_NAME);
    context.inputFilePath = getInputPath() / fixture;
    pugi::xml_document source;
    if (fixture.extension() == ".enigmaxml") {
        std::vector<char> input;
        readFile(context.inputFilePath, input);
        EXPECT_TRUE(source.load_buffer(input.data(), input.size()));
    } else {
        const auto input = denigma::formats::enigmaxml::detail::extractMusxInputData(context.inputFilePath, context);
        EXPECT_TRUE(source.load_buffer(input.primaryBuffer.data(), input.primaryBuffer.size()));
    }
    edit(source);
    std::ostringstream stream;
    source.save(stream);
    const auto xml = stream.str();
    std::string output;
    const auto result = denigma::formats::musicxml::EnigmaXmlToMusicXmlMultiOutputConverter{}.convert(
        std::as_bytes(std::span(xml)),
        [&](std::string_view, std::span<const std::byte> bytes) { output.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size()); },
        denigma::formats::musicxml::Options{});
    EXPECT_FALSE(result.hasError());
    return output;
}

} // namespace

TEST(MusicXmlMetadata, CreditsPreserveVisibleFontRuns)
{
    setupTestDataPaths();
    const auto xml = convertEditedMusicXmlFixture("page_text.musx", [](pugi::xml_document& source) {
        for (const auto& selected : source.select_nodes("//blockText")) {
            selected.node().text().set("^fontid(1)^size(12)^nfx(128)Hidden^nfx(0)First^size(18)^nfx(1)Second^nfx(128)Hidden");
        }
    });
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(xml.c_str()));
    const auto credits = document.select_nodes("//credit[credit-words]");
    ASSERT_FALSE(credits.empty());
    for (const auto& selected : credits) {
        const auto words = selected.node().select_nodes("credit-words");
        ASSERT_EQ(words.size(), 2u);
        EXPECT_STREQ(words[0].node().child_value(), "First");
        EXPECT_STREQ(words[1].node().child_value(), "Second");
        EXPECT_GT(words[1].node().attribute("font-size").as_double(), words[0].node().attribute("font-size").as_double());
        EXPECT_STREQ(words[1].node().attribute("font-weight").value(), "bold");
        EXPECT_TRUE(words[0].node().attribute("default-x"));
        EXPECT_FALSE(words[1].node().attribute("default-x"));
        EXPECT_FALSE(words[1].node().attribute("default-y"));
    }
}

TEST(MusicXmlChordFixture, LowercaseRootAndBassKeepTheirPitch)
{
    setupTestDataPaths();
    const auto xml = convertEditedMusicXmlFixture("chords.musx", [](pugi::xml_document& source) {
        const auto assignments = source.select_nodes("//chordAssign");
        ASSERT_FALSE(assignments.empty());
        for (const auto& selected : assignments) {
            auto chord = selected.node();
            chord.remove_child("rootLowerCase");
            chord.remove_child("bassLowerCase");
            chord.append_child("rootLowerCase");
            chord.append_child("bassLowerCase");
        }
    });
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(xml.c_str()));
    const auto roots = document.select_nodes("//root-step");
    const auto basses = document.select_nodes("//bass-step");
    ASSERT_FALSE(roots.empty());
    ASSERT_FALSE(basses.empty());
    for (const auto& nodes : {roots, basses}) {
        for (const auto& selected : nodes) {
            const auto step = selected.node();
            const std::string upper = step.child_value();
            ASSERT_EQ(upper.size(), 1u);
            EXPECT_EQ(std::string(step.attribute("text").value()), std::string(1, static_cast<char>(std::tolower(upper.front()))));
        }
    }
}

TEST(MusicXmlParts, StaffHiddenKeyKeepsAlterationsAndMode)
{
    setupTestDataPaths();
    const auto xml = convertEditedMusicXmlFixture(utils::utf8ToPath("reference/notAscii-其れ.enigmaxml"), [](pugi::xml_document& source) {
        for (const auto& selected : source.select_nodes("//staffSpec")) {
            auto staff = selected.node();
            staff.remove_child("hideKeySigs");
            staff.append_child("hideKeySigs");
        }
        for (const auto& selected : source.select_nodes("//measSpec[not(@part)]")) {
            auto measure = selected.node();
            measure.remove_child("keySig");
            measure.append_child("keySig").append_child("key").text().set(2);
        }
    });
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(xml.c_str()));
    const auto keys = document.select_nodes("//key");
    ASSERT_FALSE(keys.empty());
    for (const auto& selected : keys) {
        EXPECT_STREQ(selected.node().attribute("print-object").value(), "no");
        EXPECT_EQ(selected.node().child("fifths").text().as_int(), 2);
        EXPECT_STREQ(selected.node().child_value("mode"), "major");
    }
}

TEST(MusicXmlNotes, OrnamentAccidentalsStayWithTheirOrnament)
{
    setupTestDataPaths();
    constexpr int kTurnFlatSharp = 0xF5B6;
    const auto xml = convertEditedMusicXmlFixture(utils::utf8ToPath("reference/notAscii-其れ.enigmaxml"), [](pugi::xml_document& source) {
        auto font = source.select_node("//fontName[@cmper='0']/name").node();
        ASSERT_TRUE(font);
        font.text().set("Finale Maestro");
        auto articulation = source.select_node("//articDef[@cmper='1']").node();
        ASSERT_TRUE(articulation);
        articulation.child("charMain").text().set(kTurnFlatSharp);
        articulation.child("charAlt").text().set(kTurnFlatSharp);
    });
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(xml.c_str()));
    const auto ornaments = document.select_nodes("//ornaments[turn]");
    ASSERT_FALSE(ornaments.empty());
    for (const auto& selected : ornaments) {
        const auto ornament = selected.node();
        const auto marks = ornament.select_nodes("accidental-mark");
        ASSERT_EQ(marks.size(), 2u);
        EXPECT_STREQ(ornament.first_child().name(), "turn");
        EXPECT_STREQ(marks[0].node().child_value(), "flat");
        EXPECT_STREQ(marks[0].node().attribute("placement").value(), "above");
        EXPECT_STREQ(marks[1].node().child_value(), "sharp");
        EXPECT_STREQ(marks[1].node().attribute("placement").value(), "below");
    }
}
