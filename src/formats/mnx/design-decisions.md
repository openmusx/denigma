# MNX Design Decisions

Deliberate choices the MNX exporter makes, and why. These are settled positions, not open work.

Deferred features and concrete `mnxdom` limitations belong with the code that runs into them. If a decision recorded here is ever reversed, delete the entry rather than leaving it to contradict the code.

## Tempo

### A tempo states what Finale plays, not what the page shows

An MNX tempo is a playback instruction. It carries a number and a note value and nothing else: there is no way to give it visible text, and a reader that draws it at all draws a generic metronome marking of its own devising. So the number Denigma writes is the tempo Finale would play, and the appearance of the Finale expression that produced it is not part of the question.

A Finale text expression stores a playback tempo, as a value and a beat unit, separately from whatever its text happens to say, and the two are free to disagree. Those stored settings are what Denigma exports, because they are what Finale sounds. An expression with no playback settings at all falls back to its displayed equation, which is the only tempo it has. A displayed number written with a decimal point survives that fallback, since MNX `bpm` is a real number as of schema version 34.

Finale's "Match Playback to Metronome Marking Text" setting is not consulted. It appears to keep the stored value in step with the text as the text is edited, which leaves the stored value correct and makes reading the text unnecessary; what it does not do is supply a beat unit, so an expression can display one note value and sound another. `metronome_marks.musx` holds such a case: its first mark displays an eighth and stores a playback beat of a quarter, and it exports as the quarter it sounds.

The displayed equation is not lost by this. MusicXML has room for both numbers and writes both: `<per-minute>` takes the printed equation and `<sound tempo>` takes the playback tempo. Only MNX, having one field, has to choose.

### Every tempo carries a provenance id

A tempo is written with the id of the Finale object it came from, `m<cmper>.textExp<def>.inci<n>` (or `shapeExp`) for an expression and `m<cmper>.tempoDef.inci<n>` for a Tempo Tool record, whether or not a gap report was requested. The gap report anchors a tempo's lost text to this id, and an output that changed shape depending on whether a report was wanted would make the report describe a different document from the one the user has. Several expressions can share a position, so the id names the assignment rather than the position.

### A Tempo Tool change that loses its position is dropped, not reported

Expressions take precedence over Tempo Tool changes at the same position. A Tempo Tool change that loses is dropped silently and never becomes a gap: it is implicitly playback only, so nothing that could be drawn is lost with it, and Finale files routinely carry a Tempo Tool record beside a tempo expression, so the drop is the ordinary case rather than a finding worth a diagnostic. One that is exported is reported as a partial `playback-only` gap, because MNX cannot mark a tempo as hidden and a reader will draw it. A hidden expression assignment, which Finale also plays without drawing, gets the same `playback-only` gap and, when it has text, the text gap as well: the two say different things, that Finale shows nothing and what it would show, and a client that decides to display the tempo anyway needs the second.

### A playback beat unit that is not a note value is restated in quarter notes

MNX states a tempo as a count of one note value per minute, so the beat unit has to be a note value: a base with some number of augmentation dots. Finale's beat unit is a raw EDU duration and need not be either. `calcDurationInfoFromEdu` answers with the closest base and dot count for any duration in range rather than refusing, so the exporter spells its answer back out and compares it against the original before trusting it.

A beat unit that does not survive that comparison is restated as a count of quarter notes. Nothing is lost by the restatement: MNX `bpm` is a real number as of schema version 34, and dividing by the EDUs in a quarter note divides by a power of two, which is exact in binary floating point. The same path takes a beat unit outside the range of a note value, which `calcDurationInfoFromEdu` would otherwise throw on.

## Expressions

### Which expressions belong to the global measure is decided by type

The classifier says what a marking is; whether it attaches to the global measure or to a part measure is MNX's decision, made by expression type in `processGlobalExpressions`. Tempo marks, metronome marks, tempo alterations and rehearsal marks are one marking of the whole score however Finale distributes them, so the global pass handles them once per staff-list assignment group (`classify::groupExpressionAssignments`), whichever staff the assignment sits on. Everything else is a staff marking even when a staff list copies it onto several staves or a category staff list distributes it: a technique text or a multimeasure-rest number on three staves is three staff expressions, and `processExpressions` handles each with its part measure. The presence of a staff group or a floating staff value therefore never routes an expression on its own.

## Smart shapes

### A smart shape MNX cannot carry is a gap from its emission site, and a beat-attached slur is one of them

MNX has slurs, ties, ottavas, dynamic wedges, arpeggios and non-arpeggio brackets and nothing else that spans. Every other smart shape (a custom or built-in line, a glissando or tab slide, a trill or vibrato line, a keyboard pedal line, a bend, an entry-attached trill) is recorded as a gap by the function that would emit it: `processSmartShapes` for beat-attached shapes and `processEntrySmartShapes` for entry-attached ones, following the exporter's existing split of the two assignment lists. The gap report's design decisions say how the two are anchored.

A beat-attached slur is a gap as well. The MNX slur path hosts the entry-attached slurs Finale normally creates, whose endpoints name their events; a beat-attached slur names positions, and hosting it would mean choosing the events those positions coincide with. That is roadmap work, and until it is done the slur is reported rather than dropped, as is an entry-attached slur whose other end coincides with no event.

## Lyrics

### A lyric line carries a provenance id only when it has a word extension

The gap report anchors a lost word extension to the lyric line it belongs to, and, as with tempos, the id is written whether or not a report was requested, so that the report never describes a different document from the one the user has. Unlike a tempo, a syllable is not an object anything else points at, and lyric-heavy scores have thousands of them; an id on every line would outweigh the lyrics. So a line takes its id (`core::calcLyricAssignId`) exactly when its assignment has a word extension, the one case a gap anchors to it, and no other line has one.

## Sequences

### An empty staff measure is written as a full-measure rest

MNX requires a part-measure to carry a `sequences` array but says nothing about what an empty array means. A reader is free to draw nothing for it, and the spec's own full-measure-rest example spells an empty bar out as a sequence with `fullMeasure` set. So a staff that exports no music in a measure gets exactly that: one sequence with empty `content` and `fullMeasure: {}`, naming its staff when the part has more than one. This matches the MusicXML exporter, which writes a `<rest measure="yes"/>` for the same measures.

Whether the rest appears is decided from the staff as it stands at the start of the measure, the same way Finale decides it. "Display Rests in Empty Measures" turned off, whether on the staff or through a staff style, leaves the measure with no sequences. So does an alternate notation that replaces entries, since a measure repeat or slash region draws its own content and a blank-notation region draws nothing; the measure-repeat example in the spec likewise carries no full-measure rest. "Display Rests", which hides entered rests, is not consulted: MNX has no way to hide a rest, and the exporter already exports entered rests regardless of it. Consult it here and for entered rests if MNX ever gains a `hidden` option on rests.

Cue entries are skipped by the exporter and have no MNX representation yet, so a measure holding only cues counts as empty and gets the rest its staff settings call for. Revisit this when MNX gains a cue encoding.

A staff-attached fermata over an empty measure attaches to that staff's full-measure rest. A staff whose settings suppressed the rest gets one created for the fermata anyway, because MNX has nowhere else to put it.

### The entries of a zero-length tuplet are omitted

A Finale tuplet with a reference count of zero takes no time, and its entries are phantoms: a user or a plugin such as Beam Over Barline puts a note there so that a beam or a tie can reach across a barline, and the note that actually sounds sits at the same position. MNX has nowhere to put such an entry. A tuplet's ratio must be positive, an event outside a tuplet always takes its written duration, and a `grace` container would draw the note small. So the tuplet and its entries are omitted, and nothing else may refer to them: a beam skips them, a tie into one is written as `lv`, and a slur ending on one is a gap. The one form `musxdom` interprets, a singleton beam with the phantom's notehead and stem hidden, never reaches this path.

The count of omitted tuplets is logged once per conversion as a warning. Each occurrence is logged only at verbose level, because a document that carries the workaround carries it many times.

### A sequence's direction hint comes from its layer's stem setting

A v1 sequence carries `directionHint` exactly when Finale's layer attributes freeze its stems and those attributes are in effect (`EntryInfoPtr::calcIfLayerSettingsApply`). That holds when the layer is alone on the staff, because "Apply Settings Only if Notes are in Other Layers" can be off, and when the layer also has v2 entries, although every stem in such a layer is written explicitly. A v2 sequence never carries a hint: Finale stems voice 2 per v2 launch, so no one direction describes the sequence.

Events get an explicit `stemDirection` only for a manual stem freeze and in a layer that uses v2. A v1-only layer relies on the hint.

## Clefs

### A clef Finale does not draw is a hidden clef

A clef with `ShowClefMode::Never`, or with `WhenNeeded` on a staff that hides clefs, is exported with `hide`, matching the MusicXML exporter's `print-object`. A blank clef is hidden as well. On a percussion staff it becomes a percussion clef. On any other staff it keeps its letter clef when Finale's clef definition reads as one, and otherwise becomes a treble clef, which is what Finale's own blank clefs are. Tablature clefs have no MNX sign and are still skipped.

## Tuplets

### Stem-relative tuplet placement names its side

MNX `auto` placement leaves the side to the reader, so Finale's beam-side and note-side positioning styles are resolved against the stem direction of the tuplet's first entry and written as `above` or `below`. Manual positioning names no side and stays `auto`.

## Schema items not exported

Each of these is omitted on purpose. Revisit one if Finale data turns out to carry it.

- `grace.graceType`: a playback nuance; the MNX default stands.
- `multi-note-tremolo.individualDuration`: derivable from `marks` and `outer`.
- `ottava.placement`: Finale draws an ottava where convention puts it, which is what `auto` means.
- `ottava.voice`: Finale ottavas apply to the whole staff.
- `page.layout` and `system.layoutChanges`: a Finale system has one staff configuration, and each system already names its layout.
- `dynamic-group.visuallyContinues`: the spec asks readers to derive it from `position` and `end`.
- `dynamic-group.staffEnd`: Finale hairpins do not cross staves.
- `slur.sideEnd`: Finale has no mid-slur side change.
- `slur.startNote` and `endNote`: a Finale slur attaches to an entry, never to one note of a chord.
- Layout `staff.label`, `labelref` and `symbol`: a Finale staff has one source, so labels go on the source, and a bracket on a single staff is already a group.
- `measure-repeat.displayNumber` and `staffPosition`: Finale draws both by convention.
- `lyric-line-metadata.lang` and `_c`: Finale has no source for either.
- `perform`: the schema defines it as an empty object.
