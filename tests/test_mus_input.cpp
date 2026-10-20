// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "core/musx_reader.h"
#include "denigma/formats/mnx.h"
#include "denigma/formats/mss.h"
#include "denigma/formats/musicxml.h"
#include "denigma/formats/svg.h"
#include "export/export.h"
#include "input/input.h"
#include "test_utils.h"

using namespace denigma;
using finale_mus_reader::MusRecognition;

namespace {

Buffer legacyFixture(const char* name)
{
    return readInputBytes(getInputPath() / "legacy-mus" / name);
}

std::span<const std::byte> textBytes(std::string_view text)
{
    return std::as_bytes(std::span(text.data(), text.size()));
}

} // namespace

TEST(MusInput, RecognitionAndContentPrecedence)
{
    setupTestDataPaths();
    for (const char* name : {"F100-note.mus", "F300-baseline.mus", "F2002-baseline.mus", "F2012-noteartexp.mus"}) {
        SCOPED_TRACE(name);
        const auto data = legacyFixture(name);
        const auto bytes = std::as_bytes(std::span(data));
        EXPECT_EQ(recognizeMus(bytes), MusRecognition::Candidate);
        for (const char* source : {"score.mus", "score", "score.binary", "score.musx", "score.enigmaxml", "score.enigmaxml.zip"}) {
            EXPECT_EQ(resolveInputFormat(bytes, source), InputFormat::Mus);
        }
        EXPECT_NE(recognizeMus(bytes.first(1)), MusRecognition::Candidate);
    }
    EXPECT_EQ(recognizeMus({}), MusRecognition::NotFinale);
    EXPECT_EQ(recognizeMus(textBytes("unrelated bytes")), MusRecognition::NotFinale);
    EXPECT_EQ(recognizeMus(textBytes("ENIGMA TRANSPORTABLE FILE")), MusRecognition::FinaleNonScore);
    auto damaged = legacyFixture("F300-baseline.mus");
    damaged.resize(damaged.size() / 2);
    EXPECT_EQ(recognizeMus(std::as_bytes(std::span(damaged))), MusRecognition::Damaged);
    EXPECT_THROW(resolveInputFormat(std::as_bytes(std::span(damaged)), "broken.musx"), std::invalid_argument);
    EXPECT_EQ(resolveInputFormat({}, "score.MUSX"), InputFormat::Musx);
    EXPECT_EQ(resolveInputFormat({}, "score.ENIGMAXML.ZIP"), InputFormat::ZippedEnigmaXml);
}

TEST(MusInput, DocumentsRetainIdentityAndConstructionPolicy)
{
    setupTestDataPaths();
    const auto buffer = legacyFixture("F2008-parts-voiced.mus");
    DenigmaContext context("test");
    context.inputFilePath = utils::utf8ToPath("score-其れ.binary");
    const auto input = prepareMusInput(std::as_bytes(std::span(buffer)));
    const auto applied = createMusxDocument<MusxReader>(input, context, musx::dom::PartVoicingPolicy::Apply);
    const auto ignored = createMusxDocument<MusxReader>(input, context);
    ASSERT_TRUE(applied);
    ASSERT_TRUE(ignored);
    EXPECT_NE(applied, ignored);
    EXPECT_EQ(applied->getSourcePath(), context.inputFilePath);
    EXPECT_EQ(applied->getPartVoicingPolicy(), musx::dom::PartVoicingPolicy::Apply);
    EXPECT_EQ(ignored->getPartVoicingPolicy(), musx::dom::PartVoicingPolicy::Ignore);
    EXPECT_GT(applied->getOthers()->getArray<musx::dom::others::PartDefinition>(musx::dom::SCORE_PARTID).size(), std::size_t(1));
}

TEST(MusInput, NativeAdaptersAndSelectedParts)
{
    setupTestDataPaths();
    ConverterRegistry registry;
    formats::musicxml::registerConverters(registry);
    formats::mnx::registerConverters(registry);
    formats::mss::registerConverters(registry);
    formats::svg::registerConverters(registry);
    const auto data = legacyFixture("F100-note.mus");
    const auto bytes = std::as_bytes(std::span(data));
    for (const auto format : {FormatId::MusicXml, FormatId::MnxJson, FormatId::MssXml}) {
        const auto result = registry.convert(FormatId::Mus, format, bytes);
        ASSERT_TRUE(result) << static_cast<int>(format);
        ASSERT_FALSE(result.outputs().empty());
        EXPECT_FALSE(result.outputs().front().data.empty());
        const auto output = result.outputs().front().data;
        const std::string text(reinterpret_cast<const char*>(output.data()), output.size());
        if (format == FormatId::MusicXml) {
            pugi::xml_document xml;
            ASSERT_TRUE(xml.load_buffer(text.data(), text.size()));
            const auto pitch = xml.select_node("//note/pitch").node();
            ASSERT_TRUE(pitch);
            EXPECT_STREQ(pitch.child_value("step"), "F");
            EXPECT_EQ(pitch.child("alter").text().as_int(), 1);
            EXPECT_EQ(pitch.child("octave").text().as_int(), 4);
        } else if (format == FormatId::MnxJson) {
            auto actual = nlohmann::json::parse(text);
            actual["mnx"].erase("_x");
            std::ifstream reference(getInputPath() / "reference/legacy-mus/F100-note.mnx");
            EXPECT_EQ(actual, nlohmann::json::parse(reference));
        } else if (format == FormatId::MssXml) {
            const auto path = getOutputPath() / "F100-note.mss";
            pugi::xml_document xml;
            ASSERT_TRUE(xml.load_buffer(text.data(), text.size()));
            auto style = xml.child("museScore").child("Style");
            // Dot widths depend on which fonts the host can resolve.
            style.remove_child("repeatBarlineDotSeparation");
            style.remove_child("dotDotDistance");
            std::ofstream outputFile(path);
            xml.save(outputFile);
            outputFile.close();
            compareFiles(path, getInputPath() / "reference/legacy-mus/F100-note.mss");
        }
    }
    const auto shapeData = legacyFixture("F100-shapexp.mus");
    const auto svg = registry.convert(FormatId::Mus, FormatId::Svg, std::as_bytes(std::span(shapeData)));
    ASSERT_TRUE(svg);
    ASSERT_FALSE(svg.outputs().empty());
    EXPECT_FALSE(svg.outputs().front().data.empty());

    const auto voiced = legacyFixture("F2008-parts-voiced.mus");
    formats::musicxml::Options options;
    options.allPartsAndScore = true;
    const auto parts = registry.convert(FormatId::Mus, FormatId::MusicXml, std::as_bytes(std::span(voiced)), {&options});
    ASSERT_TRUE(parts);
    EXPECT_GT(parts.outputs().size(), std::size_t(1));
    options.allPartsAndScore = false;
    options.partName = "";
    const auto selected = registry.convert(FormatId::Mus, FormatId::MusicXml, std::as_bytes(std::span(voiced)), {&options});
    ASSERT_TRUE(selected);
    EXPECT_EQ(selected.outputs().size(), std::size_t(1));
    for (const auto format : {FormatId::MusicXml, FormatId::MnxJson, FormatId::MssXml, FormatId::Svg}) {
        EXPECT_FALSE(registry.convert(FormatId::Mus, format, textBytes("invalid")));
    }
}

TEST(MusInput, ResourcesOwnBuffersValidateAndReuse)
{
    setupTestDataPaths();
    const auto resources = [] {
        const std::string fonts = "Maestro\nPetaluma\n";
        const std::string map = "<Finale><PercussionNoteMap name=\"Example\"/></Finale>";
        const std::string soundmap = "<Soundmap><Description><Name>Example</Name></Description></Soundmap>";
        const std::string catalog = "<InstrumentCatalog/>";
        const std::vector<MusResource> inputs = {
            {MusResourceKind::SymbolFonts, "MacSymbolFonts.txt", textBytes(fonts)},
            {MusResourceKind::PercussionMap, "Example.xml", textBytes(map)},
            {MusResourceKind::Soundmap, "first.soundmap", textBytes(soundmap)},
            {MusResourceKind::Soundmap, "second.soundmap", textBytes(soundmap)},
            {MusResourceKind::InstrumentCatalog, "catalog.xml", textBytes(catalog)},
            {MusResourceKind::PercussionConversionTable, "conversion.txt", textBytes("// empty conversion table\n")},
        };
        return MusResources::create(inputs);
    }();
    ASSERT_TRUE(resources);
    DenigmaContext context("test");
    context.musResources = resources;
    const auto buffer = legacyFixture("F100-note.mus");
    const auto input = prepareMusInput(std::as_bytes(std::span(buffer)));
    EXPECT_TRUE(createMusxDocument<MusxReader>(input, context));
    EXPECT_TRUE(createMusxDocument<MusxReader>(input, context));
    const std::vector<MusResource> invalid = {{MusResourceKind::Soundmap, "bad", textBytes("<Other/>")}};
    EXPECT_THROW((void)MusResources::create(invalid), std::invalid_argument);
    const std::vector<MusResource> duplicate = {
        {MusResourceKind::SymbolFonts, "one", textBytes("Maestro")},
        {MusResourceKind::SymbolFonts, "two", textBytes("Petaluma")},
    };
    EXPECT_THROW((void)MusResources::create(duplicate), std::invalid_argument);
    const std::vector<MusResource> unnamed = {{MusResourceKind::PercussionMap, "", textBytes("<Finale/>")}};
    EXPECT_THROW((void)MusResources::create(unnamed), std::invalid_argument);
    EXPECT_TRUE(MusResources::create());
}

TEST(MusInput, CliDiscoveryDefaultsAndUnsupportedOutputs)
{
    setupTestDataPaths();
    const auto folder = getOutputPath() / "mus-discovery";
    std::filesystem::create_directories(folder / "nested");
    const auto source = getInputPath() / "legacy-mus/F100-note.mus";
    for (const char* name : {"plain", "other.binary", "disguised.enigmaxml.zip"}) {
        std::filesystem::copy_file(source, folder / name, std::filesystem::copy_options::overwrite_existing);
    }
    std::filesystem::copy_file(source, folder / "nested/deeper", std::filesystem::copy_options::overwrite_existing);
    std::ofstream(folder / "unrelated.txt") << "unrelated data";
    ArgList args = {DENIGMA_NAME, pathString(folder), "--recursive", "--force", "--no-log"};
    EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0);
    for (const char* name : {"plain.musicxml", "other.musicxml", "disguised.enigmaxml.musicxml", "nested/deeper.musicxml"}) {
        EXPECT_TRUE(std::filesystem::exists(folder / name)) << name;
    }
    ArgList rejected = {DENIGMA_NAME, pathString(folder / "plain"), "--enigmaxml", pathString(folder / "rejected.enigmaxml"), "--no-log"};
    EXPECT_NE(denigmaTestMain(rejected.argc(), rejected.argv()), 0);
    EXPECT_FALSE(std::filesystem::exists(folder / "rejected.enigmaxml"));
    ExportCommand command;
    DenigmaContext context("test");
    const auto prepared = prepareInput(folder / "disguised.enigmaxml.zip", context);
    EXPECT_EQ(prepared.originalFormat, InputFormat::Mus);
    EXPECT_EQ(command.defaultOutputFormat("disguised.enigmaxml.zip", prepared), MUSICXML_EXTENSION);
}

TEST(MusInput, CliWildcardsExclusionsAndResourcePaths)
{
    setupTestDataPaths();
    const auto folder = getOutputPath() / "mus-selection";
    std::filesystem::create_directories(folder / "excluded");
    const auto source = getInputPath() / "legacy-mus/F100-note.mus";
    std::filesystem::copy_file(source, folder / "keep.binary");
    std::filesystem::copy_file(source, folder / "excluded/hidden.binary");
    std::ofstream(folder / "fonts.txt") << "Maestro\n";
    std::ofstream(folder / "map.xml") << "<Finale/>";
    std::ofstream(folder / "table.txt") << "// no conversions\n";
    std::ofstream(folder / "first.soundmap") << "<Soundmap/>";
    std::ofstream(folder / "second.soundmap") << "<Soundmap/>";
    std::ofstream(folder / "catalog.xml") << "<InstrumentCatalog/>";
    ArgList args = {DENIGMA_NAME, pathString(folder), "--recursive", "--exclude-folder", "excluded", "--no-log", "--force", "--mus-symbol-fonts",
        pathString(folder / "fonts.txt"), "--mus-percussion-map", pathString(folder / "map.xml"), "--mus-percussion-conversion-table",
        pathString(folder / "table.txt"), "--mus-soundmap", pathString(folder / "first.soundmap"), "--mus-soundmap",
        pathString(folder / "second.soundmap"), "--mus-instrument-catalog", pathString(folder / "catalog.xml")};
    EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0);
    EXPECT_TRUE(std::filesystem::exists(folder / "keep.musicxml"));
    EXPECT_FALSE(std::filesystem::exists(folder / "excluded/hidden.musicxml"));
    const auto wildcardOutput = folder / "wildcard.musicxml";
    ArgList wildcard = {DENIGMA_NAME, pathString(folder / "*.binary"), "--musicxml", pathString(wildcardOutput), "--force", "--no-log"};
    EXPECT_EQ(denigmaTestMain(wildcard.argc(), wildcard.argv()), 0);
    EXPECT_TRUE(std::filesystem::exists(wildcardOutput));
    ArgList missing = {DENIGMA_NAME, pathString(folder / "keep.binary"), "--mus-symbol-fonts"};
    EXPECT_NE(denigmaTestMain(missing.argc(), missing.argv()), 0);
    ArgList duplicate = {DENIGMA_NAME, pathString(folder / "keep.binary"), "--mus-symbol-fonts", "one", "--mus-symbol-fonts", "two"};
    EXPECT_NE(denigmaTestMain(duplicate.argc(), duplicate.argv()), 0);
    ArgList invalid = {DENIGMA_NAME, pathString(folder / "keep.binary"), "--mus-soundmap", pathString(folder / "fonts.txt"), "--no-log"};
    EXPECT_NE(denigmaTestMain(invalid.argc(), invalid.argv()), 0);
}

TEST(MusInput, CliPreservesInputsAndReportsDamagedDiscovery)
{
    setupTestDataPaths();
    const auto folder = getOutputPath() / "mus-protection";
    std::filesystem::create_directories(folder);
    const auto original = legacyFixture("F300-baseline.mus");
    const auto write = [&](const std::filesystem::path& path, std::span<const char> data) {
        std::ofstream output(path, std::ios::binary);
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
    };
    write(folder / "score", original);
    write(folder / "score.musicxml", original);
    write(folder / "broken", std::span(original).first(original.size() / 2));
    ArgList args = {DENIGMA_NAME, pathString(folder), "--force", "--no-log"};
    checkStderr("Skipping damaged legacy Finale file", [&] { EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0); });
    EXPECT_EQ(readInputBytes(folder / "score"), original);
    EXPECT_EQ(readInputBytes(folder / "score.musicxml"), original);
    ArgList explicitBroken = {DENIGMA_NAME, pathString(folder / "broken"), "--no-log"};
    EXPECT_NE(denigmaTestMain(explicitBroken.argc(), explicitBroken.argv()), 0);
}

TEST(MusInput, ProvenanceUsesContentAndPreservesMisleadingFilename)
{
    setupTestDataPaths();
    const auto data = legacyFixture("F100-note.mus");
    const auto bytes = std::as_bytes(std::span(data));
    const std::string sourceName = "score.enigmaxml.zip";
    formats::musicxml::Options xmlOptions;
    xmlOptions.common.sourceName = sourceName;
    std::string xmlText;
    const auto xmlResult = formats::musicxml::MusToMusicXmlMultiOutputConverter().convert(
        bytes,
        [&](std::string_view, std::span<const std::byte> output) { xmlText.assign(reinterpret_cast<const char*>(output.data()), output.size()); },
        xmlOptions);
    ASSERT_TRUE(xmlResult);
    pugi::xml_document xml;
    ASSERT_TRUE(xml.load_buffer(xmlText.data(), xmlText.size()));
    EXPECT_STREQ(xml.select_node("//miscellaneous-field[@name='source-format']").node().child_value(), "mus");
    EXPECT_EQ(xml.select_node("//miscellaneous-field[@name='source-filename']").node().child_value(), sourceName);
    formats::mnx::Options mnxOptions;
    mnxOptions.common.sourceName = sourceName;
    std::ostringstream output;
    ASSERT_TRUE(formats::mnx::MusToMnxJsonConverter().convert(bytes, output, mnxOptions));
    const auto mnx = nlohmann::json::parse(output.str());
    const auto& source = mnx.at("mnx").at("_x").at("mnxdom").at("source");
    EXPECT_EQ(source.at("format"), "mus");
    EXPECT_EQ(source.at("filename"), sourceName);
}

TEST(MusInput, MassageAcceptsExplicitLegacyCompanionWithoutExtension)
{
    setupTestDataPaths();
    const auto companion = getOutputPath() / "legacy-companion";
    std::filesystem::copy_file(getInputPath() / "legacy-mus/F100-note.mus", companion);
    const auto musicXml = getOutputPath() / "source.musicxml";
    ArgList exportArgs = {DENIGMA_NAME, pathString(companion), "--musicxml", pathString(musicXml), "--no-log"};
    ASSERT_EQ(denigmaTestMain(exportArgs.argc(), exportArgs.argv()), 0);
    const auto bytes = readInputBytes(musicXml);
    pugi::xml_document xml;
    ASSERT_TRUE(xml.load_buffer(bytes.data(), bytes.size()));
    xml.child("score-partwise").child("identification").child("encoding").child("software").text().set("Finale 2012");
    {
        std::ofstream rewritten(musicXml);
        xml.save(rewritten);
    }
    const auto output = getOutputPath() / "result.musicxml";
    ArgList massageArgs = {
        DENIGMA_NAME, "massage", pathString(musicXml), "--finale-file", pathString(companion), "--musicxml", pathString(output), "--no-log"};
    EXPECT_EQ(denigmaTestMain(massageArgs.argc(), massageArgs.argv()), 0);
    EXPECT_TRUE(std::filesystem::exists(output));
}
