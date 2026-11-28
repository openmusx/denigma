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
#include "denigma/classify/lyrics.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "core/denigma.h"

namespace denigma {
namespace classify {

using namespace musx::dom;

namespace {

/// A syllable that is nothing but elision markers splits into runs that are all empty, so it draws no text.
bool isBlankSyllable(const LyricsSyllableInfo& syllable)
{
    const auto runs = syllable.calcElisionRuns();
    return std::all_of(runs.begin(), runs.end(), [](const LyricsSyllableInfo::ElisionRun& run) { return run.text().empty(); });
}

bool hasStrippedUnderscores(const details::LyricAssign& assignment)
{
    const auto lyricText = assignment.getLyricText();
    return lyricText && assignment.syllable > 0 && assignment.syllable <= lyricText->syllables.size()
           && lyricText->syllables[assignment.syllable - 1]->strippedUnderscores > 0;
}

} // namespace

LyricWordExtension classifyLyricWordExtension(const MusxInstance<details::LyricAssign>& assignment)
{
    LyricWordExtension result;
    ASSERT_IF (!assignment) {
        return result;
    }

    // A stored extension (wext) decides the kind even when the syllable also had underscores stripped.
    if (assignment->wext != 0) {
        const auto lyricOptions = assignment->getDocument()->getOptions()->get<options::LyricOptions>();
        result.kind = lyricOptions && lyricOptions->useSmartWordExtensions ? lyric::WordExtensionKind::Smart : lyric::WordExtensionKind::Legacy;
    } else if (hasStrippedUnderscores(*assignment)) {
        result.kind = lyric::WordExtensionKind::Underscores;
    }

    switch (result.kind) {
    case lyric::WordExtensionKind::None: break;
    case lyric::WordExtensionKind::Smart:
    case lyric::WordExtensionKind::Legacy: result.endEntry = assignment->calcWordExtensionEndpoint(); break;
    // Without wext there is no stored extension to consult, so the end is inferred from the music.
    case lyric::WordExtensionKind::Underscores: result.endEntry = assignment->calcInferredWordExtensionEndpoint(); break;
    }
    return result;
}

template <typename T>
EntryLyricAssignments<T> classifyLyricAssignments(const MusxInstanceList<T>& assignments)
{
    struct Candidate
    {
        MusxInstance<T> assignment;
        size_t syllableIndex{};
        bool blank{};
    };
    // Groups keep the order in which each lyric number first appears on the entry.
    std::vector<std::pair<Cmper, std::vector<Candidate>>> groups;

    EntryLyricAssignments<T> result;
    for (const auto& assignment : assignments) {
        const auto lyricText = assignment->getLyricText();
        if (!lyricText || assignment->syllable == 0 || assignment->syllable > lyricText->syllables.size()) {
            result.invalid.push_back(assignment);
            continue;
        }
        const size_t syllableIndex = size_t(assignment->syllable - 1); // Finale syllable numbers are 1-based.
        auto group = std::find_if(groups.begin(), groups.end(), [&](const auto& g) { return g.first == assignment->lyricNumber; });
        if (group == groups.end()) {
            group = groups.insert(groups.end(), {assignment->lyricNumber, {}});
        }
        group->second.push_back({assignment, syllableIndex, isBlankSyllable(*lyricText->syllables[syllableIndex])});
    }

    for (auto& [lyricNumber, candidates] : groups) {
        auto chosen = std::find_if(candidates.begin(), candidates.end(), [](const Candidate& candidate) { return !candidate.blank; });
        if (chosen == candidates.end()) {
            chosen = candidates.begin();
        }
        LyricLineAssignment<T> line;
        line.assignment = chosen->assignment;
        line.syllableIndex = chosen->syllableIndex;
        line.blank = chosen->blank;
        line.wordExtension = classifyLyricWordExtension(chosen->assignment);
        for (auto it = candidates.begin(); it != candidates.end(); ++it) {
            if (it != chosen) {
                result.discarded.push_back(it->assignment);
            }
        }
        if (line.blank && !line.wordExtension) {
            continue; // A blank syllable without an extension draws nothing.
        }
        result.lines.push_back(std::move(line));
    }
    return result;
}

template EntryLyricAssignments<details::LyricAssignVerse> classifyLyricAssignments(const MusxInstanceList<details::LyricAssignVerse>&);
template EntryLyricAssignments<details::LyricAssignChorus> classifyLyricAssignments(const MusxInstanceList<details::LyricAssignChorus>&);
template EntryLyricAssignments<details::LyricAssignSection> classifyLyricAssignments(const MusxInstanceList<details::LyricAssignSection>&);

} // namespace classify
} // namespace denigma
