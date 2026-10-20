// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#pragma once

#include "denigma/conversion.h"

namespace denigma {

/// @brief Optional Finale resource kinds. Numeric values are also used by the WASM ABI.
enum class MusResourceKind {
    SymbolFonts = 0,
    PercussionMap = 1,
    PercussionConversionTable = 2,
    Soundmap = 3,
    InstrumentCatalog = 4
};

/// @brief A caller-owned resource buffer, consumed synchronously by MusResources::create.
struct MusResource
{
    MusResourceKind kind{};
    /// Filename used to match percussion maps; required for PercussionMap.
    std::string name;
    std::span<const std::byte> bytes;
};

/// @brief Immutable, reusable parsed resources for legacy MUS imports.
class MusResources
{
public:
    /// Parses resources once. Throws on invalid resources or duplicate singleton kinds.
    /// Percussion maps and soundmaps retain caller order. Input buffers may be released on return.
    [[nodiscard]] static std::shared_ptr<const MusResources> create(std::span<const MusResource> resources = {});
    ~MusResources();

private:
    struct Impl;
    explicit MusResources(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
    friend struct MusResourcesAccess;
};

} // namespace denigma
