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

### Classifications are consumed before the source document is released

Some classification values keep `musxdom` instances whose accessors resolve through the document (a `FontInfo` name, for instance). A collector is therefore serialized, or otherwise read, while the `DocumentPtr` is alive. Making the classification types document-independent is future work; until then the rule is stated on `GapCollector`.

## Anchors

### A gap is anchored by the target object's id, not by a source locator

A consumer of the report has the converted document and nothing else. A Finale record locator (`details/chordAssign cmper1=3 cmper2=12 inci=0`) is unusable without reimplementing `musxdom`, so a gap carries only the id of the target object it belongs to, plus a staff and a position within that object when the feature has no node of its own. The report never carries source coordinates or a copy of the source document.

### Ids come from `core::element_ids`, never from a counter

The ids the exporters write (`ev<entry>`, `ev<entry>n<note>`, `<part>.m<cmper>`) derive from Finale identity, so they are stable across runs and across unrelated exporter changes, and the same id names the same entity in every target format. A JSON pointer was rejected as an anchor: it breaks as soon as a consumer inserts into the document, and it makes reference fixtures shift whenever the exporter emits a new element kind. Any object that needs an id for anchoring gets one derived the same way, added to `element_ids`, not minted in an exporter.

## Serialization

### One `schemaVersion` on the report

Gaps carry a `type` string and nothing else by way of taxonomy. Per-gap cause and representation codes, and per-payload version numbers, were dropped as answering questions no consumer had asked; the report-level `schemaVersion` covers compatibility until a second dimension matters.

### Reference fixtures are named per conversion

`chords.mnx.gaps.json` records the gaps of the MNX conversion of `chords.musx`. The payloads and anchors are format-neutral, but which features are gaps depends on the target, so a MusicXML report of the same file would differ and would carry its own name.
