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
#pragma once

#include <vector>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace lyric {

/// @enum WordExtensionKind
/// @brief How Finale records a lyric word extension. See
/// #musx::dom::options::LyricOptions::useSmartWordExtensions.
enum class WordExtensionKind {
    None, ///< The syllable has no word extension.
    Smart, ///< A word-extension smart shape records the endpoint, which is inferred when the shape is missing.
    Legacy, ///< The assignment stores only a drawn length, and the endpoint is inferred from the music.
    Underscores ///< The assignment stores no extension (wext), but its syllable ends in underscores, which
                ///< musx strips from the text (musx::dom::LyricsSyllableInfo::strippedUnderscores). The endpoint
                ///< is inferred from the music.
};

} // namespace lyric

/// @struct LyricWordExtension
/// @brief Result returned by lyric word-extension classification.
///
/// Whether the syllable has an extension comes from the source data (#kind). Where it ends
/// (#endEntry) is resolved for every kind except lyric::WordExtensionKind::None, but may be missing
/// even when the syllable has an extension, so exporters write the extension from #kind and its
/// endpoint from #endEntry.
struct LyricWordExtension
{
    /// How the assignment stores its extension, or lyric::WordExtensionKind::None when it has none.
    lyric::WordExtensionKind kind{lyric::WordExtensionKind::None};
    /// The entry the extension ends on, which may be the syllable's own entry. Null when no endpoint
    /// could be found, such as for a syllable followed by a hyphen. See
    /// musx::dom::details::LyricAssign::calcWordExtensionEndpoint, and for lyric::WordExtensionKind::Underscores
    /// musx::dom::details::LyricAssign::calcInferredWordExtensionEndpoint.
    musx::dom::EntryInfoPtr endEntry;

    /// Returns true when the assignment has a word extension, whether or not it has an #endEntry.
    explicit operator bool() const noexcept { return kind != lyric::WordExtensionKind::None; }
};

/// @brief Classifies the word extension of one lyric syllable assignment.
///
/// A stored extension (musx::dom::details::LyricAssign::wext) classifies as smart or legacy even when the
/// syllable also had underscores stripped; only without one do stripped underscores classify as
/// lyric::WordExtensionKind::Underscores. The end entry is looked for whenever the kind is not None.
/// @param assignment The syllable assignment to classify.
/// @return The classification, with #LyricWordExtension::kind set to
/// #lyric::WordExtensionKind::None and no #LyricWordExtension::endEntry when the assignment has no word
/// extension (musx::dom::details::LyricAssign::wext is zero) and its syllable had no underscores stripped.
LyricWordExtension classifyLyricWordExtension(const musx::dom::MusxInstance<musx::dom::details::LyricAssign>& assignment);

/// @struct LyricLineAssignment
/// @brief The one syllable assignment to export for a lyric line on an entry.
/// @tparam T musx::dom::details::LyricAssignVerse, LyricAssignChorus or LyricAssignSection.
template <typename T>
struct LyricLineAssignment
{
    /// The assignment to export for this line.
    musx::dom::MusxInstance<T> assignment;
    /// The 0-based index of #assignment's syllable in its lyric text.
    size_t syllableIndex{};
    /// True when the syllable has no visible text, such as a lone non-breaking space typed to skip a
    /// note. A blank line exports only its word extension: no text, syllabic or display number.
    bool blank{};
    /// The word extension of #assignment. See #classifyLyricWordExtension.
    LyricWordExtension wordExtension;
};

/// @struct EntryLyricAssignments
/// @brief One entry's assignments of one lyric block type, resolved to at most one per line.
/// @tparam T musx::dom::details::LyricAssignVerse, LyricAssignChorus or LyricAssignSection.
template <typename T>
struct EntryLyricAssignments
{
    /// One assignment per exported line, in the order each line first appears on the entry.
    std::vector<LyricLineAssignment<T>> lines;
    /// Assignments whose lyric text is missing or whose syllable number is out of range.
    std::vector<musx::dom::MusxInstance<T>> invalid;
    /// Assignments not exported because another assignment of the same lyric number on the entry was
    /// chosen. An entry normally has at most one assignment per lyric line, so these indicate a damaged file.
    std::vector<musx::dom::MusxInstance<T>> discarded;
};

/// @brief Resolves one entry's assignments of one lyric block type to at most one assignment per line.
///
/// Assignments are grouped by lyric number in their stored order. Within a group, the first assignment
/// whose syllable has visible text is chosen, or the first blank one when none has; the others are
/// reported in EntryLyricAssignments::discarded. A syllable is blank when it contains nothing but elision
/// markers (see musx::dom::LyricsSyllableInfo::calcElisionRuns). A line whose chosen syllable is blank
/// and has no word extension draws nothing, so it is left out of EntryLyricAssignments::lines.
/// @tparam T musx::dom::details::LyricAssignVerse, LyricAssignChorus or LyricAssignSection.
/// @param assignments One entry's assignments of type @p T.
/// @return The resolved lines and any invalid assignments.
template <typename T>
EntryLyricAssignments<T> classifyLyricAssignments(const musx::dom::MusxInstanceList<T>& assignments);

} // namespace classify
} // namespace denigma
