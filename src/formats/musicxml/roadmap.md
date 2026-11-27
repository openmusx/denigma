# MusicXML Feature Backlog

This is the implementation backlog for larger user-facing MusicXML export features. It is not a version plan or a dependency order. Keep concrete `mx::api` limitations in [mx-api-gaps.md](mx-api-gaps.md).

## Compressed MusicXML (`.mxl`) output

Export useful standards-compliant compressed MusicXML in addition to uncompressed `.musicxml` files. A useful Denigma archive must package the score together with all linked part documents that the exporter already knows how to produce, and must preserve the relationships between them. The archive should contain an uncompressed `mimetype` entry first, a `META-INF/container.xml` file that identifies the root MusicXML document, and the linked score and part documents.

The existing ZIP utilities and MXL massage support can supply archive mechanics, but they do not solve the packaging model. A score-only archive is technically possible, but it provides no added user value for Denigma's current multi-output workflow and should not be treated as the feature's initial completion criterion. See [mx-api-gaps.md](mx-api-gaps.md) for the missing score/part relationship support.

## Microtonal key signatures

Extend the nontraditional key-signature mapping to microtonal signatures by converting Finale's EDO divisions into MusicXML semitone alterations and suitable accidental values. Preserve the effective written or concert-pitch signature independently for each staff.

## Instruments

Instrument sound coverage is partial in a way that hides itself. `musicXmlInstrumentSoundFromUuid` maps a fixed table of Finale instrument uuids, several entries are commented out, and an unrecognized uuid simply yields no `<instrument-sound>` with no diagnostic. Decide whether an unmapped instrument deserves a Verbose log, and extend the table.

Finale's own export writes `<virtual-instrument>` on most staff instruments (105 of the staff instruments in its exports of the 66 installed repertoire files that have drum kits), though every one seen so far is empty: `<virtual-instrument/>`. Denigma writes none. mx models the element through `MidiData::virtualLibrary` and `virtualName`. Come back to this to find out whether Finale ever fills it, and from what, before deciding whether Denigma should write it. Finale writes it only on staff instruments, never on drum-kit pieces.

Two neighbouring items overlap this one and should stay separate. MIDI channels below covers the playback assignment that hangs off the instrument, and percussion covers drum-kit pictograms.

## MIDI channels

Export each part's MIDI playback assignment — channel, and with it program and bank where available — as MusicXML `<midi-instrument>` data. `mx::api::PartData::midiData` already models channel, program, bank, volume, and pan, so no MX API work is needed.

The source data requires musxdom effort first: Finale's channel assignments live in its playback system (instrument definitions and their staff/layer routing), which musxdom does not yet model. Once musxdom exposes that to some degree, this becomes a Denigma mapping task.

## Percussion

Export Finale percussion pictogram expressions as semantic MusicXML `<percussion>` directions. Add an exporter-neutral classifier for exact `pict*` SMuFL glyphs, including valid beater and stick combinations with tip direction, material, parentheses, dashed circles, and strike location. Map Finale expression enclosures where supported, retain canonical SMuFL overrides, and leave mixed text or unrecognized glyph sequences as general text. Do not infer direction pictograms from percussion-note assignments; note identity and performance-direction symbols are separate concerns.

## Custom staff lines

Restate a pitched clef when the staff's lines change under it. A clef is written only where it
changes, so when a staff style moves the staff's bottom line under an unchanged pitched clef, the
clef's line is not restated, and a reader places the notes off by the distance the bottom line moved
(`custom_staves_1.musx`, measures 3, 5 and 6). Finale's own export does the same. Write the clef with
its new line where the line changes.

## Hidden staves

Open question: how to export a staff that Finale hides. A staff can be hidden by its own Force Hide
Staff setting (`Staff::hideMode`) or by a staff style that sets it. A staff that an optimized system
leaves out is layout, and is settled in the design decisions. The Jingle Bells vocal score
(`Repertoire/Holiday Patriotic/Vocal/Jingle Bells.musx`) hides its percussion staff with its own
setting (`hideMode` cutaway). The Melodic Dictation answer worksheets (7212 to 7217) hide staves 2
and 3 for the whole piece with a "Hide Staff" style. Finale's own export keeps these staves as parts
and writes `<staff-details print-object="no">` on them. Denigma exports them as visible staves.
Decide whether to follow Finale, how the hide modes (cutaway, collapse in score and parts, collapse
in score only) should differ, and how a style that hides a staff for only some measures should
export, since MusicXML's staff details apply from a measure onward.

## Notehead fill on crossed shapes

The `filled` attribute changes the appearance of enclosed shapes, so it says nothing about an X notehead. The classifier still gives X glyphs a fill for their duration variant, which writes `filled` where one disagrees with the note value, such as `noteheadXBlack` on a half note. Omit `filled` for `x` once the effect on other readers is checked.

## Alternate notation: measure repeats and slash notation

Export effective staff alternate notation as MusicXML measure styles: one- and two-bar repeats, slash notation on beats, and rhythmic notation. This requires start/stop ranges per staff and, for slash notation, dots, stems, and where possible voice exclusions. It also needs a new public MX measure-style API and writer path.

Layer-only behavior, blank notation, and Finale's independent hiding of articulations, lyrics, expressions, and smart shapes are separate fidelity work. See [mx-api-gaps.md](mx-api-gaps.md) for the detailed limitation analysis.

## Fretboard diagrams

Export Finale fretboard diagrams through MusicXML `<frame>` data on their associated `<harmony>` elements. This does not refer to simultaneous note chords.

MUSX DOM exposes the fretboard groups, styles, and diagrams referenced by `details::ChordAssign`. `mx::api::ChordData::frameData` can represent the basic string/fret grid, first fret, barre, and fingering data.

First export the basic diagram and its note/fingering/barre details, then assess specialized frame appearance, diagram placement, and capo behavior.

## Accordion registration: combining glyph sequences and legacy fonts

SMuFL defines combining glyphs (`accdnCombRH3RanksEmpty`, `accdnCombDot`, and their siblings) for a custom accordion-registration diagram outside the standard precomposed set. Exporting one means reassembling it from source glyphs, and how Finale users build such a diagram is unconfirmed. It could be several font-character runs in a Text Expression, each run's baseline or superscript shift placing a dot, or a Shape Designer shape whose `DrawChar` instructions place the dots, which would make it a Shape Expression. `classifyAssignedShapeExpression` has no accordion case, and `musx::dom::KnownShapeDefType` has no accordion entry.

Revisit this only when a real Finale file exhibits a custom accordion registration, and let that fixture settle which route (Shape Designer shape vs. baseline-shifted text runs, or both) is worth supporting. The same discipline applies to legacy, pre-SMuFL accordion registration fonts: `smufl_mapping` has no such font mappings today, and none should be added speculatively — add one only against a real fixture that needs it.

## Additional direction types

Use the principal-voice, other-direction, and image models now exposed by `mx::api`.

For principal voice, obtain representative Finale Hauptstimme and Nebenstimme smart-line fixtures before implementing the mapping. The exporter must identify both ends of the span and choose the appropriate principal-voice symbol; an isolated `analyticsHauptstimme` or `analyticsNebenstimme` expression glyph is not enough to infer a matching stop.

Use `OtherDirectionData` only for recognized direction semantics that lack a dedicated MusicXML direction type. Preserve a canonical SMuFL name and useful fallback text where available, but do not convert every unrecognized expression glyph into `<other-direction>`.

Export measure-attached Finale graphics from `details::MeasureGraphicAssign` as MusicXML `<image>` directions. Resolve embedded and external graphic sources, emit required image files through the multi-output callback, determine MIME types, and convert Finale position and size values to MusicXML tenths. Page graphics and graphics embedded in Shape Designer objects remain separate mapping tasks.

## Tablature staves

Export Finale tablature staves as MusicXML tablature rather than as ordinary pitched staves.
Nothing reads `others::Staff::notationStyle` today, so a TAB staff exports as though it were
standard: the pitches are correct and everything that makes it tablature is dropped, with no
diagnostic. `Staff::NotationStyle` distinguishes `Standard`, `Percussion`, and `Tablature`, and it
can be overridden by a staff style, so the effective value is what matters.

Four pieces make up the feature:

- The TAB clef. MusicXML spells it `<clef><sign>TAB</sign>`, and its presence is what tells a
  reader that noteheads are fret numbers; the spec notes that a TAB clef alone is sufficient to
  imply that, so no per-note text is needed. `Staff::showTabClefAllSys` says whether Finale repeats
  it on every system.
- Staff details. `<staff-tuning>` per line, from the `others::FretInstrument` at
  `Staff::fretInstId`, whose `StringInfo::pitch` gives each open string's MIDI pitch and whose
  `nutOffset` shifts it. `<capo>` comes from `Staff::capoPos`, and `<show-frets>` from
  `Staff::useTabLetters`, which selects letters over numbers. `Staff::lowestFret` and `numFrets`
  have no direct MusicXML equivalent on a staff.
- Per-note string and fret, as `<technical><string>` and `<fret>`. This is the hard part, see
  below.
- Layout specifics with no MusicXML equivalent: `Staff::vertTabNumOff`,
  `breakTabLinesAtNotes`, and `hideTuplets`. Decide a downgrade policy rather than attempting them.

The per-note half needs real work in MUSX DOM terms. Finale does not store fret numbers.
`details::TablatureNoteMods` ("tabAlter") records only a `stringNumber`, and only for notes whose
string assignment was overridden; its documentation states that Finale derives the fret from the
open-string pitch and the fret intervals in the staff's `FretInstrument`. So Denigma must compute
the fret from the note's pitch, the string's open pitch and nut offset, and the capo, and must also
reproduce Finale's automatic string assignment for every note that carries no override. That
algorithm is not documented in MUSX DOM and would need to be established, ideally as a musxdom
helper rather than in the exporter, so MNX can share it.

The per-note export is not blocked upstream. `mx::api` writes `<string>` and `<fret>` as the
`MarkType::string_` and `MarkType::fret` technical marks, with the number in `MarkData::name`. The
staff tunings are available through `StaffData::staffTunings`, but capo and show-frets remain API gaps. Reliable source-side string/fret calculation is also needed.

Two neighbours are related but separate. Fretboard diagrams above cover `<frame>` on `<harmony>`,
which is chord diagrams rather than staff content, though both read `FretInstrument`. Bends
(`BendHat`, `BendCurve`) are guitar technique under `<technical>` and are noted with the glissando
work below.

One detail is shared with percussion staves: any future notion of "the same note" on a percussion staff would
have to compare percussion note type, not merely staff position, since one position can host
different instruments and one instrument can move position. Nothing depends on that today.

## Bends

Export Finale's `BendHat` and `BendCurve` smart shapes as MusicXML `<technical><bend>`, which the
spec describes as "used in guitar notation and tablature". A bend carries `<bend-alter>` for the
interval, an optional `<pre-bend>` or `<release>`, `<with-bar>` for whammy-bar notation, and a
`bend-shape` attribute distinguishing the angled symbols of standard notation from the curved ones
common to tablature. `<bend>` also shares the `bend-sound` attribute group with `<slide>`, so its
playback approximation is expressible.

These are the third and last family of note-attached Finale lines, alongside glissandi and tab
slides, and the only remaining one Finale attaches to noteheads. They are dropped today: neither
shape type is classified, so both reach the exporter as `std::monostate` and are logged by the
unclassified-shape path.

`mx::api` writes a bend as a `MarkType::bend` technical mark whose `BendMarkData` carries `alter` in
semitones, `kind` (normal, pre-bend, or release), `releaseOffset` in ticks for a release, `withBar`
for whammy-bar notation, and `shape` (angled or curved). A bend and its release are two marks on one
note. `bend-sound` is not exposed; record it in [mx-api-gaps.md](mx-api-gaps.md) if the playback
approximation turns out to matter. Tablature staves above is a neighbour rather than a prerequisite:
a bend is notated in standard notation too, so this does not wait on TAB support.

## Shape-replaced stems

Export the Finale custom stems from `details::CustomStem` (`CustomUpStem` / `CustomDownStem`) that replace the stem with a Shape Designer shape. A shape-replaced stem keeps its ordinary direction, and the shape itself is dropped, as it is in Finale's own export.

An arbitrary stem shape has no MusicXML equivalent, so the case worth pursuing is a shape drawing one of SMuFL's combining tremolo stems, such as `stemPendereckiTremolo`. That is a tremolo in every sense except how Finale stores it, and it could export as `MarkType::tremoloUnmeasured` carrying the glyph name, exactly like the equivalent articulation. Recognizing it requires shape recognition in MUSX DOM, the same upstream dependency described below.

## Stacked single-note tremolos

Export single-note tremolos with six, seven, or eight slashes. `mx::api` models the whole MusicXML range through `MarkType::tremoloSingleSix`, `tremoloSingleSeven`, and `tremoloSingleEight`, but Denigma supports only one through five, because SMuFL precomposes only `tremolo1` through `tremolo5` and a Finale articulation is a single character or a Shape Designer shape. Both the classification and the exporter mapping remain to be done.

Finale can spell the higher counts only by stacking: a Shape Designer shape that draws several tremolo glyphs, or two tremolo articulations assigned to one entry. Recognizing the first requires a new `KnownShapeDefType` and recognizer in MUSX DOM, and a stack with variable count and spacing is a fuzzier recognition target than the fixed patterns already there. Recognizing the second requires entry-level aggregation plus vertical-offset geometry, since two tremolo articulations on one entry may equally well be two separate marks.

This is gated on evidence. Revisit it when a real-world Finale file actually spells such a tremolo; that file also settles which of the two routes is worth supporting.

## Extend the font-availability assertion to MNX

Apply the shared `allFontsAvailable` export option (`--all-fonts-available`), which says every source font will be available where the output is read, to MNX once MNX has official formatted strings. Until then, leave its current `PreferSmufl` behavior alone rather than designing around a provisional text representation. At that point, reconcile MNX with MusicXML's two meaningful choices: without the fonts use `SplitSmufl`; with them use `PreserveText`. The intermediate `PreferSmufl` has no identified long-term use case.

The assertion may eventually inform other portability choices too. Generic font-family fallbacks are the obvious neighbor because they exist for the same missing-font case, but changing them should be decided separately rather than folded into formatted-text preservation.

## Text and custom-line fidelity

Convert eligible music-font characters in expression text to `SymbolData` within the ordered `DirectionChoice::wordsRun` model, particularly for legacy symbol fonts that may not be installed on the receiving system. Preserve unknown or intentionally font-specific characters as `WordsData`.

Use page-specific `PageData` layout overrides when computing absolute credit anchors. The exporter currently uses the score's default odd/even page size and margins, so credits on pages with Finale layout overrides may be misplaced.

Define intentional downgrade policies for Finale text and line features that MusicXML cannot represent. These include full and forced-full text justification; arbitrary Shape Designer text frames; page and measure text-block geometry such as fixed dimensions, insets, corner radius, line spacing, and word wrapping; custom-line continuation text shown after system breaks; and center full/abbreviated text on general bracket or dashes lines. Preserve the closest standard appearance where possible and log material omissions.

## MX diagnostics

`writeMusicXmlToCallback` in `musicxml.cpp` logs every `mx::api::Diagnostic` it is handed, the
id-integrity codes as warnings and the rest at Verbose. Two decisions remain. Which of
`valueAdjusted`, `invalidValue`, `missingValueDefaulted`, `unmatchedSpanner`, and `droppedData`
name a Denigma defect rather than expected normalization, and so deserve promoting out of Verbose,
is not yet known; each needs checking against the fixture corpus. And data mx drops may belong in
a typed gap collection, as MNX has, rather than in the log at all.

`mxResultMessage` also reports only the XML path of a failure, although `ApiError::location` now
carries part, measure, staff, voice, and tick as well.

## Available MX APIs awaiting source mappings

The mx update at `e866c709` exposes these additional capabilities. Adopt them when Finale source semantics can be mapped reliably:

- Grace-note steal-time percentages and make-time ticks. Finale's grace playback duration alone does not establish which neighbouring note supplies that time; do not infer it or substitute make-time.
- Notehead color. Resolve actual per-note authorial settings before populating it; do not infer colors from Finale's editing display.
- Additional creators and rights statements. Finale's existing single file-info fields do not establish separate people or rights records; do not split their free text heuristically.
- Figured-bass extension lines. These need a figured-bass export path before they can be used.
- Additional accidental-mark appearance attributes. Existing ornament classification supplies accidental type and placement, but no independent brackets, size, font, or color.

The mx update at `e335eb5f` (v0.7.4) adds these:

- Note accidentals that MusicXML cannot name: `Accidental::other` with a SMuFL glyph in `NoteData::accidentalSmufl`. Pair with the microtonal key signatures above, where an EDO alteration has no MusicXML accidental value.
- Arrow marks: `MarkType::arrow` with `ArrowMarkData` for style, arrowhead, circular arrows, and a SMuFL glyph. Strum arrows and other arrow articulations would need recognizing in the articulation classifier first.
- Figured-bass position and print attributes, which need the figured-bass export path noted above.

The mx update at `9fb57380` adds `<footnote>` through `mx::api::FootnoteData`. Finale has no footnote element, but it may become possible to detect text that serves as a footnote or header note and attach it to the element it annotates.
