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

#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "denigma/classify/chords.h"
#include "denigma/classify/expressions.h"
#include "denigma/classify/formatted_text.h"
#include "denigma/classify/noteheads.h"
#include "denigma/classify/smartshapes.h"
#include "denigma/classify/text_blocks.h"
#include "denigma/gap_report.h"
#include "nlohmann/json.hpp"

// Internal to the gap report library: one serializer per classification type, each in its own
// translation unit, assembled by gap_report.cpp.

namespace denigma {
namespace gap_report {

using json = nlohmann::ordered_json;

/// @class ArrowheadTable
/// @brief The arrowhead images a report embeds once each, keyed by a reference the caps that use them carry.
///
/// The key is minted here from the arrowhead's identity in the source and names nothing in the
/// target document; see design-decisions.md.
class ArrowheadTable
{
public:
    explicit ArrowheadTable(GlyphMetricsFn glyphMetrics)
        : m_glyphMetrics(std::move(glyphMetrics))
    {}

    /// @brief Returns the reference of the cap's arrowhead, adding the image on first use.
    /// @return An empty string when the cap has no arrowhead or the arrowhead renders nothing.
    std::string reference(const classify::smartshape::LineCap& cap);

    /// @brief The table as a JSON object, empty when no arrowhead was referenced.
    [[nodiscard]] json toJson() const;

private:
    GlyphMetricsFn m_glyphMetrics;
    std::map<std::string, std::string> m_svgByReference;
};

/// @class FontTable
/// @brief The fonts a report names, each written once and referenced by key wherever it is used.
///
/// Keys are `<n>`, numbered from 1 in order of first use, so they are stable for a given report and
/// name nothing in the target document.
class FontTable
{
public:
    /// @brief Returns the key of @p font, adding it on first use.
    std::string reference(const musx::dom::ResolvedFontInfo& font);

    /// @brief The table as a JSON object, empty when no font was referenced.
    [[nodiscard]] json toJson() const;

private:
    std::map<std::string, std::string> m_keyByFont;
    json m_fonts = json::object();
};

/// @class FretboardTable
/// @brief The fretboards a report names, each written once and referenced by key from the chords that show it.
///
/// Keys are `<n>`, numbered from 1 in order of first use, so they are stable for a given report and
/// name nothing in the target document. Fretboards that serialize identically share a key.
class FretboardTable
{
public:
    /// @brief Returns the key of @p fretboard, adding it on first use.
    std::string reference(const classify::FretboardClassification& fretboard);

    /// @brief The table as a JSON object, empty when no fretboard was referenced.
    [[nodiscard]] json toJson() const;

private:
    std::map<std::string, std::string> m_keyByFretboard;
    json m_fretboards = json::object();
};

/// @brief The report-level tables that payloads reference by key.
struct ReportTables
{
    ArrowheadTable arrowheads;
    FontTable fonts;
    FretboardTable fretboards;
};

json chordJson(const classify::ChordSymbolClassification& chord, FretboardTable& fretboards);
json noteheadJson(const classify::NoteheadClassification& notehead, FontTable& fonts);
json expressionJson(const classify::ExpressionClassification& expression, FontTable& fonts);
json formattedTextJson(const classify::FormattedText& text, FontTable& fonts);
json insertJson(const classify::text::Insert& insert);
json fontJson(const musx::dom::ResolvedFontInfo& font);
json smartShapeJson(const classify::SmartShapeClassification& smartShape, ReportTables& tables);
json textBlockFrameJson(const classify::TextBlockFrame& frame);
json measureTextJson(const classify::MeasureTextClassification& text);
json pageTextJson(const classify::PageTextClassification& text, FontTable& fonts);
json pageTextPositionJson(const musx::dom::others::PageTextAssign& assignment);

// Reporting names shared by more than one serializer.
std::string_view keyboardPedalTypeName(classify::keyboardpedal::Type value);

} // namespace gap_report
} // namespace denigma
