// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/musx_reader.h"
#include "denigma/classify/jumps.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "input/input.h"
#include "mnxdom.h"
#include "musicxml/musicxml_test.h"
#include "musx/musx.h"
#include "test_utils.h"
#include "gtest/gtest.h"

using namespace musx::dom;

// These tests read the tutorials and sample files distributed with Finale 27 from DENIGMA_FINALE_MUSIC_FILES_DIR,
// which tests/CMakeLists.txt sets. They skip when those files are not installed.

#define SKIP_UNLESS_FINALE_MUSIC_FILES(root)                                                                                                      \
    if ((root).empty() || !std::filesystem::is_directory(root)) {                                                                                 \
        GTEST_SKIP() << "Finale 27 Music Files not found at \"" << (root).string() << "\". Set DENIGMA_FINALE_MUSIC_FILES_DIR to run this test."; \
    }

namespace {

using denigma::classify::jump::Jump;

std::filesystem::path finaleMusicFilesDir()
{
    return std::filesystem::path(DENIGMA_FINALE_MUSIC_FILES_DIR);
}

// Text repeats in the installed files. Only these files have any, and none uses D.C., D.C. al Coda, D.S. al Fine
// or D.S. al Coda.
constexpr std::string_view braham = "Worksheets & Repertoire/Repertoire/Jazz/Braham Limehouse Blues.musx";
constexpr std::string_view matchingMix3 = "Worksheets & Repertoire/Worksheets/(5000) Terminology/5014 Matching mix 3.musx";
constexpr std::string_view matchingRepeats = "Worksheets & Repertoire/Worksheets/(5000) Terminology/5008 Matching repeats.musx";
constexpr std::string_view mozart = "Worksheets & Repertoire/Repertoire/Classical/Piano Keyboard/Mozart German Dance.musx";
constexpr std::string_view individualSymbols =
    "Worksheets & Repertoire/Worksheets/(6000) Games, Flash Cards, Fingerings/6400 Individual Flash Cards/6401 Individual Symbols.musx";
constexpr std::string_view threeBlindMice = "Worksheets & Repertoire/Repertoire/Rounds/Three Blind Mice.musx";

DocumentPtr loadSample(const std::filesystem::path& path)
{
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = path;
    const denigma::MusxLoggerScope loggerScope(denigma::makeMusxLogCallback(denigmaContext));
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(path, denigmaContext);
    return denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
}

/// Finale's stored shape for a syllable's word extension, found from the shape's start rather than through the
/// entry assignment, which Finale does not always save.
MusxInstance<others::SmartShape> findWordExtensionShape(const DocumentPtr& document, const details::LyricAssign& assignment)
{
    for (const auto& shape : document->getOthers()->getAllSources<others::SmartShape>()) {
        if (shape->shapeType == others::SmartShape::ShapeType::WordExtension && shape->startLyricType == assignment.getLyricTextType()
            && shape->startLyricNum == assignment.lyricNumber && shape->startTermSeg->endPoint->entryNumber == assignment.getEntryNumber()) {
            return shape;
        }
    }
    return nullptr;
}

/// Whether Finale's automatic update of word extensions leaves the shape as it is, so that it need not be the
/// endpoint Finale's rule produces.
bool isWordExtensionShapeFrozen(const others::SmartShape& shape)
{
    constexpr Cmper frozenMarker = static_cast<Cmper>(-1);
    const auto& endAdjustment = shape.endTermSeg->endPointAdj;
    return shape.endLyricNum == frozenMarker || (endAdjustment && endAdjustment->active);
}

/// Copies an installed sample into the test output directory and exports it to MNX and MusicXML there, returning
/// the copy's path.
std::filesystem::path exportSample(const std::filesystem::path& root, std::string_view sample)
{
    const auto sourcePath = root / std::filesystem::path(sample);
    auto inputPath = getOutputPath() / "finale_samples" / sourcePath.filename();
    std::filesystem::create_directories(inputPath.parent_path());
    std::filesystem::copy(sourcePath, inputPath, std::filesystem::copy_options::overwrite_existing);
    ArgList args = {DENIGMA_NAME, "export", pathString(inputPath), "--mnx", "--musicxml"};
    checkStderr({"Processing", pathString(inputPath.filename())},
        [&]() { EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export: " << pathString(inputPath); });
    return inputPath;
}

} // namespace

TEST(FinaleSamples, InferredWordExtensionEndpointsMatchFinale)
{
    const auto root = finaleMusicFilesDir();
    SKIP_UNLESS_FINALE_MUSIC_FILES(root);

    // Files whose smart word extensions exercise LyricAssign::calcInferredWordExtensionEndpoint. Every shape Finale
    // recomputes must end where the inference does.
    constexpr std::string_view samples[] = {
        "Tutorials/EntryExercises.musx", // barlines that end word extensions
        "Tutorials/Tutorial 9.musx", // a melisma over another verse's syllables
        "Worksheets & Repertoire/Repertoire/Classical/Vocal/Schubert Ave Maria.musx", // a grace note within an extension
        "Worksheets & Repertoire/Repertoire/Holiday Patriotic/Leadsheets/God Rest Ye Merry Gentlemen.musx", // frozen shapes, hard spaces
        "Worksheets & Repertoire/Repertoire/Holiday Patriotic/Leadsheets/O Holy Night.musx", // many extensions across barlines
        "Worksheets & Repertoire/Repertoire/Holiday Patriotic/Leadsheets/Silent Night.musx", // an extension in voice 2
        "Worksheets & Repertoire/Repertoire/Holiday Patriotic/Leadsheets/What Child Final.musx", // frozen shapes
        "Worksheets & Repertoire/Repertoire/Holiday Patriotic/Vocal/Christmas Caroling Collection/Caroling Letter Format/Angels We Have "
        "Heard.musx", // frozen shapes over a refrain
        "Worksheets & Repertoire/Repertoire/Holiday Patriotic/Vocal/Christmas Caroling Collection/Caroling Letter Format/Hark! the "
        "Herald.musx", // manually positioned ends
        "Worksheets & Repertoire/Repertoire/Rounds/Three Blind Mice.musx", // shapes without an entry assignment
    };

    size_t loaded = 0;
    size_t compared = 0;
    for (const auto sample : samples) {
        const auto path = root / std::filesystem::path(sample);
        if (!std::filesystem::exists(path)) {
            ADD_FAILURE() << "missing sample: " << sample;
            continue;
        }
        const auto document = loadSample(path);
        ASSERT_TRUE(document) << sample;
        loaded++;
        const auto compare = [&](auto tag) {
            using Assignment = typename decltype(tag)::type;
            for (const auto& assignment : document->getDetails()->getAllSources<Assignment>()) {
                if (!assignment->wext) {
                    continue;
                }
                const auto shape = findWordExtensionShape(document, *assignment);
                if (!shape || isWordExtensionShapeFrozen(*shape)) {
                    continue;
                }
                compared++;
                const auto expected = shape->endTermSeg->endPoint->entryNumber;
                const auto describe = [&]() {
                    std::ostringstream description;
                    description << sample << ": " << Assignment::XmlNodeName << " " << assignment->lyricNumber << " on entry "
                                << assignment->getEntryNumber();
                    return description.str();
                };
                const auto inferred = assignment->calcInferredWordExtensionEndpoint();
                ASSERT_TRUE(inferred) << describe();
                EXPECT_EQ(inferred->getEntry()->getEntryNumber(), expected) << describe();
                // Whether or not the shape is reachable through its entry assignment.
                const auto endpoint = assignment->calcWordExtensionEndpoint();
                ASSERT_TRUE(endpoint) << describe();
                EXPECT_EQ(endpoint->getEntry()->getEntryNumber(), expected) << describe();
            }
        };
        compare(std::type_identity<details::LyricAssignVerse>{});
        compare(std::type_identity<details::LyricAssignChorus>{});
        compare(std::type_identity<details::LyricAssignSection>{});
    }
    EXPECT_EQ(loaded, std::size(samples));
    EXPECT_GT(compared, 0u);
}

TEST(FinaleSamples, JumpClassificationMatchesFinale)
{
    const auto root = finaleMusicFilesDir();
    SKIP_UNLESS_FINALE_MUSIC_FILES(root);

    struct ExpectedJump
    {
        MeasCmper measure;
        Jump visual;
        Jump playback;
    };
    struct Sample
    {
        std::string_view path;
        std::vector<ExpectedJump> jumps;
    };
    const Sample samples[] = {
        // Plain "D.S." with an absolute jump to a measure that has no segno.
        {braham, {{77, Jump::DalSegno, Jump::DalSegno}}},
        // A "Fine" with no playback action, then a segno that plays as one.
        {matchingMix3, {{6, Jump::Fine, Jump::None}, {7, Jump::Segno, Jump::Segno}}},
        // A segno glyph set to jump, so it plays as D.S.
        {matchingRepeats, {{6, Jump::Segno, Jump::DalSegno}, {7, Jump::Fine, Jump::Fine}}},
        {mozart, {{16, Jump::Fine, Jump::Fine}, {32, Jump::DCAlFine, Jump::DCAlFine}}},
        // Maestro coda and segno glyphs, all set to jump.
        {individualSymbols,
            {{40, Jump::Coda, Jump::ToCoda}, {41, Jump::Segno, Jump::DalSegno}, {44, Jump::Segno, Jump::DalSegno}, {45, Jump::Coda, Jump::ToCoda}}},
        // Printed but given no playback action.
        {threeBlindMice, {{2, Jump::Fine, Jump::None}, {14, Jump::DCAlFine, Jump::None}}},
    };

    for (const auto& sample : samples) {
        const auto document = loadSample(root / std::filesystem::path(sample.path));
        ASSERT_TRUE(document) << sample.path;
        const auto assignments = document->getOthers()->getAllSources<others::TextRepeatAssign>();
        ASSERT_EQ(assignments.size(), sample.jumps.size()) << sample.path;
        for (size_t i = 0; i < assignments.size(); i++) {
            const auto& expected = sample.jumps[i];
            const auto jump = denigma::classify::classifyJump(assignments[i]);
            EXPECT_EQ(assignments[i]->getCmper(), expected.measure) << sample.path;
            EXPECT_EQ(jump.visual, expected.visual) << sample.path << " m" << expected.measure;
            EXPECT_EQ(jump.playback, expected.playback) << sample.path << " m" << expected.measure;
        }
    }
}

TEST(FinaleSamples, JumpsExportToMnx)
{
    const auto root = finaleMusicFilesDir();
    SKIP_UNLESS_FINALE_MUSIC_FILES(root);

    struct ExpectedMeasure
    {
        size_t measure;
        std::optional<mnx::JumpType> jump;
        bool segno;
        bool coda;
        bool fine;
    };
    struct Sample
    {
        std::string_view path;
        std::vector<ExpectedMeasure> measures;
    };
    // Every measure with a jump, segno, coda or fine. A jump that Finale does not play is not exported.
    const Sample samples[] = {
        {braham, {{77, mnx::JumpType::Segno, false, false, false}}},
        {matchingMix3, {{7, std::nullopt, true, false, false}}},
        {matchingRepeats, {{6, mnx::JumpType::Segno, true, false, false}, {7, std::nullopt, false, false, true}}},
        {mozart, {{16, std::nullopt, false, false, true}, {32, mnx::JumpType::DcAlFine, false, false, false}}},
        {individualSymbols, {{40, mnx::JumpType::ToCoda, false, true, false}, {41, mnx::JumpType::Segno, true, false, false},
                                {44, mnx::JumpType::Segno, true, false, false}, {45, mnx::JumpType::ToCoda, false, true, false}}},
        {threeBlindMice, {}},
    };

    for (const auto& sample : samples) {
        auto mnxPath = exportSample(root, sample.path);
        mnxPath.replace_extension(".mnx");
        const auto doc = mnx::Document::create(mnxPath);
        const auto measures = doc.global().measures();
        size_t next = 0;
        for (size_t i = 0; i < measures.size(); i++) {
            const auto measure = measures[i];
            if (!measure.jump() && !measure.segno() && !measure.coda() && !measure.fine()) {
                continue;
            }
            ASSERT_LT(next, sample.measures.size()) << sample.path << " unexpected jump in m" << (i + 1);
            const auto& expected = sample.measures[next++];
            ASSERT_EQ(i + 1, expected.measure) << sample.path;
            EXPECT_EQ(measure.jump() ? std::optional(measure.jump()->type()) : std::nullopt, expected.jump) << sample.path << " m" << (i + 1);
            EXPECT_EQ(measure.segno().has_value(), expected.segno) << sample.path << " m" << (i + 1);
            EXPECT_EQ(measure.coda().has_value(), expected.coda) << sample.path << " m" << (i + 1);
            EXPECT_EQ(measure.fine().has_value(), expected.fine) << sample.path << " m" << (i + 1);
        }
        EXPECT_EQ(next, sample.measures.size()) << sample.path;
    }
}

TEST(FinaleSamples, JumpsExportToMusicXml)
{
    const auto root = finaleMusicFilesDir();
    SKIP_UNLESS_FINALE_MUSIC_FILES(root);

    struct ExpectedDirection
    {
        size_t measure;
        std::string_view text; ///< the words, or "segno" or "coda" for a glyph
        std::string_view sound; ///< the jump attributes of the sound element, or empty for none
    };
    struct Sample
    {
        std::string_view path;
        std::vector<ExpectedDirection> directions;
    };
    // The first part's jump directions, one per staff that shows them. A jump that Finale does not play keeps
    // its words and has no sound.
    const Sample samples[] = {
        {braham, {{77, "D.S.", "dalsegno=9"}, {77, "D.S.", "dalsegno=9"}}},
        {matchingMix3, {{6, "Fine", ""}, {7, "segno", "segno=7"}}},
        // The segno jumps absolutely with no target measure, so there is no dalsegno destination to write.
        {matchingRepeats, {{6, "segno", ""}, {7, "Fine", "fine=7"}}},
        {mozart, {{16, "Fine", "fine=16"}, {16, "Fine", "fine=16"}, {32, "D.C. al Fine", "dacapo"}, {32, "D.C. al Fine", "dacapo"}}},
        {threeBlindMice, {{2, "Fine", ""}, {14, "D.C. al Fine", ""}}},
    };

    const auto describeSound = [](const mx::api::SoundData& sound) {
        std::string result;
        const auto append = [&](std::string_view name, const std::string& value) {
            if (!value.empty()) {
                result += (result.empty() ? "" : " ") + std::string(name) + "=" + value;
            }
        };
        if (sound.dacapo == mx::api::Bool::yes) {
            result = "dacapo";
        }
        append("segno", sound.segno);
        append("dalsegno", sound.dalsegno);
        append("coda", sound.coda);
        append("tocoda", sound.tocoda);
        append("fine", sound.fine);
        return result;
    };

    for (const auto& sample : samples) {
        auto musicXmlPath = exportSample(root, sample.path);
        musicXmlPath.replace_extension(".musicxml");
        const auto score = denigma::test::musicxml::loadScoreData(musicXmlPath);
        ASSERT_TRUE(score) << sample.path;
        ASSERT_FALSE(score->parts.empty()) << sample.path;
        size_t next = 0;
        const auto& measures = score->parts.front().measures;
        for (size_t i = 0; i < measures.size(); i++) {
            for (const auto& staff : measures[i].staves) {
                for (const auto& direction : staff.directions) {
                    const auto sound = describeSound(direction.soundData);
                    std::string text;
                    for (const auto& choice : direction.directionTypes) {
                        if (choice.isSegno()) {
                            text = "segno";
                        } else if (choice.isCoda()) {
                            text = "coda";
                        }
                    }
                    for (const auto& words : denigma::test::musicxml::directionWords(direction)) {
                        text += words.text;
                    }
                    const bool isJump = !sound.empty() || text == "segno" || text == "coda" || text == "Fine" || text.starts_with("D.");
                    if (!isJump) {
                        continue;
                    }
                    ASSERT_LT(next, sample.directions.size()) << sample.path << " unexpected jump in m" << (i + 1) << ": " << text;
                    const auto& expected = sample.directions[next++];
                    EXPECT_EQ(i + 1, expected.measure) << sample.path << ": " << text;
                    EXPECT_EQ(text, expected.text) << sample.path << " m" << (i + 1);
                    EXPECT_EQ(sound, expected.sound) << sample.path << " m" << (i + 1);
                }
            }
        }
        EXPECT_EQ(next, sample.directions.size()) << sample.path;
    }
}
