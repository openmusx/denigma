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

#include <string>
#include <type_traits>

#include "musx/musx.h"

namespace denigma {
namespace core {

/// @brief Computes a stable id for an event (an entry, or the full-measure rest standing in for it).
/// Shared by every exporter that needs to point at an event, so the same musx entry always yields
/// the same id regardless of target format.
std::string calcEventId(const musx::dom::EntryInfoPtr& entryInfo);

/// @brief Computes a stable id for one note within an entry.
std::string calcNoteId(const musx::dom::NoteInfoPtr& noteInfo);

/// @brief Computes a stable id for a measure, independent of any part.
std::string calcGlobalMeasureId(musx::dom::Cmper cmperValue);

/// @brief Computes a stable id for one part's instance of a measure.
///
/// A bare #calcGlobalMeasureId value is not unique across parts sharing one document-wide id
/// namespace (MNX's `Object::id` and MusicXML's `xs:ID`-typed id attributes both require
/// document-wide uniqueness, and both formats can hold every part's measures in one document), so
/// a part-measure id is the measure id prefixed with its owning part's id.
std::string calcPartMeasureId(const std::string& partId, musx::dom::Cmper cmperValue);

/// @brief Computes a stable id for an object derived from one measure-expression assignment.
///
/// The id records the assignment's full Finale provenance: the measure, the expression definition
/// it assigns, and the assignment's inci within the measure, as `m<cmper>.textExp<def>.inci<n>` or
/// `m<cmper>.shapeExp<def>.inci<n>`. An assignment with no definition yields an empty string.
std::string calcExpressionId(const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment);

/// @brief Computes a stable id for an object derived from one Tempo Tool record (`tempoDef`).
std::string calcTempoDefId(musx::dom::Cmper measureCmper, musx::dom::Inci inci);

/// @brief Computes a stable id for a tuplet definition from its entry and incidence.
std::string calcTupletId(const musx::dom::EntryInfoPtr& firstEntryInfo, const musx::dom::MusxInstance<musx::dom::details::TupletDef>& tupletDef);

/// @brief Computes a stable id for an object derived from one lyric syllable assignment.
///
/// The id records the assignment's full Finale provenance: the event, the lyric block type and
/// number it assigns, and the assignment's inci on the entry, as
/// `ev<entry>.<verse|chorus|section><number>.inci<n>`.
/// @tparam T A subtype of musx::dom::details::LyricAssign.
template <typename T>
std::string calcLyricAssignId(const musx::dom::MusxInstance<T>& assignment, const musx::dom::EntryInfoPtr& entryInfo)
{
    static_assert(std::is_base_of_v<musx::dom::details::LyricAssign, T>, "T must be a subtype of LyricAssign");
    return calcEventId(entryInfo) + "." + std::string(T::TextType::XmlNodeName) + std::to_string(assignment->lyricNumber) + ".inci"
           + std::to_string(assignment->getInci().value_or(0));
}

} // namespace core
} // namespace denigma
