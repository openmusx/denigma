# Gap Report Design Decisions

Deliberate choices behind conversion gap collection and the gap report serializer, and why. These are settled positions, not open work. If a decision recorded here is ever reversed, delete the entry rather than leaving it to contradict the code.

## Collection

### Exporters depend on the collector, never on the serializer

`classify::GapCollector` (`include/denigma/classify/gaps.h`) is a header-only container of classification values and belongs to the classify library. The serializer (`denigma::gap-report`) depends on JSON and is expected to grow as more classification types become reportable, so no exporter links it. A front end that wants a report (the CLI, the WebAssembly wrapper, a test) creates a collector, passes it through `CommonOptions::gapCollector`, and serializes afterwards. `denigma_BUILD_GAP_REPORT` can remove the serializer from a build entirely; the converters are unaffected.

### Front ends serialize through `withGapReport`, not preprocessor conditionals

`include/denigma/gap_report.h` is usable in every build. It holds the one conditional block that decides whether the serializer exists, and `withGapReport(collector, callback)` runs the callback only when it does. A front end writes the callback as a generic lambda whose body depends on the writer parameter, so a build without the serializer never instantiates it and no call site carries a `#if`. The same pattern is used for instrumentation in `finale-mus-reader`.

### A gap is recorded where the exporter decides it cannot emit the feature

The MNX `processChords` and `processNotehead` functions are the same functions that will emit chord symbols and notehead shapes once MNX can represent them. Until then their output is a gap. Recording at the decision site keeps one source of truth for what was and was not exported; a separate post-pass over the source document would have to re-derive the exporter's own filtering (hidden assignments, requested-part visibility, layers hidden by alternate notation) and would drift from it.

### The payload is the classification, as structured

A gap carries the `classify` type the exporter already computed, and the serializer writes that type's fields as they are declared. There are no exporter-owned payload structs mirroring classifier types, so adding a reportable feature means adding a classification type to `GapPayload` and a serializer for it, nothing more. A consumer can derive its own types from the classifier headers by inspection.

The one payload that is not a classification is the `PlaybackOnly` marker. It says that the anchored object sounds in Finale without being drawn (a Tempo Tool change, a hidden expression assignment) and that the target cannot hide it. There is nothing to classify about it.

### A gap is recorded by the function that will one day emit the feature

There are no `report…Gaps` functions. `appendTempos` writes tempos and, while MNX has no text on a tempo, records the text as a gap; the `switch` in `processExpressions` has a case for every expression type, and a case whose MNX object does not exist yet records a gap where its export will go. The aspiration is to import everything, and keeping the gap call at the future emission site means the gap disappears by replacing one line, not by finding a parallel function.

### Formatted text is resolved when the report is written, not when the expression is classified

Every expression is classified whether or not a report is wanted, and most classifications never become gaps. The text runs with their fonts and glyph names are therefore built by the serializer from the classification's parsing context, through `classify::classifyFormattedText`, and not stored on `ExpressionClassification`. The cost is that the source document must still be alive when the report is written, which the next entry already requires.

### The collector keeps the source document alive

Some classification values keep `musxdom` instances whose accessors resolve through the document (a `FontInfo` name, the parsing context that formatted text is resolved from, the `ShapeDef` an arrowhead is drawn from), and every instance holds the document weakly. The exporter therefore hands its `DocumentPtr` to the collector (`GapCollector::retainDocument`) when it starts, and the collector owns the document until it is destroyed. A front end can then serialize after the converter has returned, which is what the CLI and the WebAssembly wrapper do. Asking front ends to serialize before the document is released was the earlier rule; it held only by accident, because the expression parsing context happened to capture a strong document reference, and the first payload without such a capture broke it. Making the classification types document-independent remains future work.

### A smart shape is a gap where the exporter would emit it, split by attachment

Finale gives every smart shape a measure assignment and gives an entry-attached one an entry assignment as well. The MNX exporter already walks both lists, the measure list once per staff measure and the entry list once per event, and each walk records the gaps for the shapes it owns: the measure walk takes the beat-attached shapes and the event walk takes the entry-attached ones (`SmartShape::entryBased`), so no shape is reported twice. An entry-attached gap is held until every part has been exported, because its end must name an event that exists in the output, and the entry it ends on may not have been reached yet, or may never be exported (a cue layer). Those gaps therefore follow the others in the report rather than sitting in traversal order.

A hidden shape is not reported: Finale draws nothing for it, so the target loses nothing by omitting it. (A hidden built-in ottava that carries the octave semantics for a visible custom line is exported as an ottava and is not a gap either.) The lyric hyphen and word-extension shape types belong to the lyrics and are never reported as smart shapes; a word extension is reported from its lyric line instead (next entry).

### A lyric word extension is a gap on its lyric line

Finale records a word extension on the syllable assignment (`LyricAssign::wext`), and the lyric line the exporter writes for that syllable is the object that stands for it, so the gap is anchored to the line's provenance id (`ev<entry>.<verse|chorus|section><n>.inci<n>`, see `core::calcLyricAssignId`) with a partial extent: the syllable is there and the extension is what it lost. The payload is `classify::LyricWordExtension`, recorded by `createLyrics` and finalized after every part has been exported, for the same reason entry-attached smart shapes are.

The extension's end is an `end` anchor when it is known. With smart word extensions on, the word-extension smart shape names the entry the extension reaches, and the end is that entry's event, or the part measure it falls in when the entry was not exported. Finale stores a held final syllable's extension with both ends on the syllable's own entry, so such a gap ends on the event it starts from: the extension runs through that event. With smart word extensions off, `wext` is the drawn length in EVPU, a layout quantity with no counterpart in the target, so the gap has no end and the payload states `reach: unknown`; the length is not reported.

## Anchors

### A gap is anchored by the target object's id, not by a source locator

A consumer of the report has the converted document and nothing else. A Finale record locator (`details/chordAssign cmper1=3 cmper2=12 inci=0`) is unusable without reimplementing `musxdom`, so a gap carries only the id of the target object it belongs to, plus a staff and a position within that object when the feature has no node of its own. The report never carries source coordinates or a copy of the source document. This applies to every location a gap names, placements included: a Finale staff cmper never reaches the report. The exporter, which owns the mapping from Finale staves to target parts and staff numbers, performs the translation when it records the gap; the serializer only writes what it is given, so no payload can take a shortcut through a source identifier. A staff the exporter has no part for is left out of the report and logged.

### Ids come from `core::element_ids`, never from a counter

The ids the exporters write (`ev<entry>`, `ev<entry>n<note>`, `<part>.m<cmper>`) derive from Finale identity, so they are stable across runs and across unrelated exporter changes, and the same id names the same entity in every target format. A JSON pointer was rejected as an anchor: it breaks as soon as a consumer inserts into the document, and it makes reference fixtures shift whenever the exporter emits a new element kind. Any object that needs an id for anchoring gets one derived the same way, added to `element_ids`, not minted in an exporter.

An id records Finale provenance, not target semantics; the semantics are already in the target document. An object derived from a measure-expression assignment is therefore `m<cmper>.textExp<def>.inci<n>` (or `shapeExp`), whatever the target made of it, and one derived from a Tempo Tool record is `m<cmper>.tempoDef.inci<n>`. A staff-list group, which is several assignments for one marking, takes the inci of the member that names the group (see `classify::ExpressionAssignmentGroup`).

### An expression gap anchors where the target put the expression

A tempo mark that MNX exported as a tempo is anchored to that tempo's id: the object exists, and the gap is what it lacks. An expression of a type MNX treats as global (tempo family, tempo alteration, rehearsal mark; see the MNX design decisions) is anchored to the global measure with a position, once per staff-list assignment group. Any other expression is a staff marking and is anchored to the part measure with its staff and position, one gap per staff it is assigned to. The choice follows the exporter's own routing of the type, not how Finale distributed the assignment.

### A spanning feature has an end anchor

A smart shape runs from one place to another, and a consumer that draws it needs both. A gap therefore carries an optional `end`, an anchor like the first: for an entry-attached shape the event, or the note when the shape names one, that it ends on; for a beat-attached shape the part measure, staff and position of its end point. An end whose entry the target did not export falls back to the measure form, so the report never names an id the document lacks. The end is a second anchor rather than a placement because a placement says where one feature is drawn again, while an end says where the same drawing stops.

### A feature drawn in several places is one gap with placements

A Finale staff list draws one marking on several staves, one assignment per staff. Reporting each assignment would report one marking several times, and dropping all but one would lose where it is drawn. The gap is recorded once, anchored as above, and carries `placements`: the target-side places it would be drawn (`system-top`, `system-bottom`, or a part measure with a staff), for the members shown in the requested score or part. Placements name target ids, translated by the exporter as the anchor rule requires.

## Serialization

### One `schemaVersion` on the report

Gaps carry a `type` string and an `extent` and nothing else by way of taxonomy. Per-gap cause codes and per-payload version numbers were dropped as answering questions no consumer had asked; the report-level `schemaVersion` covers compatibility until a second dimension matters.

### A gap states its extent

A gap is `complete` when nothing in the target stands for the feature, and `partial` when the anchored object stands for it but lost the reported payload: a tempo without its text, a tempo that should not be displayed. A consumer that renders gaps needs the distinction to know whether to draw something new or to decorate something already there, and the anchor alone does not tell it, since both kinds can anchor to an object.

### Arrowheads are embedded once, as SVG, keyed by a report-internal reference

A custom line's cap may be a Shape Designer arrowhead, which a consumer could not draw from Finale's instruction list without reimplementing Shape Designer. The report therefore embeds the arrowhead as SVG, rendered by `musxdom`'s `SvgConvert` at serialization time, in a report-level `arrowheads` table that each cap references by key. The same shape caps many lines in a typical document, so the table holds each once. Preset arrowheads are embedded the same way so that a consumer needs no knowledge of Finale's presets either.

The key (`custom-<cmper>`, `preset-<name>`) is minted by the serializer from the arrowhead's identity in the source and is the one place the report carries a source identifier. It names nothing in the target document, so the anchor rule does not apply to it; it exists only so that caps can share an image, and a consumer treats it as opaque.

The image's coordinates are staff spaces at 100% staff size (`SvgConvert::toSvg` with a scaling of `1 / EVPU_PER_SPACE` and no unit suffix), because a notation renderer thinks in spaces and scales a marking with its staff. The `viewBox` is the shape's own bounds in that space, so the shape's origin, which Finale places at the end of the line, stays recoverable; the entry states `unit` and `origin` so a consumer need not know this. Text drawn inside an arrowhead shape is measured through the `GlyphMetricsFn` the front end passes in `GapReportOptions`, or sized heuristically when there is none.

### Reference fixtures are named per conversion

`chords.mnx.gaps.json` records the gaps of the MNX conversion of `chords.musx`. The payloads and anchors are format-neutral, but which features are gaps depends on the target, so a MusicXML report of the same file would differ and would carry its own name.
