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
#pragma once

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "denigma/classify/chords.h"
#include "denigma/classify/noteheads.h"

namespace denigma {
namespace classify {

/// @struct GapPosition
/// @brief Position within an anchored measure as a whole-note fraction.
struct GapPosition
{
    int numerator{};
    int denominator{1};
};

/// @struct GapAnchor
/// @brief Stable target object identity and optional location within that object.
struct GapAnchor
{
    std::string id;
    std::optional<int> staff;
    std::optional<GapPosition> position;
};

using GapPayload = std::variant<ChordSymbolClassification, NoteheadClassification>;

/// @struct Gap
/// @brief One classified feature omitted from a conversion target.
struct Gap
{
    GapAnchor anchor;
    GapPayload payload;
};

/// @class GapCollector
/// @brief Collects typed conversion gaps for inspection or later serialization.
///
/// Serialize or otherwise consume the collected gaps before releasing the parsed source document.
/// Some classification values may retain non-owning access to document-backed source objects.
class GapCollector
{
public:
    /// Adds one typed gap.
    template <typename Payload>
    void add(GapAnchor anchor, Payload payload)
    {
        m_gaps.push_back({std::move(anchor), GapPayload(std::move(payload))});
    }

    /// Returns collected gaps in source traversal order.
    [[nodiscard]] std::span<const Gap> gaps() const noexcept { return m_gaps; }

private:
    std::vector<Gap> m_gaps;
};

} // namespace classify
} // namespace denigma
