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

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace denigma {
namespace gap_report {

namespace {

using namespace classify;
using namespace classify::smartshape;

// Reporting names of the classifier and musxdom enums this serializer writes.

std::string_view shapeTypeName(musx::dom::others::SmartShape::ShapeType type)
{
    using Enum = musx::dom::others::SmartShape::ShapeType;
    switch (type) {
    case Enum::SlurDown: return "slur-down";
    case Enum::SlurUp: return "slur-up";
    case Enum::Decrescendo: return "decrescendo";
    case Enum::Crescendo: return "crescendo";
    case Enum::OctaveDown: return "octave-down";
    case Enum::OctaveUp: return "octave-up";
    case Enum::DashLineUp: return "dash-line-up";
    case Enum::DashLineDown: return "dash-line-down";
    case Enum::DashSlurDown: return "dash-slur-down";
    case Enum::DashSlurUp: return "dash-slur-up";
    case Enum::DashLine: return "dash-line";
    case Enum::SolidLine: return "solid-line";
    case Enum::SolidLineDown: return "solid-line-down";
    case Enum::SolidLineUp: return "solid-line-up";
    case Enum::Trill: return "trill";
    case Enum::SlurAuto: return "slur-auto";
    case Enum::DashSlurAuto: return "dash-slur-auto";
    case Enum::TrillExtension: return "trill-extension";
    case Enum::SolidLineDownBoth: return "solid-line-down-both";
    case Enum::SolidLineUpBoth: return "solid-line-up-both";
    case Enum::TwoOctaveDown: return "two-octave-down";
    case Enum::TwoOctaveUp: return "two-octave-up";
    case Enum::DashLineDownBoth: return "dash-line-down-both";
    case Enum::DashLineUpBoth: return "dash-line-up-both";
    case Enum::Glissando: return "glissando";
    case Enum::TabSlide: return "tab-slide";
    case Enum::BendHat: return "bend-hat";
    case Enum::BendCurve: return "bend-curve";
    case Enum::CustomLine: return "custom-line";
    case Enum::SolidLineUpLeft: return "solid-line-up-left";
    case Enum::SolidLineDownLeft: return "solid-line-down-left";
    case Enum::DashLineUpLeft: return "dash-line-up-left";
    case Enum::DashLineDownLeft: return "dash-line-down-left";
    case Enum::SolidLineUpDown: return "solid-line-up-down";
    case Enum::SolidLineDownUp: return "solid-line-down-up";
    case Enum::DashLineUpDown: return "dash-line-up-down";
    case Enum::DashLineDownUp: return "dash-line-down-up";
    case Enum::Hyphen: return "hyphen";
    case Enum::WordExtension: return "word-extension";
    case Enum::DashContourSlurDown: return "dash-contour-slur-down";
    case Enum::DashContourSlurUp: return "dash-contour-slur-up";
    case Enum::DashContourSlurAuto: return "dash-contour-slur-auto";
    }
    return "unknown";
}

std::string_view lineStyleName(GeneralLine::LineStyle value)
{
    using Enum = GeneralLine::LineStyle;
    switch (value) {
    case Enum::Char: return "char";
    case Enum::Solid: return "solid";
    case Enum::Dashed: return "dashed";
    }
    return "unknown";
}

std::string_view lineCapTypeName(LineCap::Type value)
{
    using Enum = LineCap::Type;
    switch (value) {
    case Enum::None: return "none";
    case Enum::Hook: return "hook";
    case Enum::ArrowheadPreset: return "arrowhead-preset";
    case Enum::ArrowheadCustom: return "arrowhead-custom";
    }
    return "unknown";
}

std::string_view arrowheadPresetName(musx::dom::ArrowheadPreset value)
{
    using Enum = musx::dom::ArrowheadPreset;
    switch (value) {
    case Enum::SmallFilled: return "small-filled";
    case Enum::SmallOutline: return "small-outline";
    case Enum::SmallCurved: return "small-curved";
    case Enum::LargeCurved: return "large-curved";
    case Enum::MediumCurved: return "medium-curved";
    }
    return "unknown";
}

std::string_view knownShapeDefTypeName(musx::dom::KnownShapeDefType value)
{
    using Enum = musx::dom::KnownShapeDefType;
    switch (value) {
    case Enum::Unrecognized: return "unrecognized";
    case Enum::Blank: return "blank";
    case Enum::TenutoMark: return "tenuto-mark";
    case Enum::SlurTieCurveRight: return "slur-tie-curve-right";
    case Enum::SlurTieCurveLeft: return "slur-tie-curve-left";
    case Enum::PedalArrowheadDown: return "pedal-arrowhead-down";
    case Enum::PedalArrowheadUp: return "pedal-arrowhead-up";
    case Enum::PedalArrowheadShortUpDownLongUp: return "pedal-arrowhead-short-up-down-long-up";
    case Enum::PedalArrowheadLongUpDownShortUp: return "pedal-arrowhead-long-up-down-short-up";
    case Enum::VerticalLineRightHooks: return "vertical-line-right-hooks";
    case Enum::SnapPizzicatoAbove: return "snap-pizzicato-above";
    case Enum::SnapPizzicatoBelow: return "snap-pizzicato-below";
    case Enum::BuzzPizzicato: return "buzz-pizzicato";
    case Enum::FingernailPizzCurveUp: return "fingernail-pizzicato-curve-up";
    case Enum::FingernailPizzCurveDown: return "fingernail-pizzicato-curve-down";
    }
    return "unknown";
}

std::string_view pedalCapTypeName(KeyboardPedal::CapType value)
{
    using Enum = KeyboardPedal::CapType;
    switch (value) {
    case Enum::None: return "none";
    case Enum::Hook: return "hook";
    case Enum::PedalDown: return "pedal-down";
    case Enum::PedalUp: return "pedal-up";
    case Enum::PedalChange: return "pedal-change";
    }
    return "unknown";
}

std::string_view contourName(musx::dom::CurveContourDirection value)
{
    using Enum = musx::dom::CurveContourDirection;
    switch (value) {
    case Enum::Unspecified: return "unspecified";
    case Enum::Down: return "down";
    case Enum::Up: return "up";
    }
    return "unknown";
}

std::string_view pseudoTieTypeName(PseudoTie::Type value)
{
    using Enum = PseudoTie::Type;
    switch (value) {
    case Enum::LaissezVibrer: return "laissez-vibrer";
    case Enum::TieEnd: return "tie-end";
    }
    return "unknown";
}

json lineCapJson(const LineCap& cap, ArrowheadTable& arrowheads)
{
    json result{{"type", lineCapTypeName(cap.type)}};
    switch (cap.type) {
    case LineCap::Type::None: break;
    case LineCap::Type::Hook: result["hookLength"] = cap.hookLength; break;
    case LineCap::Type::ArrowheadPreset:
        if (cap.preset) {
            result["preset"] = arrowheadPresetName(*cap.preset);
        }
        break;
    case LineCap::Type::ArrowheadCustom: result["knownType"] = knownShapeDefTypeName(cap.customArrowheadType); break;
    }
    if (auto reference = arrowheads.reference(cap); !reference.empty()) {
        result["arrowhead"] = std::move(reference);
    }
    return result;
}

/// @brief Adds a text position when the line has text there. Resolved at serialization, so the
/// source document must still be alive; see design-decisions.md.
void addText(json& target, std::string_view key, const musx::util::EnigmaParsingContext& text)
{
    if (text) {
        target[std::string(key)] = formattedTextJson(classify::classifyFormattedText(text));
    }
}

json generalLineJson(const GeneralLine& line, ArrowheadTable& arrowheads)
{
    json result{
        {"lineStyle", lineStyleName(line.lineStyle)},
        {"lineVisible", line.lineVisible},
    };
    switch (line.lineStyle) {
    case GeneralLine::LineStyle::Solid: result["lineWidth"] = line.lineWidth; break;
    case GeneralLine::LineStyle::Dashed:
        result["lineWidth"] = line.lineWidth;
        result["dashOn"] = line.dashOn;
        result["dashOff"] = line.dashOff;
        break;
    case GeneralLine::LineStyle::Char: {
        json lineChar{{"codePoint", static_cast<std::uint32_t>(line.lineChar)}};
        if (line.lineCharGlyphName) {
            lineChar["glyph"] = *line.lineCharGlyphName;
        }
        if (line.lineCharFont) {
            lineChar["font"] = fontJson(line.lineCharFont->resolve());
        }
        result["lineChar"] = std::move(lineChar);
        break;
    }
    }
    result["horizontal"] = line.horizontal;
    result["startCap"] = lineCapJson(line.startCap, arrowheads);
    result["endCap"] = lineCapJson(line.endCap, arrowheads);
    addText(result, "startText", line.startText);
    addText(result, "continuationText", line.continuationText);
    addText(result, "endText", line.endText);
    addText(result, "centerFullText", line.centerFullText);
    addText(result, "centerAbbrText", line.centerAbbrText);
    return result;
}

json keyboardPedalTextJson(const KeyboardPedalClassification& text)
{
    return {
        {"type", keyboardPedalTypeName(text.type)},
        {"fromGlyph", text.fromGlyph},
    };
}

json keyboardPedalJson(const KeyboardPedal& pedal, ArrowheadTable& arrowheads)
{
    json result{{"line", generalLineJson(pedal.line, arrowheads)}};
    if (pedal.startText) {
        result["startText"] = keyboardPedalTextJson(*pedal.startText);
    }
    if (pedal.continuationText) {
        result["continuationText"] = keyboardPedalTextJson(*pedal.continuationText);
    }
    if (pedal.endText) {
        result["endText"] = keyboardPedalTextJson(*pedal.endText);
    }
    result["startCap"] = pedalCapTypeName(pedal.startCap);
    result["endCap"] = pedalCapTypeName(pedal.endCap);
    return result;
}

/// @brief Names the classified value and writes its fields under a key of the same name.
void appendValue(json& result, const SmartShapeValue& value, ArrowheadTable& arrowheads)
{
    std::visit(
        [&](const auto& payload) {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, std::monostate>) {
                result["kind"] = "unclassified";
            } else if constexpr (std::is_same_v<Payload, Ottava>) {
                result["kind"] = "ottava";
                json ottava{
                    {"octaveShift", payload.octaveShift},
                    {"hasVisualProxy", payload.hasVisualProxy},
                    {"isSemanticCarrier", payload.calcIsSemanticCarrier()},
                };
                if (payload.line) {
                    ottava["line"] = generalLineJson(*payload.line, arrowheads);
                }
                result["ottava"] = std::move(ottava);
            } else if constexpr (std::is_same_v<Payload, Crescendo>) {
                result["kind"] = "crescendo";
            } else if constexpr (std::is_same_v<Payload, Decrescendo>) {
                result["kind"] = "decrescendo";
            } else if constexpr (std::is_same_v<Payload, Slur>) {
                result["kind"] = "slur";
                result["slur"] = {{"contour", contourName(payload.contour)}};
            } else if constexpr (std::is_same_v<Payload, ArpeggiatedTie>) {
                result["kind"] = "arpeggiated-tie";
                result["arpeggiatedTie"] = {{"contour", contourName(payload.contour)}};
            } else if constexpr (std::is_same_v<Payload, PseudoTie>) {
                result["kind"] = "pseudo-tie";
                result["pseudoTie"] = {
                    {"type", pseudoTieTypeName(payload.type)},
                    {"contour", contourName(payload.contour)},
                };
            } else if constexpr (std::is_same_v<Payload, NonArpeggio>) {
                result["kind"] = "non-arpeggio";
            } else if constexpr (std::is_same_v<Payload, KeyboardPedal>) {
                result["kind"] = "keyboard-pedal";
                result["keyboardPedal"] = keyboardPedalJson(payload, arrowheads);
            } else if constexpr (std::is_same_v<Payload, TrillLine>) {
                result["kind"] = "trill-line";
                json trill{{"includesTrSymbol", payload.includesTrSymbol}};
                if (payload.line) {
                    trill["line"] = generalLineJson(*payload.line, arrowheads);
                }
                result["trillLine"] = std::move(trill);
            } else if constexpr (std::is_same_v<Payload, VibratoLine>) {
                result["kind"] = "vibrato-line";
                result["vibratoLine"] = {{"line", generalLineJson(payload.line, arrowheads)}};
            } else if constexpr (std::is_same_v<Payload, Glissando>) {
                result["kind"] = "glissando";
                result["glissando"] = {{"line", generalLineJson(payload.line, arrowheads)}};
            } else if constexpr (std::is_same_v<Payload, GeneralLine>) {
                result["kind"] = "general-line";
                result["generalLine"] = generalLineJson(payload, arrowheads);
            }
        },
        value);
}

} // namespace

std::string ArrowheadTable::reference(const classify::smartshape::LineCap& cap)
{
    using musx::util::SvgConvert;
    // Coordinates in staff spaces at 100% staff size; see design-decisions.md.
    constexpr double scaling = 1.0 / musx::dom::EVPU_PER_SPACE;
    std::string key;
    std::function<std::string()> render;
    switch (cap.type) {
    case LineCap::Type::ArrowheadPreset:
        if (!cap.preset) {
            return {};
        }
        key = "preset-" + std::string(arrowheadPresetName(*cap.preset));
        render = [preset = *cap.preset]() { return SvgConvert::presetArrowheadAsSvg(preset, scaling, SvgConvert::SvgUnit::None); };
        break;
    case LineCap::Type::ArrowheadCustom:
        if (!cap.customArrowhead) {
            return {};
        }
        key = "custom-" + std::to_string(cap.customArrowhead->getCmper());
        render = [&]() { return SvgConvert::toSvg(*cap.customArrowhead, scaling, SvgConvert::SvgUnit::None, m_glyphMetrics); };
        break;
    default: return {};
    }
    auto found = m_svgByReference.find(key);
    if (found == m_svgByReference.end()) {
        // An arrowhead that renders nothing is remembered too, so it is not rendered again.
        found = m_svgByReference.emplace(key, render()).first;
    }
    return found->second.empty() ? std::string() : key;
}

json ArrowheadTable::toJson() const
{
    json result = json::object();
    for (const auto& [key, svg] : m_svgByReference) {
        if (svg.empty()) {
            continue;
        }
        result[key] = {
            {"unit", "staff-space"},
            {"origin", "line-end"},
            {"svg", svg},
        };
    }
    return result;
}

json smartShapeJson(const classify::SmartShapeClassification& smartShape, ArrowheadTable& arrowheads)
{
    json result{{"shapeType", shapeTypeName(smartShape.shapeType)}};
    appendValue(result, smartShape.value, arrowheads);
    return result;
}

} // namespace gap_report
} // namespace denigma
