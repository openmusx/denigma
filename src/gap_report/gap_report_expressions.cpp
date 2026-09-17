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
#include "gap_report_json.h"

#include <type_traits>
#include <utility>

namespace denigma {
namespace gap_report {

namespace {

using namespace classify;
using namespace classify::articulation;
using namespace classify::dynamics;
using namespace classify::expression;

// Reporting names of the classifier enums this serializer writes.

std::string_view expressionTypeName(ExpressionType type)
{
    switch (type) {
    case ExpressionType::GenericText: return "generic-text";
    case ExpressionType::Dynamic: return "dynamic";
    case ExpressionType::Fermata: return "fermata";
    case ExpressionType::BreathMark: return "breath-mark";
    case ExpressionType::StringMute: return "string-mute";
    case ExpressionType::AccordionRegistration: return "accordion-registration";
    case ExpressionType::HarpDiagram: return "harp-diagram";
    case ExpressionType::KeyboardPedal: return "keyboard-pedal";
    case ExpressionType::PseudoTie: return "pseudo-tie";
    case ExpressionType::NonArpeggio: return "non-arpeggio";
    case ExpressionType::TempoMark: return "tempo-mark";
    case ExpressionType::MetronomeMark: return "metronome-mark";
    case ExpressionType::TempoAlteration: return "tempo-alteration";
    case ExpressionType::TechniqueText: return "technique-text";
    case ExpressionType::RehearsalMark: return "rehearsal-mark";
    case ExpressionType::MultimeasureRestNumber: return "multimeasure-rest-number";
    case ExpressionType::MeasureRepeatCount: return "measure-repeat-count";
    case ExpressionType::Error: return "error";
    case ExpressionType::Suppress: return "suppress";
    }
    return "unknown";
}

std::string_view classificationBasisName(ClassificationBasis basis)
{
    switch (basis) {
    case ClassificationBasis::FinaleCategory: return "finale-category";
    case ClassificationBasis::Heuristic: return "heuristic";
    case ClassificationBasis::FinaleCategoryConfirmed: return "finale-category-confirmed";
    case ClassificationBasis::FinaleCategoryCorrected: return "finale-category-corrected";
    case ClassificationBasis::FallbackToGenericText: return "fallback-to-generic-text";
    }
    return "unknown";
}

std::string_view expressionScopeName(ExpressionScope scope)
{
    switch (scope) {
    case ExpressionScope::Unassigned: return "unassigned";
    case ExpressionScope::Staff: return "staff";
    case ExpressionScope::TopStaff: return "top-staff";
    case ExpressionScope::BottomStaff: return "bottom-staff";
    }
    return "unknown";
}

std::string_view techniqueTextTypeName(TechniqueText::Type type)
{
    using Type = TechniqueText::Type;
    switch (type) {
    case Type::None: return "none";
    case Type::Arco: return "arco";
    case Type::Pizzicato: return "pizzicato";
    case Type::ColLegno: return "col-legno";
    case Type::ColLegnoBattuto: return "col-legno-battuto";
    case Type::ColLegnoTratto: return "col-legno-tratto";
    case Type::SulPonticello: return "sul-ponticello";
    case Type::SulTasto: return "sul-tasto";
    case Type::Flautando: return "flautando";
    case Type::Ordinario: return "ordinario";
    case Type::Mute: return "mute";
    case Type::StraightMute: return "straight-mute";
    case Type::CupMute: return "cup-mute";
    case Type::HarmonMute: return "harmon-mute";
    case Type::PlungerMute: return "plunger-mute";
    case Type::BucketMute: return "bucket-mute";
    case Type::SolotoneMute: return "solotone-mute";
    case Type::StopMute: return "stop-mute";
    case Type::Stopped: return "stopped";
    case Type::Open: return "open";
    case Type::Other: return "other";
    }
    return "unknown";
}

std::string_view harpPedalPositionName(HarpDiagram::PedalPosition position)
{
    using PedalPosition = HarpDiagram::PedalPosition;
    switch (position) {
    case PedalPosition::Flat: return "flat";
    case PedalPosition::Natural: return "natural";
    case PedalPosition::Sharp: return "sharp";
    }
    return "unknown";
}

std::string_view dynamicName(Dynamic value)
{
    switch (value) {
    case Dynamic::None: return "none";
    case Dynamic::Other: return "other";
    case Dynamic::pppppp: return "pppppp";
    case Dynamic::ppppp: return "ppppp";
    case Dynamic::pppp: return "pppp";
    case Dynamic::ppp: return "ppp";
    case Dynamic::pp: return "pp";
    case Dynamic::p: return "p";
    case Dynamic::mp: return "mp";
    case Dynamic::mf: return "mf";
    case Dynamic::f: return "f";
    case Dynamic::ff: return "ff";
    case Dynamic::fff: return "fff";
    case Dynamic::ffff: return "ffff";
    case Dynamic::fffff: return "fffff";
    case Dynamic::ffffff: return "ffffff";
    case Dynamic::fp: return "fp";
    case Dynamic::ffp: return "ffp";
    case Dynamic::fz: return "fz";
    case Dynamic::ffz: return "ffz";
    case Dynamic::pf: return "pf";
    case Dynamic::sf: return "sf";
    case Dynamic::sfp: return "sfp";
    case Dynamic::sfpp: return "sfpp";
    case Dynamic::sfz: return "sfz";
    case Dynamic::sffz: return "sffz";
    case Dynamic::sfzp: return "sfzp";
    case Dynamic::rf: return "rf";
    case Dynamic::rfz: return "rfz";
    case Dynamic::n: return "n";
    }
    return "unknown";
}

std::string_view dynamicLevelName(Level value)
{
    switch (value) {
    case Level::None: return "none";
    case Level::Other: return "other";
    case Level::pppppp: return "pppppp";
    case Level::ppppp: return "ppppp";
    case Level::pppp: return "pppp";
    case Level::ppp: return "ppp";
    case Level::pp: return "pp";
    case Level::p: return "p";
    case Level::mp: return "mp";
    case Level::mf: return "mf";
    case Level::f: return "f";
    case Level::ff: return "ff";
    case Level::fff: return "fff";
    case Level::ffff: return "ffff";
    case Level::fffff: return "fffff";
    case Level::ffffff: return "ffffff";
    case Level::n: return "n";
    }
    return "unknown";
}

std::string_view dynamicReinforcementName(Reinforcement value)
{
    switch (value) {
    case Reinforcement::None: return "none";
    case Reinforcement::Sforzando: return "sforzando";
    case Reinforcement::Rinforzando: return "rinforzando";
    }
    return "unknown";
}

std::string_view stringMuteTypeName(StringMute::Type value)
{
    using Enum = StringMute::Type;
    switch (value) {
    case Enum::On: return "on";
    case Enum::Off: return "off";
    }
    return "unknown";
}

std::string_view accordionHandName(AccordionRegistration::Hand value)
{
    using Enum = AccordionRegistration::Hand;
    switch (value) {
    case Enum::Other: return "other";
    case Enum::Right: return "right";
    case Enum::Left: return "left";
    }
    return "unknown";
}

std::string_view accordionRankCountName(AccordionRegistration::RankCount value)
{
    using Enum = AccordionRegistration::RankCount;
    switch (value) {
    case Enum::Other: return "other";
    case Enum::Two: return "two";
    case Enum::Three: return "three";
    case Enum::Four: return "four";
    }
    return "unknown";
}

std::string_view accordionInstrumentTypeName(AccordionRegistration::InstrumentType value)
{
    using Enum = AccordionRegistration::InstrumentType;
    switch (value) {
    case Enum::Other: return "other";
    case Enum::Piccolo: return "piccolo";
    case Enum::Clarinet: return "clarinet";
    case Enum::Bassoon: return "bassoon";
    case Enum::Oboe: return "oboe";
    case Enum::Violin: return "violin";
    case Enum::ImitationMusette: return "imitation-musette";
    case Enum::AuthenticMusette: return "authentic-musette";
    case Enum::Organ: return "organ";
    case Enum::Harmonium: return "harmonium";
    case Enum::Bandoneon: return "bandoneon";
    case Enum::Accordion: return "accordion";
    }
    return "unknown";
}

std::string_view accordionDotPositionName(AccordionRegistration::DotPosition value)
{
    using Enum = AccordionRegistration::DotPosition;
    switch (value) {
    case Enum::Other: return "other";
    case Enum::Top: return "top";
    case Enum::UpperMiddle: return "upper-middle";
    case Enum::Middle: return "middle";
    case Enum::LowerMiddle: return "lower-middle";
    case Enum::Bottom: return "bottom";
    }
    return "unknown";
}

std::string_view keyboardPedalTypeName(classify::keyboardpedal::Type value)
{
    using Enum = classify::keyboardpedal::Type;
    switch (value) {
    case Enum::PedalOne: return "pedal-one";
    case Enum::PedalTwo: return "pedal-two";
    case Enum::PedalThree: return "pedal-three";
    case Enum::PedalUp: return "pedal-up";
    case Enum::HalfPedal: return "half-pedal";
    case Enum::PedalUpNotch: return "pedal-up-notch";
    case Enum::PedalUpSpecial: return "pedal-up-special";
    case Enum::HookStart: return "hook-start";
    case Enum::HookEnd: return "hook-end";
    case Enum::Hyphen: return "hyphen";
    case Enum::PedalChange: return "pedal-change";
    }
    return "unknown";
}

std::string_view glyphStylePlacementName(GlyphStyle::Placement value)
{
    using Enum = GlyphStyle::Placement;
    switch (value) {
    case Enum::Automatic: return "automatic";
    case Enum::Above: return "above";
    case Enum::Below: return "below";
    }
    return "unknown";
}

json tempoInfoJson(const classify::expression::TempoInfo& tempo)
{
    return {
        {"text", tempo.text},
        {"beatsPerMinute", tempo.beatsPerMinute},
        {"beatUnitEdu", tempo.beatUnitEdu},
    };
}

json dynamicJson(const classify::dynamics::Mark& mark)
{
    return {
        {"dynamic", dynamicName(mark.dynamic)},
        {"composition",
            {
                {"reinforcement", dynamicReinforcementName(mark.composition.reinforcement)},
                {"level", dynamicLevelName(mark.composition.level)},
                {"forzato", mark.composition.forzato},
                {"subsequent", dynamicLevelName(mark.composition.subsequent)},
            }},
        {"glyphs", mark.glyphs},
    };
}

json metronomeJson(const classify::expression::MetronomeMark& mark)
{
    return {
        {"tempo", tempoInfoJson(mark.tempo)},
        {"noteTypeEdu", static_cast<int>(mark.noteType)},
        {"noteGlyphName", mark.noteGlyphName},
        {"augmentationDots", mark.augmentationDots},
        {"displayedBeatsPerMinute", mark.displayedBeatsPerMinute},
    };
}

json accordionJson(const classify::articulation::AccordionRegistration& registration)
{
    auto dots = json::array();
    for (const auto& dot : registration.dots) {
        dots.push_back(accordionDotPositionName(dot.position));
    }
    return {
        {"hand", accordionHandName(registration.hand)},
        {"rankCount", accordionRankCountName(registration.rankCount)},
        {"instrumentType", accordionInstrumentTypeName(registration.instrumentType)},
        {"dots", std::move(dots)},
        {"glyphs", registration.glyphNames},
    };
}

json harpDiagramJson(const classify::expression::HarpDiagram& diagram)
{
    return {
        {"d", harpPedalPositionName(diagram.d)},
        {"c", harpPedalPositionName(diagram.c)},
        {"b", harpPedalPositionName(diagram.b)},
        {"e", harpPedalPositionName(diagram.e)},
        {"f", harpPedalPositionName(diagram.f)},
        {"g", harpPedalPositionName(diagram.g)},
        {"a", harpPedalPositionName(diagram.a)},
    };
}

/// @brief Writes the payload of the classification's active alternative under its own key.
/// An alternative no exporter reports as a gap writes nothing beyond the type.
void appendValue(json& result, const classify::ExpressionValue& value)
{
    namespace expression = classify::expression;
    std::visit(
        [&](const auto& payload) {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, classify::dynamics::Mark>) {
                result["dynamic"] = dynamicJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::articulation::StringMute>) {
                result["stringMute"] = {
                    {"type", stringMuteTypeName(payload.type)},
                    {"placement", glyphStylePlacementName(payload.glyphStyle.placement)},
                };
            } else if constexpr (std::is_same_v<Payload, classify::articulation::AccordionRegistration>) {
                result["accordionRegistration"] = accordionJson(payload);
            } else if constexpr (std::is_same_v<Payload, expression::HarpDiagram>) {
                result["harpDiagram"] = harpDiagramJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::keyboardpedal::Type>) {
                result["keyboardPedal"] = keyboardPedalTypeName(payload);
            } else if constexpr (std::is_same_v<Payload, expression::TempoText>) {
                result["tempo"] = tempoInfoJson(payload.tempo);
            } else if constexpr (std::is_same_v<Payload, expression::MetronomeMark>) {
                result["metronome"] = metronomeJson(payload);
            } else if constexpr (std::is_same_v<Payload, expression::TempoAlteration>) {
                result["tempoAlteration"] = tempoInfoJson(payload.tempo);
            } else if constexpr (std::is_same_v<Payload, expression::TechniqueText>) {
                result["technique"] = {
                    {"type", techniqueTextTypeName(payload.type)},
                    {"text", payload.text},
                };
            } else if constexpr (std::is_same_v<Payload, expression::RehearsalMark>) {
                result["rehearsalMark"] = {{"text", payload.text}};
            } else if constexpr (std::is_same_v<Payload, expression::MultimeasureRestNumber>) {
                result["multimeasureRestNumber"] = {{"number", payload.number}};
            } else if constexpr (std::is_same_v<Payload, expression::MeasureRepeatCount>) {
                result["measureRepeatCount"] = {{"count", payload.count}};
            } else if constexpr (std::is_same_v<Payload, expression::GenericText>) {
                result["genericText"] = {{"text", payload.text}};
            } else if constexpr (std::is_same_v<Payload, expression::Error>) {
                result["error"] = {{"message", payload.message}};
            }
        },
        value);
}

} // namespace

json expressionJson(const classify::ExpressionClassification& expression)
{
    json result{
        {"type", expressionTypeName(expression.type)},
        {"basis", classificationBasisName(expression.basis)},
        {"scope", expressionScopeName(expression.scope)},
    };
    // Resolved at serialization, so the source document must still be alive; see design-decisions.md.
    if (expression.enigmaCtx) {
        result["text"] = formattedTextJson(classify::classifyFormattedText(*expression.enigmaCtx));
    }
    appendValue(result, expression.value);
    return result;
}

} // namespace gap_report
} // namespace denigma
