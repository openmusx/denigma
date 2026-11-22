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
#include "denigma/classify/articulations.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "classify.h"
#include "musx/util/Fraction.h"
#include "smufl_mapping.h"

namespace denigma {
namespace classify {

using namespace articulation;

namespace {

using ArticulationType = ArticulationMark::Type;
using TechniqueType = TechniqueMark::Type;
using HarmonMuteQualifier = HarmonMute::Qualifier;
using PluckedDampType = PluckedDamp::Type;
using StringMuteType = StringMute::Type;
using NoteSlideType = NoteSlide::Type;

struct AmbiguousMark
{
    enum class Shape {
        Unspecified,
        SmallCircle,
        Plus
    };

    Shape shape{};
    OtherMark::Category category{};
    GlyphStyle glyphStyle{};
    std::optional<std::string> glyphName;
};

using PrivateClassification = std::variant<ArticulationClassification, AmbiguousMark>;

static GlyphStyle glyphStyleFromGlyphName(const std::string_view glyphName)
{
    const auto endsWith = [&](const std::string_view suffix) {
        return glyphName.size() >= suffix.size() && glyphName.compare(glyphName.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (endsWith("Above") || endsWith("AboveLegacy")) {
        return {GlyphStyle::Placement::Above};
    }
    if (endsWith("Below") || endsWith("BelowLegacy")) {
        return {GlyphStyle::Placement::Below};
    }
    return {};
}

static GlyphStyle glyphStyleFromKnownShapeDefType(musx::dom::KnownShapeDefType shapeDefType)
{
    using ST = musx::dom::KnownShapeDefType;
    switch (shapeDefType) {
    case ST::SnapPizzicatoAbove: return {GlyphStyle::Placement::Above};
    case ST::SnapPizzicatoBelow: return {GlyphStyle::Placement::Below};
    default: break;
    }
    return {};
}

static GlyphStyle glyphStyleFromGlyphName(const std::optional<std::string>& glyphName)
{
    if (glyphName) {
        return glyphStyleFromGlyphName(std::string_view(glyphName.value()));
    }
    return {};
}

static void setGlyphMetadata(ArticulationClassification& result, std::optional<std::string> glyphName)
{
    result.glyphName = std::move(glyphName);
}

static ArticulationClassification makeArticulationMark(std::vector<ArticulationType> types, std::optional<std::string> glyphName)
{
    const auto glyphStyle = glyphStyleFromGlyphName(glyphName);
    std::vector<ArticulationMark> marks;
    marks.reserve(types.size());
    for (auto type : types) {
        marks.push_back({type, glyphStyle});
    }
    ArticulationClassification result;
    result.value = ArticulationMarks{std::move(marks)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeTechniqueMark(TechniqueType type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = TechniqueMark{type, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeHarmonMute(HarmonMuteQualifier qualifier, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = HarmonMute{qualifier, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makePluckedDamp(PluckedDampType type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = PluckedDamp{type, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeStringMute(StringMuteType type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = StringMute{type, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static std::optional<AccordionRegistration> classifyAccordionRegistrationGlyph(std::string_view glyphName)
{
    using Hand = AccordionRegistration::Hand;
    using RankCount = AccordionRegistration::RankCount;
    using InstrumentType = AccordionRegistration::InstrumentType;
    using DotPosition = AccordionRegistration::DotPosition;

    AccordionRegistration result;
    result.glyphNames.emplace_back(glyphName);

    const std::string_view name = glyphName;
    if (name.starts_with("accdnRH3Ranks") || name == "accdnCombRH3RanksEmpty") {
        result.hand = Hand::Right;
        result.rankCount = RankCount::Three;
    } else if (name.starts_with("accdnRH4Ranks") || name == "accdnCombRH4RanksEmpty") {
        result.hand = Hand::Right;
        result.rankCount = RankCount::Four;
    } else if (name.starts_with("accdnLH2Ranks") || name == "accdnCombLH2RanksEmpty") {
        result.hand = Hand::Left;
        result.rankCount = RankCount::Two;
    } else if (name.starts_with("accdnLH3Ranks") || name == "accdnCombLH3RanksEmptySquare") {
        result.hand = Hand::Left;
        result.rankCount = RankCount::Three;
    } else {
        return std::nullopt;
    }

    static const std::unordered_map<std::string_view, InstrumentType> instrumentTypes = {
        {"accdnRH3RanksPiccolo", InstrumentType::Piccolo},
        {"accdnRH3RanksClarinet", InstrumentType::Clarinet},
        {"accdnRH3RanksBassoon", InstrumentType::Bassoon},
        {"accdnRH3RanksOboe", InstrumentType::Oboe},
        {"accdnRH3RanksViolin", InstrumentType::Violin},
        {"accdnRH3RanksImitationMusette", InstrumentType::ImitationMusette},
        {"accdnRH3RanksAuthenticMusette", InstrumentType::AuthenticMusette},
        {"accdnRH3RanksOrgan", InstrumentType::Organ},
        {"accdnRH3RanksHarmonium", InstrumentType::Harmonium},
        {"accdnRH3RanksBandoneon", InstrumentType::Bandoneon},
        {"accdnRH3RanksAccordion", InstrumentType::Accordion},
    };
    if (const auto found = instrumentTypes.find(name); found != instrumentTypes.end()) {
        result.instrumentType = found->second;
    }

    static const std::unordered_map<std::string_view, std::vector<DotPosition>> dotPositions = {
        {"accdnRH3RanksPiccolo", {DotPosition::Top}},
        {"accdnRH3RanksClarinet", {DotPosition::Middle}},
        {"accdnRH3RanksUpperTremolo8", {DotPosition::UpperMiddle}},
        {"accdnRH3RanksLowerTremolo8", {DotPosition::LowerMiddle}},
        {"accdnRH3RanksBassoon", {DotPosition::Bottom}},
        {"accdnRH3RanksOboe", {DotPosition::Top, DotPosition::Middle}},
        {"accdnRH3RanksViolin", {DotPosition::Middle, DotPosition::UpperMiddle}},
        {"accdnRH3RanksImitationMusette", {DotPosition::Top, DotPosition::Middle, DotPosition::UpperMiddle}},
        {"accdnRH3RanksAuthenticMusette", {DotPosition::LowerMiddle, DotPosition::Middle, DotPosition::UpperMiddle}},
        {"accdnRH3RanksOrgan", {DotPosition::Top, DotPosition::Bottom}},
        {"accdnRH3RanksHarmonium", {DotPosition::Top, DotPosition::Middle, DotPosition::Bottom}},
        {"accdnRH3RanksBandoneon", {DotPosition::Middle, DotPosition::Bottom}},
        {"accdnRH3RanksAccordion", {DotPosition::Middle, DotPosition::UpperMiddle, DotPosition::Bottom}},
        {"accdnRH3RanksMaster", {DotPosition::Top, DotPosition::LowerMiddle, DotPosition::UpperMiddle, DotPosition::Bottom}},
        {"accdnRH3RanksTwoChoirs", {DotPosition::LowerMiddle, DotPosition::UpperMiddle}},
        {"accdnRH3RanksTremoloLower8ve", {DotPosition::LowerMiddle, DotPosition::UpperMiddle, DotPosition::Bottom}},
        {"accdnRH3RanksTremoloUpper8ve", {DotPosition::Top, DotPosition::LowerMiddle, DotPosition::UpperMiddle}},
        {"accdnRH3RanksDoubleTremoloLower8ve", {DotPosition::LowerMiddle, DotPosition::Middle, DotPosition::UpperMiddle, DotPosition::Bottom}},
        {"accdnRH3RanksDoubleTremoloUpper8ve", {DotPosition::Top, DotPosition::LowerMiddle, DotPosition::Middle, DotPosition::UpperMiddle}},
        {"accdnRH3RanksFullFactory",
            {DotPosition::Top, DotPosition::LowerMiddle, DotPosition::Middle, DotPosition::UpperMiddle, DotPosition::Bottom}},
        {"accdnLH2Ranks8Round", {DotPosition::Top}},
        {"accdnLH2Ranks16Round", {DotPosition::Bottom}},
        {"accdnLH2Ranks8Plus16Round", {DotPosition::Top, DotPosition::Bottom}},
        {"accdnLH2RanksMasterRound", {DotPosition::Top, DotPosition::Bottom}},
        {"accdnLH2RanksMasterPlus16Round", {DotPosition::Top, DotPosition::Bottom}},
        {"accdnLH2RanksFullMasterRound", {DotPosition::Top, DotPosition::Bottom}},
        {"accdnLH3Ranks8Square", {DotPosition::Middle}},
        {"accdnLH3Ranks2Square", {DotPosition::Top}},
        {"accdnLH3RanksDouble8Square", {DotPosition::Middle, DotPosition::Bottom}},
        {"accdnLH3Ranks2Plus8Square", {DotPosition::Top, DotPosition::Middle}},
        {"accdnLH3RanksTuttiSquare", {DotPosition::Top, DotPosition::Middle, DotPosition::Bottom}},
    };
    if (const auto found = dotPositions.find(name); found != dotPositions.end()) {
        for (const auto position : found->second) {
            result.dots.push_back({position});
        }
    }
    return result;
}

static std::optional<ArticulationClassification> makeAccordionRegistration(std::string glyphName)
{
    auto registration = classifyAccordionRegistrationGlyph(glyphName);
    if (!registration) {
        return std::nullopt;
    }
    ArticulationClassification result;
    result.value = std::move(*registration);
    result.glyphName = std::move(glyphName);
    return result;
}

static ArticulationClassification makeTremolo(Tremolo::Style style, int marks, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Tremolo{style, marks, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeFermata(Fermata::Shape shape, Fermata::Duration duration, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Fermata{shape, duration, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeBreathMark(BreathMark::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = BreathMark{type, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeCaesura(Caesura::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Caesura{type, glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeArpeggio(Arpeggio::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    // The span is entry-dependent, so classifyArticulation fills it in once an entry is known.
    result.value = Arpeggio{type, glyphStyleFromGlyphName(glyphName), std::nullopt};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeOrnament(
    Ornament::Type type, std::optional<std::string> glyphName, std::vector<Ornament::AccidentalMark> accidentals = {})
{
    ArticulationClassification result;
    result.value = Ornament{type, glyphStyleFromGlyphName(glyphName), std::move(accidentals)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeVerticalEntryBracket()
{
    ArticulationClassification result;
    result.value = VerticalEntryBracket{};
    return result;
}

static ArticulationClassification makeNoteSlide(NoteSlideType type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = NoteSlide{type, musx::dom::NoteInfoPtr(), glyphStyleFromGlyphName(glyphName)};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makePseudoTie(PseudoTie::Type type, musx::dom::CurveContourDirection contour)
{
    ArticulationClassification result;
    result.value = PseudoTie{type, contour};
    return result;
}

static AmbiguousMark makeAmbiguousMark(AmbiguousMark::Shape shape, OtherMark::Category category, std::optional<std::string> glyphName)
{
    return {shape, category, glyphStyleFromGlyphName(glyphName), std::move(glyphName)};
}

static ArticulationClassification makeTechniqueMark(
    TechniqueType type, const GlyphStyle& glyphStyle, std::optional<std::string> glyphName = std::nullopt)
{
    ArticulationClassification result;
    result.value = TechniqueMark{type, glyphStyle};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeOtherMark(
    OtherMark::Category category, const GlyphStyle& glyphStyle, std::optional<std::string> glyphName = std::nullopt)
{
    ArticulationClassification result;
    result.value = OtherMark{category, glyphStyle};
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static musx::dom::InstrumentFamily instrumentFamilyForEntry(const musx::dom::EntryInfoPtr& entryInfo)
{
    if (const auto staff = entryInfo.createCurrentStaff()) {
        return musx::dom::instrumentFamilyFromUuid(staff->instUuid);
    }
    return musx::dom::InstrumentFamily::Unspecified;
}

static ArticulationClassification resolveAmbiguousMark(AmbiguousMark mark, const musx::dom::EntryInfoPtr& entryInfo)
{
    ArticulationClassification result;
    const auto instrumentFamily = instrumentFamilyForEntry(entryInfo);
    switch (mark.shape) {
    case AmbiguousMark::Shape::SmallCircle:
        if (instrumentFamily == musx::dom::InstrumentFamily::Brass) {
            result = makeTechniqueMark(TechniqueType::BrassOpen, mark.glyphStyle, std::move(mark.glyphName));
        } else if (instrumentFamily == musx::dom::InstrumentFamily::Strings || instrumentFamily == musx::dom::InstrumentFamily::PluckedStrings) {
            result = makeTechniqueMark(TechniqueType::StringHarmonic, mark.glyphStyle, std::move(mark.glyphName));
        } else {
            result = makeOtherMark(mark.category, mark.glyphStyle, std::move(mark.glyphName));
        }
        break;
    case AmbiguousMark::Shape::Plus:
        if (instrumentFamily == musx::dom::InstrumentFamily::Brass) {
            result = makeTechniqueMark(TechniqueType::BrassStopped, mark.glyphStyle, std::move(mark.glyphName));
        } else if (instrumentFamily == musx::dom::InstrumentFamily::Strings || instrumentFamily == musx::dom::InstrumentFamily::PluckedStrings) {
            // Harp may need to be treated separately if '+' has a distinct meaning in harp notation.
            result = makeTechniqueMark(TechniqueType::LeftHandPizzicato, mark.glyphStyle, std::move(mark.glyphName));
        } else {
            result = makeOtherMark(mark.category, mark.glyphStyle, std::move(mark.glyphName));
        }
        break;
    default: result = makeOtherMark(mark.category, mark.glyphStyle, std::move(mark.glyphName)); break;
    }
    return result;
}

static std::vector<Ornament::AccidentalMark> accidentalAbove(Ornament::Accidental accidental)
{
    return {{accidental, musx::dom::VerticalPlacement::Above}};
}

static std::vector<Ornament::AccidentalMark> accidentalBelow(Ornament::Accidental accidental)
{
    return {{accidental, musx::dom::VerticalPlacement::Below}};
}

static std::vector<Ornament::AccidentalMark> accidentalsAboveBelow(Ornament::Accidental above, Ornament::Accidental below)
{
    return {
        {above, musx::dom::VerticalPlacement::Above},
        {below, musx::dom::VerticalPlacement::Below},
    };
}

static ArticulationClassification classifyUnicodeSymbol(char32_t symbol)
{
    switch (symbol) {
    case 0x1D167:
    case 0x1D16A: return makeTremolo(Tremolo::Style::Measured, 1, std::nullopt);
    case 0x1D168:
    case 0x1D16B: return makeTremolo(Tremolo::Style::Measured, 2, std::nullopt);
    case 0x1D169:
    case 0x1D16C: return makeTremolo(Tremolo::Style::Measured, 3, std::nullopt);
    case 0x1D17B: return makeArticulationMark({ArticulationType::Accent}, std::nullopt);
    case 0x1D17C: return makeArticulationMark({ArticulationType::Staccato}, std::nullopt);
    case 0x1D17D: return makeArticulationMark({ArticulationType::Tenuto}, std::nullopt);
    case 0x1D17E: return makeArticulationMark({ArticulationType::Staccatissimo}, std::nullopt);
    case 0x1D17F: return makeArticulationMark({ArticulationType::StrongAccent}, std::nullopt);
    default: return {};
    }
}

static ArticulationClassification classifyShape(const musx::dom::MusxInstance<musx::dom::others::ArticulationDef>& def, musx::dom::Cmper shapeId)
{
    if (!def || !shapeId) {
        return {};
    }
    ArticulationClassification result{};
    if (auto shape = def->getDocument()->getOthers()->get<musx::dom::others::ShapeDef>(def->getRequestedPartId(), shapeId)) {
        using ST = musx::dom::KnownShapeDefType;
        const auto recognizedShapeType = shape->recognize();
        switch (recognizedShapeType) {
        case ST::TenutoMark: result = makeArticulationMark({ArticulationType::Tenuto}, std::nullopt); break;
        case ST::VerticalLineRightHooks: result = makeVerticalEntryBracket(); break;
        case ST::SnapPizzicatoAbove: result = makeTechniqueMark(TechniqueType::SnapPizzicato, std::nullopt); break;
        case ST::SnapPizzicatoBelow: result = makeTechniqueMark(TechniqueType::SnapPizzicato, std::nullopt); break;
        case ST::BuzzPizzicato: result = makeTechniqueMark(TechniqueType::BuzzPizzicato, std::nullopt); break;
        case ST::FingernailPizzCurveUp:
        case ST::FingernailPizzCurveDown: result = makeTechniqueMark(TechniqueType::Fingernails, std::nullopt); break;
        default: break;
        }
        std::visit(
            [&](auto& value) {
                using T = std::decay_t<decltype(value)>;
                constexpr bool hasGlyphStyle = !std::is_same_v<T, std::monostate> && !std::is_same_v<T, PseudoTie>
                                               && !std::is_same_v<T, AccordionRegistration> && !std::is_same_v<T, BracketEnclosure>;

                if constexpr (std::is_same_v<T, ArticulationMarks>) {
                    for (auto& mark : value.marks) {
                        mark.glyphStyle = glyphStyleFromKnownShapeDefType(recognizedShapeType);
                    }
                } else if constexpr (hasGlyphStyle) {
                    value.glyphStyle = glyphStyleFromKnownShapeDefType(recognizedShapeType);
                }
            },
            result.value);
    }
    return result;
}

static PrivateClassification classifyGlyphName(std::string glyphName)
{
    using GlyphClassifier = PrivateClassification (*)(std::string);
    static const std::unordered_map<std::string_view, GlyphClassifier> glyphClassifiers = {
        // Arpeggio
        {"arpeggiato", [](std::string glyphName) -> PrivateClassification { return makeArpeggio(Arpeggio::Type::Normal, std::move(glyphName)); }},
        {"arpeggiatoDown", [](std::string glyphName) -> PrivateClassification { return makeArpeggio(Arpeggio::Type::Down, std::move(glyphName)); }},
        {"arpeggiatoUp", [](std::string glyphName) -> PrivateClassification { return makeArpeggio(Arpeggio::Type::Up, std::move(glyphName)); }},
        {"arpeggioVerticalSegment",
            [](std::string glyphName) -> PrivateClassification { return makeArpeggio(Arpeggio::Type::VerticalSegment, std::move(glyphName)); }},

        // ArticulationMarks
        {"articAccentAbove",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Accent}, std::move(glyphName)); }},
        {"articAccentAboveLegacy",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Accent}, std::move(glyphName)); }},
        {"articAccentBelow",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Accent}, std::move(glyphName)); }},
        {"articAccentStaccatoAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Staccato}, std::move(glyphName));
            }},
        {"articAccentStaccatoAboveLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Staccato}, std::move(glyphName));
            }},
        {"articAccentStaccatoBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Staccato}, std::move(glyphName));
            }},
        {"articAccentStaccatoBelowLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Staccato}, std::move(glyphName));
            }},
        {"articMarcatoAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::StrongAccent}, std::move(glyphName));
            }},
        {"articMarcatoBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::StrongAccent}, std::move(glyphName));
            }},
        {"articMarcatoStaccatoAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::StrongAccent}, std::move(glyphName));
            }},
        {"articMarcatoStaccatoAboveLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::StrongAccent}, std::move(glyphName));
            }},
        {"articMarcatoStaccatoBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::StrongAccent}, std::move(glyphName));
            }},
        {"articMarcatoStaccatoBelowLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::StrongAccent}, std::move(glyphName));
            }},
        {"articMarcatoTenutoAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::StrongAccent, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articMarcatoTenutoBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::StrongAccent, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articStaccatissimoAbove",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Spiccato}, std::move(glyphName)); }},
        {"articStaccatissimoBelow",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Spiccato}, std::move(glyphName)); }},
        {"articStaccatissimoStrokeAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccatissimo}, std::move(glyphName));
            }},
        {"articStaccatissimoStrokeBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccatissimo}, std::move(glyphName));
            }},
        {"articStaccatissimoWedgeAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccatissimo}, std::move(glyphName));
            }},
        {"articStaccatissimoWedgeBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccatissimo}, std::move(glyphName));
            }},
        {"articStaccatoAbove",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Staccato}, std::move(glyphName)); }},
        {"articStaccatoBelow",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Staccato}, std::move(glyphName)); }},
        {"articStressAbove",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Stress}, std::move(glyphName)); }},
        {"articStressBelow",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Stress}, std::move(glyphName)); }},
        {"articTenutoAbove",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Tenuto}, std::move(glyphName)); }},
        {"articTenutoAccentAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articTenutoAccentAboveLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articTenutoAccentBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articTenutoAccentBelowLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Accent, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articTenutoBelow",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Tenuto}, std::move(glyphName)); }},
        {"articTenutoStaccatoAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articTenutoStaccatoAboveLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articTenutoStaccatoBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articTenutoStaccatoBelowLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeArticulationMark({ArticulationType::Staccato, ArticulationType::Tenuto}, std::move(glyphName));
            }},
        {"articUnstressAbove",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Unstress}, std::move(glyphName)); }},
        {"articUnstressBelow",
            [](std::string glyphName) -> PrivateClassification { return makeArticulationMark({ArticulationType::Unstress}, std::move(glyphName)); }},
        {"brassBend",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassBend, std::move(glyphName)); }},
        {"brassDoitLong", [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Doit, std::move(glyphName)); }},
        {"brassDoitMedium", [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Doit, std::move(glyphName)); }},
        {"brassDoitShort", [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Doit, std::move(glyphName)); }},
        {"brassFallLipLong",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallLipMedium",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallLipShort",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughLong",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughMedium",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughShort",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallSmoothLong",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallSmoothMedium",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallSmoothShort",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughShortMedDecline",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughShortSlightDecline",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughVeryShortFastDecline",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughVeryShortMedDecline",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallRoughVeryShortSlightDecilne",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallSlight",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFallSmoothVeryLong",
            [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Falloff, std::move(glyphName)); }},
        {"brassFlip",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassFlip, std::move(glyphName)); }},
        {"brassMuteHalfClosed",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassHalfMuted, std::move(glyphName)); }},
        {"brassHarmonMuteClosed",
            [](std::string glyphName) -> PrivateClassification { return makeHarmonMute(HarmonMuteQualifier::Closed, std::move(glyphName)); }},
        {"brassHarmonMuteStemHalfLeft",
            [](std::string glyphName) -> PrivateClassification { return makeHarmonMute(HarmonMuteQualifier::HalfLeft, std::move(glyphName)); }},
        {"brassHarmonMuteStemHalfRight",
            [](std::string glyphName) -> PrivateClassification { return makeHarmonMute(HarmonMuteQualifier::HalfRight, std::move(glyphName)); }},
        {"brassHarmonMuteStemOpen",
            [](std::string glyphName) -> PrivateClassification { return makeHarmonMute(HarmonMuteQualifier::Open, std::move(glyphName)); }},
        {"brassLiftLong",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftMedium",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftShort",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftSmoothLong",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftSmoothMedium",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftSmoothShort",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftShortMedIncline",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftShortSlightIncline",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftSlight",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftSmoothVeryLong",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftVeryShortMedIncline",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassLiftVeryShortSlightIncline",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"brassPlop", [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Plop, std::move(glyphName)); }},
        {"brassScoop", [](std::string glyphName) -> PrivateClassification { return makeNoteSlide(NoteSlideType::Scoop, std::move(glyphName)); }},
        {"brassSmear",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassSmear, std::move(glyphName)); }},
        {"brassVeryLiftShort",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BrassLift, std::move(glyphName)); }},
        {"pluckedBuzzPizzicato",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::BuzzPizzicato, std::move(glyphName)); }},
        {"pluckedDamp", [](std::string glyphName) -> PrivateClassification { return makePluckedDamp(PluckedDampType::Damp, std::move(glyphName)); }},
        {"pluckedDampAll",
            [](std::string glyphName) -> PrivateClassification { return makePluckedDamp(PluckedDampType::DampAll, std::move(glyphName)); }},
        {"pluckedFingernailFlick",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::Fingernails, std::move(glyphName)); }},
        {"pluckedSnapPizzicatoAbove",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::SnapPizzicato, std::move(glyphName)); }},
        {"pluckedSnapPizzicatoBelow",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::SnapPizzicato, std::move(glyphName)); }},
        {"pluckedSnapPizzicatoAboveGerman",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::SnapPizzicato, std::move(glyphName)); }},
        {"pluckedSnapPizzicatoBelowGerman",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::SnapPizzicato, std::move(glyphName)); }},
        {"pluckedWithFingernails",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::Fingernails, std::move(glyphName)); }},
        {"stringsDownBow",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::DownBow, std::move(glyphName)); }},
        {"stringsDownBowTurned",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::DownBow, std::move(glyphName)); }},
        {"stringsMuteOff", [](std::string glyphName) -> PrivateClassification { return makeStringMute(StringMuteType::Off, std::move(glyphName)); }},
        {"stringsMuteOn", [](std::string glyphName) -> PrivateClassification { return makeStringMute(StringMuteType::On, std::move(glyphName)); }},
        {"stringsUpBow",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::UpBow, std::move(glyphName)); }},
        {"stringsUpBowTurned",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::UpBow, std::move(glyphName)); }},

        // AmbiguousMark
        {"brassMuteClosed",
            [](std::string glyphName) -> PrivateClassification {
                return makeAmbiguousMark(AmbiguousMark::Shape::Plus, OtherMark::Category::PerformanceTechnique, std::move(glyphName));
            }},
        {"brassMuteOpen",
            [](std::string glyphName) -> PrivateClassification {
                return makeAmbiguousMark(AmbiguousMark::Shape::SmallCircle, OtherMark::Category::PerformanceTechnique, std::move(glyphName));
            }},
        {"stringsHarmonic",
            [](std::string glyphName) -> PrivateClassification {
                return makeAmbiguousMark(AmbiguousMark::Shape::SmallCircle, OtherMark::Category::PerformanceTechnique, std::move(glyphName));
            }},
        {"stringsThumbPosition",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::ThumbPosition, std::move(glyphName)); }},
        {"stringsThumbPositionTurned",
            [](std::string glyphName) -> PrivateClassification { return makeTechniqueMark(TechniqueType::ThumbPosition, std::move(glyphName)); }},

        // BreathMark
        {"breathMark",
            [](std::string glyphName) -> PrivateClassification { return makeBreathMark(BreathMark::Type::Unspecified, std::move(glyphName)); }},
        {"breathMarkComma",
            [](std::string glyphName) -> PrivateClassification { return makeBreathMark(BreathMark::Type::Comma, std::move(glyphName)); }},
        {"breathMarkCommaLegacy",
            [](std::string glyphName) -> PrivateClassification { return makeBreathMark(BreathMark::Type::Comma, std::move(glyphName)); }},
        {"breathMarkSalzedo",
            [](std::string glyphName) -> PrivateClassification { return makeBreathMark(BreathMark::Type::Salzedo, std::move(glyphName)); }},
        {"breathMarkTick",
            [](std::string glyphName) -> PrivateClassification { return makeBreathMark(BreathMark::Type::Tick, std::move(glyphName)); }},
        {"breathMarkUpbow",
            [](std::string glyphName) -> PrivateClassification { return makeBreathMark(BreathMark::Type::Upbow, std::move(glyphName)); }},

        // Caesura
        {"caesura", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Normal, std::move(glyphName)); }},
        {"caesuraCurved", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Curved, std::move(glyphName)); }},
        {"caesuraShort", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Short, std::move(glyphName)); }},
        {"caesuraSingleStroke",
            [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::SingleStroke, std::move(glyphName)); }},
        {"caesuraThick", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Thick, std::move(glyphName)); }},
        {"chantCaesura", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Chant, std::move(glyphName)); }},

        // Fermata
        {"curlewSign",
            [](std::string glyphName)
                -> PrivateClassification { return makeFermata(Fermata::Shape::Curlew, Fermata::Duration::Auto, std::move(glyphName)); }},
        {"fermataAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::Normal, Fermata::Duration::Auto, std::move(glyphName));
            }},
        {"fermataBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::Normal, Fermata::Duration::Auto, std::move(glyphName));
            }},
        {"fermataLongAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::Square, Fermata::Duration::Long, std::move(glyphName));
            }},
        {"fermataLongBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::Square, Fermata::Duration::Long, std::move(glyphName));
            }},
        {"fermataLongHenzeAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::DoubleDot, Fermata::Duration::Long, std::move(glyphName));
            }},
        {"fermataLongHenzeBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::DoubleDot, Fermata::Duration::Long, std::move(glyphName));
            }},
        {"fermataShortAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::Angled, Fermata::Duration::Short, std::move(glyphName));
            }},
        {"fermataShortBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::Angled, Fermata::Duration::Short, std::move(glyphName));
            }},
        {"fermataShortHenzeAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::HalfCurve, Fermata::Duration::Short, std::move(glyphName));
            }},
        {"fermataShortHenzeBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::HalfCurve, Fermata::Duration::Short, std::move(glyphName));
            }},
        {"fermataVeryLongAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::DoubleSquare, Fermata::Duration::VeryLong, std::move(glyphName));
            }},
        {"fermataVeryLongBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::DoubleSquare, Fermata::Duration::VeryLong, std::move(glyphName));
            }},
        {"fermataVeryShortAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::DoubleAngled, Fermata::Duration::VeryShort, std::move(glyphName));
            }},
        {"fermataVeryShortBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeFermata(Fermata::Shape::DoubleAngled, Fermata::Duration::VeryShort, std::move(glyphName));
            }},

        // Ornament
        {"ornamentHaydn", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Trill, std::move(glyphName)); }},
        {"ornamentMordent",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Mordent, std::move(glyphName)); }},
        {"ornamentMordentInverted",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::InvertedMordent, std::move(glyphName)); }},
        {"ornamentShake3", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Shake, std::move(glyphName)); }},
        {"ornamentShakeMuffat1",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Shake, std::move(glyphName)); }},
        {"ornamentShortTrill",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Trill, std::move(glyphName)); }},
        {"ornamentTremblement",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Trill, std::move(glyphName)); }},
        {"ornamentTremblementCouperin",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Trill, std::move(glyphName)); }},
        {"ornamentTrill", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Trill, std::move(glyphName)); }},
        {"ornamentTrillFlatAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
            }},
        {"ornamentTrillFlatAboveLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
            }},
        {"ornamentTrillNaturalAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
            }},
        {"ornamentTrillNaturalAboveLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
            }},
        {"ornamentTrillSharpAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
            }},
        {"ornamentTrillSharpAboveLegacy",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
            }},
        {"ornamentTurn", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Turn, std::move(glyphName)); }},
        {"ornamentTurnFlatAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
            }},
        {"ornamentTurnFlatAboveSharpBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(
                    Ornament::Type::Turn, std::move(glyphName), accidentalsAboveBelow(Ornament::Accidental::Flat, Ornament::Accidental::Sharp));
            }},
        {"ornamentTurnFlatBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Flat));
            }},
        {"ornamentTurnInverted",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::InvertedTurn, std::move(glyphName)); }},
        {"ornamentTurnNaturalAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
            }},
        {"ornamentTurnNaturalBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Natural));
            }},
        {"ornamentTurnSharpAbove",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
            }},
        {"ornamentTurnSharpAboveFlatBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(
                    Ornament::Type::Turn, std::move(glyphName), accidentalsAboveBelow(Ornament::Accidental::Sharp, Ornament::Accidental::Flat));
            }},
        {"ornamentTurnSharpBelow",
            [](std::string glyphName) -> PrivateClassification {
                return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Sharp));
            }},
        {"ornamentTurnSlash",
            [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::InvertedTurn, std::move(glyphName)); }},

        // Tremolo
        {"buzzRoll", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName)); }},
        {"pendereckiTremolo",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName)); }},
        {"stemPendereckiTremolo",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName)); }},
        {"stockhausenTremolo",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName)); }},
        {"tremolo1", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 1, std::move(glyphName)); }},
        {"tremolo1Alt",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 1, std::move(glyphName)); }},
        {"tremoloFingered1",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 1, std::move(glyphName)); }},
        {"tremolo2", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 2, std::move(glyphName)); }},
        {"tremolo2Alt",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 2, std::move(glyphName)); }},
        {"tremoloFingered2",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 2, std::move(glyphName)); }},
        {"tremolo3", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 3, std::move(glyphName)); }},
        {"tremolo3Alt",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 3, std::move(glyphName)); }},
        {"tremoloFingered3",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 3, std::move(glyphName)); }},
        {"tremolo4", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 4, std::move(glyphName)); }},
        {"tremolo4Legacy",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 4, std::move(glyphName)); }},
        {"tremoloFingered4",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 4, std::move(glyphName)); }},
        {"tremolo5", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 5, std::move(glyphName)); }},
        {"tremolo5Legacy",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 5, std::move(glyphName)); }},
        {"tremoloFingered5",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 5, std::move(glyphName)); }},
        {"unmeasuredTremolo",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName)); }},
        {"unmeasuredTremoloSimple",
            [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName)); }},
    };

    const std::string_view glyph = glyphName;
    if (const auto it = glyphClassifiers.find(glyph); it != glyphClassifiers.end()) {
        return it->second(std::move(glyphName));
    }
    if (auto accordionRegistration = makeAccordionRegistration(std::move(glyphName))) {
        return std::move(*accordionRegistration);
    }
    return {};
}

} // namespace

static PrivateClassification classifySymbol(const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    if (auto unicodeClassification = classifyUnicodeSymbol(symbol)) {
        return unicodeClassification;
    }
    if (fontInfo) {
        if (const auto* glyphName = smufl_mapping::getGlyphNameForFont(
                fontInfo->getName(), fontInfo->calcDrawnCharacter(symbol), fontInfo->calcIsSMuFL(), smufl_mapping::SmuflGlyphSource::Finale)) {
            return classifyGlyphName(std::string(*glyphName));
        }
    }
    return ArticulationClassification{};
}

ArticulationClassification classifyArticulationSymbol(const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    auto classification = classifySymbol(fontInfo, symbol);
    if (auto* ambiguousMark = std::get_if<AmbiguousMark>(&classification)) {
        return makeOtherMark(ambiguousMark->category, ambiguousMark->glyphStyle, std::move(ambiguousMark->glyphName));
    }
    return std::get<ArticulationClassification>(std::move(classification));
}

static std::optional<musx::util::ArpeggioSpanCandidate> calcArpeggioCandidate(
    const musx::dom::EntryInfoPtr& entryInfo, const musx::dom::MusxInstance<musx::dom::details::ArticulationAssign>& assignment, Arpeggio::Type type)
{
    std::optional<musx::util::ArpeggioSpanCandidate> result;
    const auto singleEntryCandidate = [&](musx::util::ArpeggioDirection direction, musx::util::ArpeggioArrow arrow) {
        auto& candidate = result.emplace();
        candidate.sourceEntry = entryInfo;
        candidate.topEntry = entryInfo;
        candidate.bottomEntry = entryInfo;
        candidate.direction = direction;
        candidate.arrow = arrow;
    };

    switch (type) {
    case Arpeggio::Type::VerticalSegment:
        // Only the segment glyph builds its span by searching for the entries the stack of segments
        // reaches. The whole-sign glyphs roll the chord they sit on and nothing else.
        return musx::util::calcArpeggioSpanForAssignment(
            entryInfo, assignment, {}, [](const musx::dom::details::ArticulationAssign::SelectedSymbolContext&) {
                // The selected symbol was already classified as a vertical arpeggio segment;
                // this callback bypasses musx's raw-SMuFL-codepoint fallback for legacy fonts.
                return true;
            });
    case Arpeggio::Type::Normal: singleEntryCandidate(musx::util::ArpeggioDirection::Auto, musx::util::ArpeggioArrow::None); break;
    case Arpeggio::Type::Up: singleEntryCandidate(musx::util::ArpeggioDirection::Up, musx::util::ArpeggioArrow::Up); break;
    case Arpeggio::Type::Down: singleEntryCandidate(musx::util::ArpeggioDirection::Down, musx::util::ArpeggioArrow::Down); break;
    }
    return result;
}

static PrivateClassification classifySelectedSymbolContext(const musx::dom::details::ArticulationAssign::SelectedSymbolContext& context)
{
    if (context.symbol.isShape) {
        return classifyShape(context.definition, context.symbol.shapeId);
    }
    return classifySymbol(context.symbol.font, context.symbol.character);
}

/// @brief Returns the entry's notes that are included in the part's voicing, from the note furthest from the stem
/// toward the stem. A rest has none.
static std::vector<musx::dom::NoteInfoPtr> calcNotesFromStemSide(const musx::dom::EntryInfoPtr& entryInfo)
{
    std::vector<musx::dom::NoteInfoPtr> result;
    const auto entry = entryInfo->getEntry();
    if (!entry->isNote) {
        return result;
    }
    for (size_t noteIndex = 0; noteIndex < entry->notes.size(); noteIndex++) {
        if (musx::dom::NoteInfoPtr note(entryInfo, noteIndex); note.calcIsIncludedInVoicing()) {
            result.push_back(note);
        }
    }
    // Notes run from lowest to highest, and an up-stem chord has its noteheads at the bottom.
    if (!entryInfo.calcUpStem()) {
        std::reverse(result.begin(), result.end());
    }
    return result;
}

namespace {

/// One side of a notehead enclosure, as drawn by a single articulation symbol.
struct EnclosureDelimiter
{
    /// The side of the enclosure the delimiter draws.
    enum class Side {
        Opening,
        Closing
    };
    Side side{};
    BracketEnclosure::Type type{};
    /// SMuFL glyph name of the symbol, or nullopt for an ASCII delimiter.
    std::optional<std::string> glyphName;
};

/// A positioned delimiter that may pair with another to enclose a note or a rest.
struct DelimiterCandidate
{
    size_t resultIndex{};
    EnclosureDelimiter delimiter;
    double staffPosition{};
    /// Horizontal offset in EVPU, from the entry's origin to the handle or, when #isCenteredOnNotehead, from the center
    /// of the entry's primary notehead column (or of the rest) to the symbol's visible center.
    double horizontalOffset{};
    bool isCenteredOnNotehead{};
    std::shared_ptr<musx::dom::FontInfo> font;
    bool isConsumed{};

    /// Returns the symbol's horizontal position from the left edge of what it encloses, given that target's width and
    /// its displacement from the entry's primary notehead column.
    double calcHorizontalPosition(double targetWidth, double targetDisplacement) const
    {
        return (isCenteredOnNotehead ? targetWidth / 2 + horizontalOffset : horizontalOffset) - targetDisplacement;
    }
};

} // namespace

/// @brief Recognizes a symbol that draws one side of a notehead enclosure.
/// @details A symbol that names a glyph is judged by the glyph alone, because a music font can draw an unrelated
/// glyph at an ASCII delimiter's code point. Only a symbol with no glyph name is read as an ASCII delimiter.
static std::optional<EnclosureDelimiter> classifyEnclosureDelimiter(const musx::dom::others::ArticulationDef::SelectedSymbol& symbol)
{
    using Type = BracketEnclosure::Type;
    using Side = EnclosureDelimiter::Side;
    if (symbol.isShape || !symbol.font) {
        return std::nullopt;
    }
    if (auto glyphName = detail::glyphNameForFont(symbol.font, symbol.character)) {
        static const std::unordered_map<std::string_view, std::pair<Side, Type>> glyphDelimiters = {
            {"noteheadParenthesisLeft", {Side::Opening, Type::Parentheses}},
            {"noteheadParenthesisRight", {Side::Closing, Type::Parentheses}},
            {"accidentalParensLeft", {Side::Opening, Type::Parentheses}},
            {"accidentalParensRight", {Side::Closing, Type::Parentheses}},
            {"accidentalBracketLeft", {Side::Opening, Type::Brackets}},
            {"accidentalBracketRight", {Side::Closing, Type::Brackets}},
        };
        const auto it = glyphDelimiters.find(*glyphName);
        if (it == glyphDelimiters.end()) {
            return std::nullopt;
        }
        return EnclosureDelimiter{it->second.first, it->second.second, std::move(glyphName)};
    }
    switch (symbol.font->calcDrawnCharacter(symbol.character)) {
    case U'(': return EnclosureDelimiter{Side::Opening, Type::Parentheses, std::nullopt};
    case U')': return EnclosureDelimiter{Side::Closing, Type::Parentheses, std::nullopt};
    case U'[': return EnclosureDelimiter{Side::Opening, Type::Brackets, std::nullopt};
    case U']': return EnclosureDelimiter{Side::Closing, Type::Brackets, std::nullopt};
    default: return std::nullopt;
    }
}

/// @brief Classifies one assignment. An assignment that classifies as std::monostate but draws a positioned
/// enclosure delimiter is added to @p delimiterCandidates under @p resultIndex.
static ArticulationClassification classifyAssignment(const musx::dom::MusxInstance<musx::dom::details::ArticulationAssign>& assignment,
    const musx::dom::EntryInfoPtr& entryInfo, size_t resultIndex, std::vector<DelimiterCandidate>& delimiterCandidates)
{
    ArticulationClassification result{};
    if (const auto laissezVibrer = assignment->calcIsPseudoTie(musx::utils::PseudoTieMode::LaissezVibrer, entryInfo)) {
        result = makePseudoTie(PseudoTie::Type::LaissezVibrer, laissezVibrer->shape->calcSlurContour());
    } else if (const auto tieEnd = assignment->calcIsPseudoTie(musx::utils::PseudoTieMode::TieEnd, entryInfo)) {
        result = makePseudoTie(PseudoTie::Type::TieEnd, tieEnd->shape->calcSlurContour());
    } else if (const auto symbolContext = assignment->calcSelectedSymbolContext(entryInfo)) {
        auto classification = classifySelectedSymbolContext(symbolContext.value());
        result = std::holds_alternative<AmbiguousMark>(classification)
                     ? resolveAmbiguousMark(std::get<AmbiguousMark>(std::move(classification)), entryInfo)
                     : std::get<ArticulationClassification>(std::move(classification));
        result.placement = symbolContext->placement;
        if (auto* arpeggio = std::get_if<Arpeggio>(&result.value)) {
            arpeggio->candidate = calcArpeggioCandidate(entryInfo, assignment, arpeggio->type);
        }
        if (!result) {
            if (auto delimiter = classifyEnclosureDelimiter(symbolContext->symbol)) {
                const auto staffPosition = assignment->calcHandleStaffPosition(entryInfo);
                const auto horizontalOffset = assignment->calcHorizontalOffset(entryInfo);
                // The notehead a centered delimiter encloses is not known until pairing.
                const bool isCenteredOnNotehead =
                    horizontalOffset && horizontalOffset->anchor == musx::dom::details::ArticulationAssign::HorizontalOffset::Anchor::NoteheadCenter;
                if (staffPosition && horizontalOffset) {
                    delimiterCandidates.push_back({resultIndex, std::move(*delimiter), *staffPosition, horizontalOffset->offset, isCenteredOnNotehead,
                        symbolContext->symbol.font});
                }
            }
        }
    }
    result.assignment = assignment;
    return result;
}

/// @brief Pairs the entry's delimiter candidates into bracket enclosures by the rules of #BracketEnclosure.
/// @details Each enclosure replaces the opening delimiter's result, and the closing delimiter's result is removed.
/// Of notes whose windows are equally close to the handle, as in a unison, the one further from the stem is chosen.
static void classifyBracketEnclosures(std::vector<ArticulationClassification>& results, std::vector<DelimiterCandidate>& candidates,
    const musx::dom::EntryInfoPtr& entryInfo, const std::vector<musx::dom::NoteInfoPtr>& notesFromStemSide)
{
    constexpr double WINDOW_BELOW_NOTE_SPACES = 3.0 / 4.0;
    constexpr double WINDOW_ABOVE_NOTE_SPACES = 1.0 / 4.0;
    // Rest glyphs vary in height and placement, so their window is looser than a note's.
    constexpr double WINDOW_BELOW_REST_SPACES = 1.0;
    constexpr double WINDOW_ABOVE_REST_SPACES = 1.0;
    constexpr double ENCLOSURE_VERTICAL_TOLERANCE_EVPU = 4.0;
    // Rule-of-thumb notehead widths, from a survey of SMuFL fonts.
    constexpr double NOTEHEAD_WIDTH_SPACES = 1.25;
    constexpr double WHOLE_NOTEHEAD_WIDTH_SPACES = 1.7;
    constexpr double DOUBLE_WHOLE_NOTEHEAD_WIDTH_SPACES = 2.25;
    // Rule-of-thumb rest width.
    constexpr double REST_WIDTH_SPACES = 1.25;

    const auto staff = entryInfo.createCurrentStaff();
    if (!staff || staff->lineSpace <= 0) {
        return;
    }
    const auto entry = entryInfo->getEntry();
    const double evpuPerSpace = static_cast<double>(staff->lineSpace);
    // A resized or grace entry is drawn smaller, though the articulation offsets are not scaled with it.
    double entryScale = musx::util::Fraction::fromPercent(entryInfo.calcEntrySize()).toDouble();
    if (entry->graceNote) {
        if (const auto graceOptions = entry->getDocument()->getOptions()->get<musx::dom::options::GraceNoteOptions>()) {
            entryScale *= musx::util::Fraction::fromPercent(graceOptions->gracePerc).toDouble();
        }
    }
    // Handle positions are built from whole EVPU offsets, so a vertical distance is compared as whole EVPU. Rounding
    // keeps the staff-position conversion from moving a distance across a boundary.
    const auto calcVerticalDistanceEvpu = [&](double fromStaffPosition, double toStaffPosition) {
        return std::round((toStaffPosition - fromStaffPosition) * evpuPerSpace / 2);
    };

    /// What an enclosure can enclose: one note of a note entry, or a whole rest entry.
    struct Target
    {
        musx::dom::NoteInfoPtr note; ///< Falsy for a rest.
        double staffPosition{};
        double windowBelowEvpu{};
        double windowAboveEvpu{};
        double width{};
        /// Horizontal displacement of the target from the entry's primary notehead column.
        double displacement{};
        bool isEnclosed{};
    };
    std::vector<Target> targets;
    if (entry->isNote) {
        const double columnWidthSpaces = [&]() {
            const auto noteType = std::get<0>(entry->calcDurationInfo());
            if (noteType >= musx::dom::NoteType::Breve) {
                return DOUBLE_WHOLE_NOTEHEAD_WIDTH_SPACES;
            }
            return noteType == musx::dom::NoteType::Whole ? WHOLE_NOTEHEAD_WIDTH_SPACES : NOTEHEAD_WIDTH_SPACES;
        }();
        const double columnWidth = columnWidthSpaces * musx::dom::EVPU_PER_SPACE * entryScale;
        const bool isUpStem = entryInfo.calcUpStem();
        targets.reserve(notesFromStemSide.size());
        for (const auto& note : notesFromStemSide) {
            // A note of a second drawn on the other side of the stem lies one column width outside the primary
            // notehead column: to its right on an up-stem entry, or to its left on a down-stem entry.
            double displacement = 0.0;
            if (isUpStem && note->upStemSecond) {
                displacement = columnWidth;
            } else if (!isUpStem && note->downStemSecond) {
                displacement = -columnWidth;
            }
            targets.push_back({note, static_cast<double>(note.calcNoteProperties().staffPosition), WINDOW_BELOW_NOTE_SPACES * evpuPerSpace,
                WINDOW_ABOVE_NOTE_SPACES * evpuPerSpace, columnWidth * musx::util::Fraction::fromPercent(note.calcNoteheadInfo().percent).toDouble(),
                displacement});
        }
    } else {
        targets.push_back(
            {musx::dom::NoteInfoPtr{}, static_cast<double>(entryInfo.calcTopBottomStaffPositions().first), WINDOW_BELOW_REST_SPACES * evpuPerSpace,
                WINDOW_ABOVE_REST_SPACES * evpuPerSpace, REST_WIDTH_SPACES * musx::dom::EVPU_PER_SPACE * entryScale, 0.0});
    }

    std::vector<size_t> removedIndices;
    for (const auto& opening : candidates) {
        if (opening.delimiter.side != EnclosureDelimiter::Side::Opening) {
            continue;
        }
        // The windows of notes a second apart overlap, so the handle encloses the target whose window center is
        // closest to it.
        Target* closestTarget = nullptr;
        double closestDistanceEvpu{};
        for (auto& candidate : targets) {
            const double aboveTargetEvpu = calcVerticalDistanceEvpu(candidate.staffPosition, opening.staffPosition);
            const bool isInWindow = aboveTargetEvpu >= -candidate.windowBelowEvpu && aboveTargetEvpu < candidate.windowAboveEvpu;
            const double fromWindowCenterEvpu = std::abs(aboveTargetEvpu - (candidate.windowAboveEvpu - candidate.windowBelowEvpu) / 2);
            if (isInWindow && (!closestTarget || fromWindowCenterEvpu < closestDistanceEvpu)) {
                closestTarget = &candidate;
                closestDistanceEvpu = fromWindowCenterEvpu;
            }
        }
        if (!closestTarget || closestTarget->isEnclosed) {
            continue;
        }
        auto& target = *closestTarget;
        if (opening.calcHorizontalPosition(target.width, target.displacement) >= 0) {
            continue;
        }
        const auto closing = std::find_if(candidates.begin(), candidates.end(), [&](const DelimiterCandidate& candidate) {
            return candidate.delimiter.side == EnclosureDelimiter::Side::Closing && !candidate.isConsumed
                   && candidate.delimiter.type == opening.delimiter.type
                   && results[candidate.resultIndex].assignment->hide == results[opening.resultIndex].assignment->hide
                   && std::abs(calcVerticalDistanceEvpu(opening.staffPosition, candidate.staffPosition)) <= ENCLOSURE_VERTICAL_TOLERANCE_EVPU
                   && candidate.font->isSame(*opening.font) && candidate.calcHorizontalPosition(target.width, target.displacement) > target.width / 2;
        });
        if (closing == candidates.end()) {
            continue;
        }
        closing->isConsumed = true;
        target.isEnclosed = true;
        auto& result = results[opening.resultIndex];
        result.value = BracketEnclosure{opening.delimiter.type, target.note, results[closing->resultIndex].assignment};
        result.glyphName = opening.delimiter.glyphName;
        removedIndices.push_back(closing->resultIndex);
    }
    std::sort(removedIndices.begin(), removedIndices.end(), std::greater<>());
    for (const auto index : removedIndices) {
        results.erase(results.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

std::vector<ArticulationClassification> classifyArticulations(
    const musx::dom::EntryInfoPtr& entryInfo, const ArticulationClassificationOptions& options)
{
    std::vector<ArticulationClassification> results;
    if (!entryInfo) {
        return results;
    }
    const auto entry = entryInfo->getEntry();
    const auto assignments = entry->getDocument()->getDetails()->getArray<musx::dom::details::ArticulationAssign>(
        entryInfo.getFrame()->getRequestedPartId(), entry->getEntryNumber());
    if (assignments.empty()) {
        return results;
    }
    // Computed only when a slide or an enclosure needs it, since most entries with assignments have neither.
    std::optional<std::vector<musx::dom::NoteInfoPtr>> notesFromStemSide;
    const auto getNotesFromStemSide = [&]() -> const std::vector<musx::dom::NoteInfoPtr>& {
        if (!notesFromStemSide) {
            notesFromStemSide = calcNotesFromStemSide(entryInfo);
        }
        return *notesFromStemSide;
    };
    // The slides of each kind are dealt one per note, in assignment order; see #NoteSlide.
    size_t intoNoteSlides = 0;
    size_t outOfNoteSlides = 0;
    std::vector<DelimiterCandidate> delimiterCandidates;
    results.reserve(assignments.size());
    for (const auto& assignment : assignments) {
        if (assignment->hide && !options.includeHidden) {
            continue;
        }
        const size_t resultIndex = results.size();
        auto& result = results.emplace_back(classifyAssignment(assignment, entryInfo, resultIndex, delimiterCandidates));
        if (auto* slide = std::get_if<NoteSlide>(&result.value)) {
            auto& ordinal = slide->isIntoNote() ? intoNoteSlides : outOfNoteSlides;
            if (const auto& notes = getNotesFromStemSide(); ordinal < notes.size()) {
                slide->note = notes[ordinal];
            }
            // Only the visible slides are dealt, so a hidden one takes the next visible slide's note.
            if (!assignment->hide) {
                ordinal++;
            }
        }
    }
    const auto isOpening = [](const DelimiterCandidate& candidate) { return candidate.delimiter.side == EnclosureDelimiter::Side::Opening; };
    if (std::any_of(delimiterCandidates.begin(), delimiterCandidates.end(), isOpening)
        && !std::all_of(delimiterCandidates.begin(), delimiterCandidates.end(), isOpening) && (!entry->isNote || !getNotesFromStemSide().empty())) {
        classifyBracketEnclosures(results, delimiterCandidates, entryInfo, getNotesFromStemSide());
    }
    return results;
}

} // namespace classify
} // namespace denigma
