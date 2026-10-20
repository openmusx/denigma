// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#pragma once

#include "core/denigma.h"
#include "denigma/mus_resources.h"
#include "finale_mus_reader/reader.h"

namespace denigma {

finale_mus_reader::MusRecognition recognizeMus(std::span<const std::byte> bytes) noexcept;
InputFormat resolveInputFormat(std::span<const std::byte> bytes, const std::filesystem::path& sourceName);
Buffer readInputBytes(const std::filesystem::path& path);
CommandInputData prepareInput(std::span<const std::byte> bytes, InputFormat format, const DenigmaContext& context);
CommandInputData prepareInput(const std::filesystem::path& path, const DenigmaContext& context);
CommandInputData prepareMusInput(std::span<const std::byte> bytes);
musx::dom::DocumentPtr readMusDocument(std::span<const std::byte> bytes, const DenigmaContext& context, musx::dom::PartVoicingPolicy policy);
void loadMusResources(DenigmaContext& context);

template <typename Reader>
musx::dom::DocumentPtr createMusxDocument(const CommandInputData& inputData, const DenigmaContext& denigmaContext,
    musx::dom::PartVoicingPolicy partVoicingPolicy = musx::dom::PartVoicingPolicy::Ignore)
{
    if (inputData.sourceFormat == FormatId::Mus) {
        return readMusDocument(std::as_bytes(std::span(inputData.primaryBuffer)), denigmaContext, partVoicingPolicy);
    }
    musx::factory::DocumentFactory::CreateOptions::EmbeddedGraphicFiles embeddedGraphicFiles;
    embeddedGraphicFiles.reserve(inputData.embeddedGraphics.size());
    for (const auto& graphic : inputData.embeddedGraphics) {
        musx::factory::DocumentFactory::CreateOptions::EmbeddedGraphicFile file;
        file.filename = graphic.filename;
        file.bytes.assign(graphic.blob.begin(), graphic.blob.end());
        embeddedGraphicFiles.emplace_back(std::move(file));
    }

    musx::factory::DocumentFactory::CreateOptions createOptions(
        denigmaContext.inputFilePath, inputData.notationMetadata.value_or(Buffer{}), std::move(embeddedGraphicFiles), partVoicingPolicy);

    return musx::factory::DocumentFactory::create<Reader>(inputData.primaryBuffer, std::move(createOptions));
}

} // namespace denigma
