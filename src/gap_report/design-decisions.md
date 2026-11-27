# Gap Report Design Decisions

Deliberate choices behind conversion gap collection and the gap report serializer, and why. These are settled positions, not open work. If a decision recorded here is ever reversed, delete the entry rather than leaving it to contradict the code.

## Collection

### Exporters depend on the collector, never on the serializer

`classify::GapCollector` (`include/denigma/classify/gaps.h`) is a header-only container of classification values and belongs to the classify library. The serializer (`denigma::gap-report`) depends on JSON and is expected to grow as more classification types become reportable, so no exporter links it. A front end that wants a report (the CLI, the WebAssembly wrapper, a test) creates a collector, passes it through `CommonOptions::gapCollector`, and serializes afterwards. `denigma_BUILD_GAP_REPORT` can remove the serializer from a build entirely; the converters are unaffected.

### Front ends serialize through `withGapReport`, not preprocessor conditionals

`include/denigma/gap_report.h` is usable in every build. It holds the one conditional block that decides whether the serializer exists, and `withGapReport(collector, callback)` runs the callback only when it does. A front end writes the callback as a generic lambda whose body depends on the writer parameter, so a build without the serializer never instantiates it and no call site carries a `#if`. The same pattern is used for instrumentation in `finale-mus-reader`.

### A gap is recorded where the exporter decides it cannot emit the feature

The MNX `processChords` function is the same function that will emit chord symbols once MNX can represent them, and until then its output is a gap. `processNotehead` emits notehead shapes and records a gap only for a Finale notehead override whose glyph has no SMuFL name. Recording at the decision site keeps one source of truth for what was and was not exported; a separate post-pass over the source document would have to re-derive the exporter's own filtering (hidden assignments, requested-part visibility, layers hidden by alternate notation) and would drift from it.

### The payload is the classification, as structured

A gap carries the `classify` type the exporter already computed, and the serializer writes that type's fields as they are declared. There are no exporter-owned payload structs mirroring classifier types, so adding a reportable feature means adding a classification type to `GapPayload` and a serializer for it, nothing more. A consumer can derive its own types from the classifier headers by inspection.

Text formatting and insert payloads are document-independent source snapshots attached to output text ranges. They retain source characters so converting a legacy character to a canonical glyph remains reversible.

A classification is target-neutral and never holds a target id, but some gaps need target ids beyond their anchor: where a span ends, which output text a formatting run covers, where each staff-list member is drawn, which pages show page text. Those belong to the one payload that needs them, not to every gap, so such a payload is a record that pairs the classification with what the target made of it: `SmartShapeGap` (the `end`), `ExpressionGap` (the `placements`), `PageTextGap` (the pages), and the text formatting and insert payloads, which carry their own `ranges`. A gap itself is only an anchor, an extent and a payload. A record refers into its classification by index where it adds a target id to something the classification lists, so the source fact and its target place are stated once each and the link between them is explicit.

Another payload that is not a classification is the `PlaybackOnly` marker. It says that the anchored object sounds in Finale without being drawn (a Tempo Tool change, a hidden expression assignment) and that the target cannot hide it. There is nothing to classify about it.

### A gap is recorded by the function that will one day emit the feature

There are no `report…Gaps` functions. `appendTempos` writes tempos and, while MNX has no text on a tempo, records the text as a gap; the `switch` in `processExpressions` has a case for every expression type, and a case whose MNX object does not exist yet records a gap where its export will go. The aspiration is to import everything, and keeping the gap call at the future emission site means the gap disappears by replacing one line, not by finding a parallel function.

### Expression classification retains a parsing context until formatted text is needed

Every expression is classified whether or not a report is wanted, and most classifications never become gaps. `ExpressionClassification` therefore retains a parsing context rather than eagerly building formatted text. The serializer resolves full expression-gap text through `classify::classifyFormattedText` when writing the report; this requires the source document to remain alive, as the next entry requires. An exporter that emits general text resolves that context at emission time instead, because it needs the glyphs for its output, and collects document-independent formatting and insert snapshots with their output ranges.

### The collector keeps the source document alive

Some classification values keep `musxdom` instances whose accessors resolve through the document (a `FontInfo` name, the parsing context that formatted text is resolved from, the `ShapeDef` an arrowhead is drawn from), and every instance holds the document weakly. The exporter therefore hands its `DocumentPtr` to the collector (`GapCollector::retainDocument`) when it starts, and the collector owns the document until it is destroyed. A front end can then serialize after the converter has returned, which is what the CLI and the WebAssembly wrapper do. Asking front ends to serialize before the document is released was the earlier rule; it held only by accident, because the expression parsing context happened to capture a strong document reference, and the first payload without such a capture broke it. Making the classification types document-independent remains future work.

### A smart shape is a gap where the exporter would emit it, split by attachment

Finale gives every smart shape a measure assignment and gives an entry-attached one an entry assignment as well. The MNX exporter already walks both lists, the measure list once per staff measure and the entry list once per event, and each walk records the gaps for the shapes it owns: the measure walk takes the beat-attached shapes and the event walk takes the entry-attached ones (`SmartShape::entryBased`), so no shape is reported twice. An entry-attached gap is held until every part has been exported, because its end must name an event that exists in the output, and the entry it ends on may not have been reached yet, or may never be exported (a cue layer). Those gaps therefore follow the others in the report rather than sitting in traversal order.

A hidden shape is not reported: Finale draws nothing for it, so the target loses nothing by omitting it. (A hidden built-in ottava that carries the octave semantics for a visible custom line is exported as an ottava and is not a gap either.) The lyric hyphen and word-extension shape types belong to the lyrics and are never reported as smart shapes; MNX carries a word extension as a lyric extender.

## Anchors

### A gap is anchored by the target object's id, not by a source locator

A consumer of the report has the converted document and nothing else. A Finale record locator (`details/chordAssign cmper1=3 cmper2=12 inci=0`) is unusable without reimplementing `musxdom`, so a gap carries only the id of the target object it belongs to (or, for page text, which belongs to no target object, no anchor and only its placements), plus a staff and a position within that object when the feature has no node of its own. The report never carries source coordinates or a copy of the source document. This applies to every location a gap names, placements included: a Finale staff cmper never reaches the report. The exporter, which owns the mapping from Finale staves to target parts and staff numbers, performs the translation when it records the gap; the serializer only writes what it is given, so no payload can take a shortcut through a source identifier. A staff the exporter has no part for is left out of the report and logged.

### Ids come from `core::element_ids`, from a counter only where Finale has no entity

The ids the exporters write (`ev<entry>`, `ev<entry>n<note>`, `<part>.m<cmper>`) derive from Finale identity, so they are stable across runs and across unrelated exporter changes, and the same id names the same entity in every target format. A JSON pointer was rejected as an anchor: it breaks as soon as a consumer inserts into the document, and it makes reference fixtures shift whenever the exporter emits a new element kind. Any object that needs an id for anchoring gets one derived the same way, added to `element_ids`, not minted in an exporter.

An id records Finale provenance, not target semantics; the semantics are already in the target document. An object derived from a measure-expression assignment is therefore `m<cmper>.textExp<def>.inci<n>` (or `shapeExp`), whatever the target made of it, and one derived from a Tempo Tool record is `m<cmper>.tempoDef.inci<n>`. A staff-list group, which is several assignments for one marking, takes the inci of the member that names the group (see `classify::ExpressionAssignmentGroup`).

A target object with no Finale counterpart has no provenance to derive an id from, and a counter is the only choice. The MNX part is one: Finale has no part entity (a Finale "part" is a linked part, which MNX writes as a score), an MNX part is one per instrument or per split instrument, and multi-instrument performers will make it a collection of such parts. Its id (`P1`, `P2`, …) is therefore numbered in the order the exporter writes parts, and it can change when an option such as instrument splitting changes which parts are written. Ids built on a part id, such as `<part>.m<cmper>`, are stable only as far as the part id is. Such an id is minted by the exporter, never derived elsewhere from Finale identity.

The serializer may take responsibility for converting a Finale identity to a target id itself, but only where the id depends on nothing but Finale provenance. A counter-minted id never qualifies, so a location that involves a part, such as a staff placement, is always supplied by the exporter. Today the exporter supplies every target id in the report.

### An expression gap anchors where the target put the expression

A tempo mark that MNX exported as a tempo is anchored to that tempo's id: the object exists, and the gap is what it lacks. An expression of a type MNX treats as global (tempo family, tempo alteration, rehearsal mark; see the MNX design decisions) is anchored to the global measure with a position, once per staff-list assignment group. A staff expression exported as positioned text is anchored to that object, with partial gaps for formatting, inserts, and any specialized semantics general text cannot express. A staff expression with no exported object is anchored to the part measure with its staff and position, one gap per staff it is assigned to. The choice follows the exporter's own routing of the type, not how Finale distributed the assignment.

### A floating staff-list member of a staff expression is a gap at the global measure

A staff expression is exported on each staff its staff list names (see the MNX design decisions). A member that floats to the bottom staff of every system names no staff, and MNX has no system-level object, so it would otherwise vanish. The group is reported once, as a complete gap anchored to the global measure with the group's position, and its placements list only the floating members. The members on real staves are already in the document as positioned text, so a consumer that draws the gap draws the floating copies alone and draws nothing twice. A group with a floating top-staff member never reaches this path: the classifier treats such a group as system text, a tempo mark when nothing more specific fits, and the global path reports it with every member's placement.

### Page text is one gap per assignment, placed on every page of every score that shows it

MNX has no page text, so a Finale page-text assignment is a complete gap. The MNX exporter writes one score per linked part, but a linked part shares its page text with the score. The only values a part can unlink are the offsets, whether right-hand pages are positioned independently with their own offsets, and whether the text shows. The assignment is therefore one gap however many scores show it, keyed by its identity, rather than one per score: a title shared by forty linked parts is one gap, not forty copies of it.

The gap has no anchor. Page text belongs to the document, not to any score, and its pages already say everywhere it is drawn, so an anchor would only make one score look like its owner. This is the one kind of gap without an anchor. Its payload is the text, frame, page assignment and alignment, which every score shares, and the position as the full score has it, whether or not the full score shows the text; hiding it is a matter of visibility, which the placements express. Its placements list every exported page that shows it, in every score, each by page id. A part that hides the text contributes no placements. The payload's `position` holds exactly the unlinkable values other than visibility: `offsetX` and `offsetY`, `independentRightPages`, and the right-page offsets when that is set. A part that changes any of them gives its placements their own `position`, written only where it differs from the payload's, so text the parts leave alone costs one line per page and nothing more. Right-page offsets that no page uses do not count as a difference. The alignment, including the right-page horizontal alignment, is shared, so it is stated once and never in a placement.

Where page text is drawn is a fact about the source, so the placements belong to the classification, which both exporters use: each names a linked part, one of its pages, and the part's own assignment, whose unlinkable values say where that part puts the text. The classification holds the full score's assignment too and copies none of its fields: what it adds is the text, the frame and the placements, which take interpretation, while the page assignment, alignment and offsets are the assignment's own and the serializer reads them from it. They cover every page in the part's page list, whether or not its layout was calculated, which is the range MusicXML writes credits for. The classification is never trimmed. `PageTextGap` adds the target side: one entry for each placement the MNX exporter wrote a page for, giving that page's id and the index of its placement, whose position applies. A placement with no exported page, such as one in a part whose layout was never calculated, has no entry, so the report names only pages the document has. A placement says that Finale assigns the text to the page, not that it renders to something there: a part-name insert in a part with no name still places the text, and the consumer, which resolves inserts for each score, draws nothing.

### Page text states Finale's page assignment ids, not one score's page numbers

A resolved first and last page would be one score's numbers, misleading for every score with a different number of leading blank pages. The payload therefore states Finale's own rule, as `assignment`: `start` and `end` and the parity `pages` (`all`, `odd` or `even`). A single-page assignment has the same `start` and `end`; a range whose `end` is 0 runs through the last page, whatever it is, so it grows with the document.

`start` and `end` are page assignment ids, which are page numbers only where no score has leading blank pages. Let M be the most leading blank pages in any score of the document, written once as the report's `leadingBlankPageSlots` and absent when it is 0, and let B be the number a given score has: the pages before its first page with a system in the target document. Ids 1 to M address the leading blank pages, aligned from the front, so id a ≤ B is that score's page a and an id with B < a ≤ M names no page in it. Id M + k is the kth page after the leading blank pages in every score, page B + k of a score with B. A range that starts on an id naming no page in a score starts there on page B + 1, provided it reaches that far. Parity applies to each score's own page numbers, odd pages being right-hand pages, not to the ids.

M is one value for the document, not for an assignment: an assignment stores no slot count of its own, its ids are linked, and `musxdom` converts every assignment with the one maximum it computes from the current page lists. A consumer that keeps Denigma's pagination draws from the placements, which are the rule already resolved for each score; one that reflows the pages applies the rule again. A part whose layout was never calculated exports no pages, so it contributes no placements.

The text is reported once, as a template rendered for the full score. An insert that varies by page or by part, such as the page number or the part name, is a run that records the insert, so a consumer resolves it for each page and score rather than receiving a copy of each.

### Measure text keeps its frame in a partial gap

A measure-attached text block is exported as positioned text, and what positioned text cannot carry is reported against it: the formatting and inserts through the usual text gaps, and the frame and the handle's offsets through a `text-block` gap. A text block that Finale holds at a fixed distance from the barline rather than at a metric position has no MNX position, so that distance is part of the gap.



A smart shape runs from one place to another, and a consumer that draws it needs both. Its payload therefore carries an optional `end`, an anchor like the first: for an entry-attached shape the event, or the note when the shape names one, that it ends on; for a beat-attached shape the part measure, staff and position of its end point. An end whose entry the target did not export falls back to the measure form, so the report never names an id the document lacks. The end is a second anchor rather than a placement because a placement says where one feature is drawn again, while an end says where the same drawing stops.

### A feature drawn in several places is one gap with placements

A Finale staff list draws one marking on several staves, one assignment per staff. Reporting each assignment would report one marking several times, and dropping all but one would lose where it is drawn. The gap is recorded once, anchored as above, and its expression carries `placements`: the target-side places it would be drawn (`system-top`, `system-bottom`, or a part measure with a staff), for the members the gap lists. Which side of its staff each member is drawn on (`above`, `below`, or `float`) and whether it floats are source facts, so the group's classification lists every member with its scope, staff and side. `ExpressionGap` pairs the members the gap lists, by index, with their target places, which the exporter translates as the anchor rule requires; a placement writes the member's side when the source states one, because a system-top marking can be drawn below the top staff and the anchor alone does not say so. Page text writes `page` placements of the same shape inside its own payload, one per page it appears on, as described above.

## Serialization

### One `schemaVersion` on the report

Gaps carry a `type` string and an `extent` and nothing else by way of taxonomy. Per-gap cause codes and per-payload version numbers were dropped as answering questions no consumer had asked; the report-level `schemaVersion` covers compatibility until a second dimension matters.

### A gap states its extent

A gap is `complete` when nothing in the target stands for the feature, and `partial` when the anchored object stands for it but lost the reported payload: a tempo without its text, a tempo that should not be displayed. A consumer that renders gaps needs the distinction to know whether to draw something new or to decorate something already there, and the anchor alone does not tell it, since both kinds can anchor to an object.

### Report schema version 3

Version 3 moves what only some payloads need into those payloads: a smart shape's `end` into `smartShape`, an expression's `placements` into `expression`, and the text ranges, renamed `ranges`, into `formatting` and `insert`. A gap's own keys are now only its anchor (`anchor`, with `staff` and `position`), `extent`, `type` and the payload. Version 3 also adds the `text-block` and `page-text` payloads, the report-level `leadingBlankPageSlots` that page assignment ids are relative to, `page` placements inside `pageText`, the optional `placement` side on expression placements, and the report-level `fonts` table. It coincides with MNX scores and pages carrying ids, which the `page` placements name. A page-text gap carries no `anchor`, so a consumer cannot assume every gap has one.

### Report schema version 4

Version 4 adds the report-level `fretboards` table and the optional `fretboard` key on a chord-symbol payload that refers into it.

### Fonts are written once, in a report-level table

Every text run names its font, and a document uses a handful of fonts across thousands of runs, so writing the font object at each run made fonts most of a text-heavy report. The report therefore carries a `fonts` table, and a run, a formatting gap, a notehead or a line character names its font by key. The table follows the arrowhead table: the key (`<n>`, numbered from 1 in order of first use, with no prefix because it always sits in a field named `font`) is minted by the serializer, names nothing in the target document, and is opaque to a consumer.

### Fretboards are written once, in a report-level table

In Finale a fretboard belongs to the chord assignment: the chord shows it or does not, and the diagram comes from a fretboard group shared by every chord on the same root. A fretboard is therefore not a gap of its own. The chord-symbol gap names it with a `fretboard` key, and the report's `fretboards` table holds each distinct fretboard once. The table follows the fonts table. The key (`<n>`, numbered from 1 in order of first use) is minted by the serializer, names nothing in the target document, and is opaque to a consumer. As with fonts, fretboards that serialize identically share a key, so no source identifier reaches the report.

A `diagram` entry is `musx::util::FretboardDisplayData` as Finale stores it, not as any target format would carry it. Its cells keep Finale's order, its shapes (`closed`, `open`, `muted`, `custom`, `none`), and every cell on a string, and each barre keeps its own fret and string range. MusicXML's frame, for one, marks a barre on its two endpoint notes and so has no place for a barre whose endpoint string is stopped higher; the report states the barre as Finale does, and how a target draws it is the importer's decision. The display data's unplayed-string summary is derived from the cells and is not written.

A `font` entry is a fretboard Finale draws from a character in its fretboard font, recognized from the chord assignment's own flags as in the MusicXML exporter. It has no fields, because musxdom does not expose which character Finale draws, and the font alone would not let a consumer draw it.

### Text-block lengths are EVPU and state their origin

Every length in a `text-block` or `page-text` payload is in EVPU (288 per inch, 24 per staff space) at 100% scaling, and each payload says so with `unit`. Values Finale stores in Efix (inset, border thickness, corner radius) are converted to fractional EVPU, so one unit covers the payload. Vertical values are positive upward, as Finale's are. A measure text's vertical offset is measured from its staff's top line (`verticalOrigin`), as the MusicXML exporter measures it; its horizontal offset is from the start of the measure. Page text is aligned within either the page or the page inside its margins, separately for each axis (`horizontalFrom`, `verticalFrom`), and then displaced by its offsets; when Finale positions right-hand pages independently, the payload carries a second position for them. Font sizes are relative to Finale's 96-EVPU staff, as for all text gaps.

Only the standard frame is described. A text block that uses a Shape Designer shape instead says so with `hasCustomShape` and nothing more.

### Arrowheads are embedded once, as SVG, keyed by a report-internal reference

A custom line's cap may be a Shape Designer arrowhead, which a consumer could not draw from Finale's instruction list without reimplementing Shape Designer. The report therefore embeds the arrowhead as SVG, rendered by `musxdom`'s `SvgConvert` at serialization time, in a report-level `arrowheads` table that each cap references by key. The same shape caps many lines in a typical document, so the table holds each once. Preset arrowheads are embedded the same way so that a consumer needs no knowledge of Finale's presets either.

The key (`custom-<cmper>`, `preset-<name>`) is minted by the serializer from the arrowhead's identity in the source and is the one place the report carries a source identifier. It names nothing in the target document, so the anchor rule does not apply to it; it exists only so that caps can share an image, and a consumer treats it as opaque.

The image's coordinates are staff spaces at 100% staff size (`SvgConvert::toSvg` with a scaling of `1 / EVPU_PER_SPACE` and no unit suffix), because a notation renderer thinks in spaces and scales a marking with its staff. The `viewBox` is the shape's own bounds in that space, so the shape's origin, which Finale places at the end of the line, stays recoverable; the entry states `unit` and `origin` so a consumer need not know this. Text drawn inside an arrowhead shape is measured through the `GlyphMetricsFn` the front end passes in `GapReportOptions`, or sized heuristically when there is none.

### Reference fixtures are named per conversion

`chords.mnx.gaps.json` records the gaps of the MNX conversion of `chords.musx`. The payloads and anchors are format-neutral, but which features are gaps depends on the target, so a MusicXML report of the same file would differ and would carry its own name.

### Text ranges name content elements and use element-local units

Report schema version 2 adds `text-formatting` and `text-insert` payloads, and optional `textRanges` on a gap. Each range has `anchor` (the output content element's id), `offset`, and `length`. Its half-open interval counts Unicode scalar values for text or glyph-list entries for SMuFL content. The gap's main anchor is the owning text object. Ordinary gaps omit `textRanges`. Version 3 moves them into the payload as `ranges`.

Formatting contains `font`, `baseline`, `superscript`, `tracking`, and `sourceText`. Inserts contain `kind`, `command`, `parameters`, and `sourceText`. Formatting gaps coalesce identical adjacent formatting within an output element, while each emitted insert chunk remains a separate replacement operation. Both are partial gaps, and their ranges may overlap. Range coordinates describe the original document; consumers must resolve and maintain their own positions through edits.

Text content ids are an exception to deriving every id directly from one Finale object: several canonical content elements can derive from one source chunk, and one element can combine several chunks. They use the expression id plus `.text<ordinal>`, where the zero-based ordinal follows the final minimal text/glyph sequence. These ids are deterministic across conversions and independent of gap collection, but changing that sequence can change its child ids. No JSON pointer or document-wide counter is used.
