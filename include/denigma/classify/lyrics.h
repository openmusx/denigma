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

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace lyric {

/// @enum WordExtensionKind
/// @brief How Finale records a lyric word extension. See
/// #musx::dom::options::LyricOptions::useSmartWordExtensions.
enum class WordExtensionKind {
    None, ///< The syllable has no word extension.
    Smart, ///< A word-extension smart shape gives the extension an endpoint entry.
    Legacy ///< The assignment stores only a drawn length, so the extension has no known endpoint.
};

} // namespace lyric

/// @struct LyricWordExtension
/// @brief Result returned by lyric word-extension classification.
struct LyricWordExtension
{
    lyric::WordExtensionKind kind{lyric::WordExtensionKind::None};
    /// The entry the extension ends on, which may be the syllable's own entry. Null for a legacy
    /// extension and for a smart extension whose shape was not found.
    musx::dom::EntryInfoPtr endEntry;

    /// Returns true when the syllable has a word extension.
    explicit operator bool() const noexcept { return kind != lyric::WordExtensionKind::None; }
};

/// @brief Classifies the word extension of one lyric syllable assignment.
/// @param assignment The syllable assignment to classify.
/// @return The classification, with #LyricWordExtension::kind set to
/// #lyric::WordExtensionKind::None when the syllable has no word extension.
LyricWordExtension classifyLyricWordExtension(const musx::dom::MusxInstance<musx::dom::details::LyricAssign>& assignment);

} // namespace classify
} // namespace denigma
