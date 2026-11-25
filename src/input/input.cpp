// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "input.h"

#include <limits>
#include <unordered_set>

#include "core/musx_reader.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "musx/factory/PlaybackResourcesFactory.h"
#include "utils/ziputils.h"

namespace denigma {
namespace {

std::span<const std::uint8_t> unsignedBytes(std::span<const std::byte> bytes)
{
    return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}

Buffer copyInputBytes(std::span<const std::byte> bytes)
{
    Buffer buffer(bytes.size());
    if (!bytes.empty()) {
        std::memcpy(buffer.data(), bytes.data(), bytes.size());
    }
    return buffer;
}

} // namespace

finale_mus_reader::MusRecognition recognizeMus(std::span<const std::byte> bytes) noexcept
{
    return finale_mus_reader::recognize(unsignedBytes(bytes)).verdict;
}

struct MusResources::Impl
{
    Impl(musx::dom::PlaybackResourcesPtr resources, finale_mus_reader::Reader value)
        : playbackResources(std::move(resources)), reader(std::move(value))
    {}
    musx::dom::PlaybackResourcesPtr playbackResources;
    finale_mus_reader::Reader reader;
};

MusResources::MusResources(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{}
MusResources::~MusResources() = default;

std::shared_ptr<const MusResources> MusResources::create(std::span<const MusResource> resources)
{
    // The playback resources are built once and shared: the reader uses them to import legacy
    // files, and every document, whatever its source format, keeps them for playback lookups.
    musx::factory::PlaybackResourceInputs playbackInputs;
    finale_mus_reader::ReaderOptions options;
    std::unordered_set<MusResourceKind> singletons;
    for (const auto& resource : resources) {
        const auto bytes = unsignedBytes(resource.bytes);
        if (bytes.empty()) {
            throw std::invalid_argument("MUS resource is empty: " + resource.name);
        }
        if (resource.kind != MusResourceKind::PercussionMap && resource.kind != MusResourceKind::Soundmap
            && !singletons.insert(resource.kind).second) {
            throw std::invalid_argument("Duplicate singleton MUS resource.");
        }
        switch (resource.kind) {
        case MusResourceKind::SymbolFonts: options.macSymbolFonts = bytes; break;
        case MusResourceKind::PercussionMap:
            if (resource.name.empty()) {
                throw std::invalid_argument("A percussion mapping resource requires a filename.");
            }
            playbackInputs.deviceAnnotations.emplace_back(resource.name, bytes.data(), bytes.size());
            break;
        case MusResourceKind::PercussionConversionTable: options.percussionMapConversionTable = bytes; break;
        case MusResourceKind::Soundmap: playbackInputs.soundmaps.emplace_back(resource.name, bytes.data(), bytes.size()); break;
        case MusResourceKind::InstrumentCatalog: playbackInputs.instrumentCatalogs.emplace_back(resource.name, bytes.data(), bytes.size()); break;
        default: throw std::invalid_argument("Unknown MUS resource kind.");
        }
    }
    auto playbackResources = musx::factory::PlaybackResourcesFactory::create<MusxReader>(playbackInputs);
    options.playbackResources = playbackResources;
    auto reader = finale_mus_reader::Reader::create<MusxReader>(options);
    return std::shared_ptr<const MusResources>(new MusResources(std::make_unique<Impl>(std::move(playbackResources), std::move(reader))));
}

struct MusResourcesAccess
{
    static const finale_mus_reader::Reader& reader(const MusResources& resources) { return resources.m_impl->reader; }
    static const musx::dom::PlaybackResourcesPtr& playbackResources(const MusResources& resources) { return resources.m_impl->playbackResources; }
};

musx::dom::PlaybackResourcesPtr playbackResources(const DenigmaContext& context)
{
    return context.musResources ? MusResourcesAccess::playbackResources(*context.musResources) : nullptr;
}

musx::dom::DocumentPtr readMusDocument(std::span<const std::byte> bytes, const DenigmaContext& context, musx::dom::PartVoicingPolicy policy)
{
    const auto resources = [&] {
        if (context.musResources) {
            return context.musResources;
        }
        static const auto defaults = MusResources::create();
        return defaults;
    }();
    finale_mus_reader::ImportOptions options;
    options.sourcePath = context.inputFilePath;
    options.partVoicingPolicy = policy;
    MusxLoggerScope logger(makeMusxLogCallback(context));
    auto document = MusResourcesAccess::reader(*resources).read(unsignedBytes(bytes), options);
    if (!document) {
        throw std::runtime_error("Unable to import legacy Finale score.");
    }
    return document;
}

InputFormat resolveInputFormat(std::span<const std::byte> bytes, const std::filesystem::path& sourceName)
{
    const auto recognized = recognizeMus(bytes);
    if (recognized == finale_mus_reader::MusRecognition::Candidate) {
        return InputFormat::Mus;
    }
    if (recognized == finale_mus_reader::MusRecognition::Damaged) {
        throw std::invalid_argument("Damaged legacy Finale binary.");
    }
    const auto key = inputFormatKey(sourceName);
    if (key == MUSX_EXTENSION) {
        return InputFormat::Musx;
    }
    if (key == ENIGMAXML_EXTENSION) {
        return InputFormat::EnigmaXml;
    }
    if (key == ENIGMAXML_ZIP_EXTENSION) {
        return InputFormat::ZippedEnigmaXml;
    }
    if (recognized == finale_mus_reader::MusRecognition::FinaleNonScore) {
        throw std::invalid_argument("Finale input is not a supported legacy score.");
    }
    throw std::invalid_argument("Unsupported input format: " + utils::utf8ToString(key));
}

Buffer readInputBytes(const std::filesystem::path& path)
{
    std::ifstream input;
    input.exceptions(std::ios::failbit | std::ios::badbit);
    input.open(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size) > (std::numeric_limits<std::size_t>::max)()) {
        throw std::length_error("Input file is too large.");
    }
    Buffer bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    return bytes;
}

CommandInputData prepareMusInput(std::span<const std::byte> bytes)
{
    if (recognizeMus(bytes) != finale_mus_reader::MusRecognition::Candidate) {
        throw std::invalid_argument("Input is not a supported legacy Finale score.");
    }
    return {copyInputBytes(bytes), std::nullopt, {}, FormatId::Mus, InputFormat::Mus};
}

CommandInputData prepareInput(std::span<const std::byte> bytes, InputFormat format, const DenigmaContext& context)
{
    auto prepared = [&]() -> CommandInputData {
        switch (format) {
        case InputFormat::Mus: return prepareMusInput(bytes);
        case InputFormat::Musx: {
            BufferRandomAccessReader reader(bytes);
            return formats::enigmaxml::detail::extractMusxInputData(reader, context);
        }
        case InputFormat::ZippedEnigmaXml: {
            BufferRandomAccessReader reader(bytes);
            const auto xml = utils::readSoleFileWithExtension(reader, ENIGMAXML_EXTENSION, context);
            return {Buffer(xml.begin(), xml.end()), std::nullopt, {}};
        }
        case InputFormat::EnigmaXml: return {copyInputBytes(bytes), std::nullopt, {}};
        }
        throw std::invalid_argument("Unsupported input format.");
    }();
    prepared.originalFormat = format;
    return prepared;
}

CommandInputData prepareInput(const std::filesystem::path& path, const DenigmaContext& context)
{
    const auto buffer = readInputBytes(path);
    const auto bytes = std::as_bytes(std::span(buffer));
    return prepareInput(bytes, resolveInputFormat(bytes, path), context);
}

void loadMusResources(DenigmaContext& context)
{
    if (context.musResourcePaths.empty()) {
        return;
    }
    std::vector<Buffer> buffers;
    buffers.reserve(context.musResourcePaths.size());
    std::vector<MusResource> resources;
    for (const auto& [kind, path] : context.musResourcePaths) {
        buffers.push_back(readInputBytes(path));
        resources.push_back({kind, utils::pathToString(path.filename()), std::as_bytes(std::span(buffers.back()))});
    }
    MusxLoggerScope logger(makeMusxLogCallback(context));
    context.musResources = MusResources::create(resources);
}

} // namespace denigma
