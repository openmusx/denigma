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
#include "gap_report_json.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace denigma {
namespace gap_report {

namespace {

std::string_view cellShapeName(musx::dom::details::FretboardDiagram::Shape shape)
{
    using Shape = musx::dom::details::FretboardDiagram::Shape;
    switch (shape) {
    case Shape::None: return "none";
    case Shape::Closed: return "closed";
    case Shape::Open: return "open";
    case Shape::Muted: return "muted";
    case Shape::Custom: return "custom";
    }
    return "none";
}

json diagramJson(const musx::util::FretboardDisplayData& diagram)
{
    auto cells = json::array();
    for (const auto& cell : diagram.cells) {
        json item{
            {"string", cell.string},
            {"fret", cell.fret},
            {"shape", cellShapeName(cell.shape)},
        };
        if (cell.finger != 0) {
            item["finger"] = cell.finger;
        }
        cells.push_back(std::move(item));
    }
    auto barres = json::array();
    for (const auto& barre : diagram.barres) {
        barres.push_back({
            {"fret", barre.fret},
            {"startString", barre.startString},
            {"endString", barre.endString},
        });
    }
    json result{
        {"kind", "diagram"},
        {"strings", diagram.stringCount},
        {"frets", diagram.fretCount},
        {"fretboardNumber", diagram.fretboardNumber},
        {"showFretboardNumber", diagram.showFretboardNumber},
    };
    if (diagram.fretNumberText) {
        result["fretNumberText"] = *diagram.fretNumberText;
    }
    result["cells"] = std::move(cells);
    result["barres"] = std::move(barres);
    return result;
}

json fretboardJson(const classify::FretboardClassification& fretboard)
{
    return std::visit(
        [](const auto& value) -> json {
            using Fretboard = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Fretboard, classify::FontFretboard>) {
                return {{"kind", "font"}};
            } else {
                return diagramJson(value);
            }
        },
        fretboard);
}

} // namespace

std::string FretboardTable::reference(const classify::FretboardClassification& fretboard)
{
    auto entry = fretboardJson(fretboard);
    auto [it, inserted] = m_keyByFretboard.emplace(entry.dump(), std::string());
    if (inserted) {
        it->second = std::to_string(m_keyByFretboard.size());
        m_fretboards[it->second] = std::move(entry);
    }
    return it->second;
}

json FretboardTable::toJson() const
{
    return m_fretboards;
}

} // namespace gap_report
} // namespace denigma
