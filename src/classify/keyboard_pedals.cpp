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
#include "denigma/classify/keyboard_pedals.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "classify.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma::classify {

namespace {

std::string normalizePedalText(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    bool pendingSpace = false;
    for (const char ch : text) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (ch != '*' && (utils::isSpace(uch) || utils::isPunctuation(uch))) {
            pendingSpace = !result.empty();
            continue;
        }
        if (pendingSpace) {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(utils::toLowerCase(ch));
    }
    return result;
}

std::optional<keyboardpedal::Type> classifyNormalizedPedalText(std::string_view text)
{
    std::string normalizedStorage = normalizePedalText(text);
    std::string_view normalized = normalizedStorage;
    constexpr std::array trailingQualifiers = {std::string_view{" sempre"}, std::string_view{" simile"}, std::string_view{" ad lib"}};
    for (bool removed = true; removed;) {
        removed = false;
        for (const auto qualifier : trailingQualifiers) {
            if (normalized.ends_with(qualifier)) {
                normalized.remove_suffix(qualifier.size());
                removed = true;
                break;
            }
        }
    }
    constexpr std::array leadingQualifiers = {std::string_view{"sempre "}, std::string_view{"con "}};
    for (const auto qualifier : leadingQualifiers) {
        if (normalized.starts_with(qualifier)) {
            normalized.remove_prefix(qualifier.size());
            break;
        }
    }

    using Type = keyboardpedal::Type;
    if (normalized == "*") {
        return Type::PedalUp;
    }
    if (normalized == "senza ped") {
        return Type::PedalUp;
    }
    if (normalized == "sost" || normalized == "sost ped") {
        return Type::PedalTwo;
    }
    if (normalized == "una corda") {
        return Type::PedalThree;
    }
    if (!normalized.starts_with("ped")) {
        return std::nullopt;
    }

    normalized.remove_prefix(3);
    while (!normalized.empty() && normalized.front() == ' ') {
        normalized.remove_prefix(1);
    }
    while (!normalized.empty() && normalized.back() == ' ') {
        normalized.remove_suffix(1);
    }
    if (normalized.empty() || normalized == "i" || normalized == "1") {
        return Type::PedalOne;
    }
    if (normalized == "ii" || normalized == "2") {
        return Type::PedalTwo;
    }
    if (normalized == "iii" || normalized == "3") {
        return Type::PedalThree;
    }
    return std::nullopt;
}

std::optional<keyboardpedal::Type> standaloneGlyphType(std::string_view glyphName)
{
    using Type = keyboardpedal::Type;
    if (glyphName == "keyboardPedalHalf" || glyphName == "keyboardPedalHalf2" || glyphName == "keyboardPedalHalf3") {
        return Type::HalfPedal;
    }
    if (glyphName == "keyboardPedalUpNotch") {
        return Type::PedalUpNotch;
    }
    if (glyphName == "keyboardPedalUpSpecial") {
        return Type::PedalUpSpecial;
    }
    if (glyphName == "keyboardPedalHookStart") {
        return Type::HookStart;
    }
    if (glyphName == "keyboardPedalHookEnd") {
        return Type::HookEnd;
    }
    if (glyphName == "keyboardPedalHyphen") {
        return Type::Hyphen;
    }
    return std::nullopt;
}

/// What a scan of Finale text has gathered so far toward a keyboard-pedal marking.
struct PedalGlyphScan
{
    /// The text with each pedal letter glyph spelled out, for matching against typed spellings.
    std::string symbolicText;
    /// A marking that only a glyph can express, such as a half pedal.
    std::optional<keyboardpedal::Type> standaloneType;
    /// Whether the single-letter S glyph named the sostenuto pedal.
    bool sawShortSostGlyph{};
    /// Where each keyboardPedalP glyph's letter sits in #symbolicText. The glyph also begins the
    /// composite Ped. sign, so it stands alone only when the next letter is not an e.
    std::vector<size_t> pedalPGlyphPositions;
};

/// Applies a pedal glyph that standaloneGlyphType does not cover. Returns false when
/// @p glyphName is not one.
bool applyPedalGlyph(PedalGlyphScan& scan, std::string_view glyphName)
{
    using GlyphHandler = void (*)(PedalGlyphScan&);
    static const std::unordered_map<std::string_view, GlyphHandler> glyphHandlers = {
        {"keyboardPedalUp", [](PedalGlyphScan& state) { state.standaloneType = keyboardpedal::Type::PedalUp; }},
        {"keyboardPedalPed", [](PedalGlyphScan& state) { state.symbolicText += "ped"; }},
        {"keyboardPedalPedNoDot", [](PedalGlyphScan& state) { state.symbolicText += "ped"; }},
        {"keyboardPedalP",
            [](PedalGlyphScan& state) {
                state.pedalPGlyphPositions.push_back(state.symbolicText.size());
                state.symbolicText += 'p';
            }},
        {"keyboardPedalE", [](PedalGlyphScan& state) { state.symbolicText += 'e'; }},
        {"keyboardPedalD", [](PedalGlyphScan& state) { state.symbolicText += 'd'; }},
        {"keyboardPedalDot", [](PedalGlyphScan& state) { state.symbolicText += '.'; }},
        {"keyboardPedalSost", [](PedalGlyphScan& state) { state.symbolicText += "sost"; }},
        {"keyboardPedalSostNoDot", [](PedalGlyphScan& state) { state.symbolicText += "sost"; }},
        {"keyboardPedalS",
            [](PedalGlyphScan& state) {
                state.sawShortSostGlyph = true;
                state.symbolicText += "sost";
            }},
    };
    const auto found = glyphHandlers.find(glyphName);
    if (found == glyphHandlers.end()) {
        return false;
    }
    found->second(scan);
    return true;
}

} // namespace

std::optional<KeyboardPedalClassification> classifyKeyboardPedal(std::string_view text)
{
    if (const auto type = classifyNormalizedPedalText(text)) {
        return KeyboardPedalClassification{*type, false};
    }
    return std::nullopt;
}

std::optional<KeyboardPedalClassification> classifyKeyboardPedal(const musx::util::EnigmaParsingContext& textContext)
{
    if (!textContext) {
        return std::nullopt;
    }

    PedalGlyphScan scan;
    bool recognizedGlyph = false;
    const auto chunks =
        textContext.collectEnigmaTextChunks(musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));
    for (const auto& chunk : chunks) {
        if (!chunk.styles.font || chunk.styles.font->hidden || chunk.text.empty()) {
            continue;
        }
        for (utils::Utf8Iterator iter(chunk.text); !iter.atEnd(); iter.next()) {
            if (!iter.valid()) {
                return std::nullopt;
            }
            const auto glyphName = detail::glyphNameForFont(chunk.styles.font, iter->codepoint);
            if (glyphName) {
                if (const auto type = standaloneGlyphType(*glyphName)) {
                    if (scan.standaloneType && *scan.standaloneType != *type) {
                        return std::nullopt;
                    }
                    scan.standaloneType = *type;
                    recognizedGlyph = true;
                    continue;
                }
                if (applyPedalGlyph(scan, *glyphName)) {
                    recognizedGlyph = true;
                    continue;
                }
            }
            if (iter->codepoint <= 0x7F) {
                scan.symbolicText.push_back(static_cast<char>(iter->codepoint));
            } else {
                return std::nullopt;
            }
        }
    }

    if (scan.standaloneType) {
        const std::string normalizedRemainder = normalizePedalText(scan.symbolicText);
        if (!normalizedRemainder.empty() && normalizedRemainder != "sempre") {
            return std::nullopt;
        }
        return KeyboardPedalClassification{*scan.standaloneType, true};
    }
    bool sawShortPedGlyph = false;
    for (auto it = scan.pedalPGlyphPositions.rbegin(); it != scan.pedalPGlyphPositions.rend(); ++it) {
        const size_t next = *it + 1;
        if (next >= scan.symbolicText.size() || utils::toLowerCase(scan.symbolicText[next]) != 'e') {
            scan.symbolicText.replace(*it, 1, "ped");
            sawShortPedGlyph = true;
        }
    }
    if (const auto type = classifyNormalizedPedalText(scan.symbolicText)) {
        return KeyboardPedalClassification{*type, recognizedGlyph, sawShortPedGlyph || scan.sawShortSostGlyph};
    }
    return std::nullopt;
}

} // namespace denigma::classify
