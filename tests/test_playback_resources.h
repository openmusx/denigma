// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "denigma/mus_resources.h"

namespace denigma::test {

/// The sound that `drumset.musx` routes its drum staff to. In Finale's own soundmaps this uuid is a
/// piano with no percussion map, so a synthetic soundmap can give it any note list.
inline constexpr const char* DRUMSET_SOUND_UUID = "db30b478-5871-3ba5-9553-6f964bb3aeb7";

/// Builds Finale resources whose only sound is @p soundUuid. When @p keysByNoteType is given, the sound
/// plays the note list "Synthetic Kit" in "Synthetic.xml", which maps each percussion note type to a
/// 0-based MIDI key. Without it, the sound names no percussion map.
inline std::shared_ptr<const MusResources> createSyntheticPlaybackResources(
    const std::string& soundUuid, const std::optional<std::vector<std::pair<int, int>>>& keysByNoteType)
{
    std::string soundmap = "<Soundmap><Description><Name>Synthetic</Name><PluginID>SYNT</PluginID></Description>"
                           "<Sound id=\"Kit\" uuid=\""
                           + soundUuid + "\"><Name>Synthetic Kit</Name>";
    if (keysByNoteType) {
        soundmap += "<PercMIDIMap>Synthetic Kit</PercMIDIMap><PercMIDIMapFile>Synthetic.xml</PercMIDIMapFile>";
    }
    soundmap += "</Sound></Soundmap>";
    std::string deviceFile = "<FinaleNameDocument><MasterDeviceNames><NoteNameList Name=\"Synthetic Kit\">";
    for (const auto& [noteType, midiKey] : keysByNoteType.value_or(std::vector<std::pair<int, int>>{})) {
        deviceFile += "<Note Number=\"" + std::to_string(midiKey) + "\" PercNoteType=\"" + std::to_string(noteType) + "\"/>";
    }
    deviceFile += "</NoteNameList></MasterDeviceNames></FinaleNameDocument>";
    const auto bytes = [](const std::string& text) { return std::as_bytes(std::span(text.data(), text.size())); };
    const std::vector<MusResource> resources = {
        {MusResourceKind::Soundmap, "Synthetic.soundmap", bytes(soundmap)},
        {MusResourceKind::PercussionMap, "Synthetic.xml", bytes(deviceFile)},
    };
    return MusResources::create(resources);
}

} // namespace denigma::test
