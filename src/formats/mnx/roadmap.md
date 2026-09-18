# MNX Roadmap

Deferred MNX exporter work. An item here is not a `@todo` in the code; see the editing rules in `AGENTS.md`.

## Host beat-attached slurs

A slur Finale attached to beats rather than to entries is reported as a gap. When both of its endpoints coincide with entries (`smartshape::EndPoint::calcAssociatedEntry`), it could be exported as an MNX slur between those events, as the MusicXML exporter does; the gap would then remain only for a slur with a floating endpoint. `slurs_beatattached.musx` is the fixture.

## Measure glyphs for gap-report images in the WebAssembly module

The gap report embeds arrowhead SVGs, and text drawn inside such a shape is sized heuristically in the module because `denigma_textmetrics` (FreeType plus the platform font resolvers) is not linked there. `denigma::GlyphMetricsFn` (an alias of `musx::util::SvgConvert::GlyphMetricsFn`) is the seam: the module could pass a callback that reaches an optional `Module.denigmaMeasureText` hook through `EM_JS`, with no new exports and no function-table growth, and the hook would measure with canvas `measureText` (synchronous, and `OffscreenCanvas` works in workers, which the module targets). The hook must return null when `document.fonts.check()` fails, because canvas substitutes a font silently and the substitute's metrics are worse than the heuristic; Node has no canvas, so the smoke test would exercise only the fallback. Text inside an arrowhead is rare, which is why this is deferred.
