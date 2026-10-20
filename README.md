# denigma command line interface (cli) utility

This utility extracts and converts Enigma XML from Finale `.musx` files and imports legacy Finale binary scores. It is a successor to the original denigma project by [Chris Roode](https://github.com/chrisroode).

**This project is not affiliated with or endorsed by Finale or its parent company.**

- It is an independent open-source utility designed to help users access and convert their own data in the absence of Finale, which has been discontinued.
- It does not contain any Finale source code.
- It is not capable of writing Finale files, only reading them.
- Nothing in this repository circumvents digital copy protection on the Finale application.

## Library layout

Denigma is split into small reusable libraries as well as the CLI utility:

- `denigma_classify` provides independent classification helpers for clefs, articulations, dynamics, and jumps.
- `denigma_format_enigmaxml` handles MUSX archive extraction and EnigmaXML pass-through.
- `denigma_format_mnx` converts MUSX content to MNX.
- `denigma_format_mss` converts MUSX content to MSS.
- `denigma_format_musicxml` converts MUSX content to MusicXML.
- `denigma_format_svg` converts MUSX content to SVG.

The documentation site for the library API is [https://openmusx.github.io/denigma/](https://openmusx.github.io/denigma/).

## Linking

Denigma can also be consumed as libraries from another CMake project. Link only the narrow library or libraries you need:

```cmake
target_link_libraries(my_tool PRIVATE denigma::classify)
target_link_libraries(my_tool PRIVATE denigma::gap-report)
target_link_libraries(my_tool PRIVATE denigma::mnx)
target_link_libraries(my_tool PRIVATE denigma::mss)
target_link_libraries(my_tool PRIVATE denigma::musicxml)
target_link_libraries(my_tool PRIVATE denigma::svg)
target_link_libraries(my_tool PRIVATE denigma::enigmaxml)
```

Use `denigma::classify` when you only need classification helpers. Use one of the format targets when you need a specific converter.

Applications may register linked formats and call `ConverterRegistry::convert` when runtime format selection or owned output buffers are useful:

```cpp
#include "denigma/formats/mnx.h"

denigma::ConverterRegistry registry;
denigma::formats::mnx::registerConverters(registry);

denigma::BufferRandomAccessReader input(musxBytes);
denigma::formats::mnx::Options options;
options.common.sourceName = "score.musx";

auto artifact = registry.convert(
	denigma::FormatId::Musx,
	denigma::FormatId::MnxJson,
	input,
	denigma::ConversionRequest{ &options });
```

`ConversionArtifact` owns the generated documents in emission order and preserves the converter's `ConversionResult`. Multi-output converters retain each suggested filename. The format-specific typed converters and their `convert` overloads remain a first-class alternative.

### Conversion gap reports

Gap collection is opt-in. Supply a `classify::GapCollector` (from `denigma/classify/gaps.h`, part of `denigma::classify`) through `CommonOptions::gapCollector`, then inspect its typed gaps directly or serialize it with the separate `denigma::gap-report` library. Exporters depend only on the collector; none of them links the serializer, so a project that consumes a converter without wanting reports never pulls it in.

```cpp
#include "denigma/gap_report.h"

denigma::classify::GapCollector gaps;
denigma::formats::mnx::Options options;
options.common.gapCollector = &gaps;

auto result = denigma::formats::mnx::MusxToMnxJsonConverter{}.convert(input, output, options);
auto report = denigma::serializeGapReport(gaps, { { "my-app", "1.0", "abc123" }, {} });
```

When `gapCollector` is null, no gaps are classified or stored. The exporter hands the collector its parsed source document, which the collector keeps alive until it is destroyed, so a report can be serialized after the conversion returns. Serialization always produces a report, including an empty `gaps` array when nothing was collected. The second member of `GapReportOptions` is an optional `denigma::GlyphMetricsFn`; the CLI passes its FreeType-backed text measurer so that text inside an embedded arrowhead image is sized exactly, and a front end without one (the WebAssembly module) gets heuristic sizing.

Chord symbols, notehead shapes, expressions, and smart shapes are the gap types so far. Chords anchor to a part-measure ID plus position and optional staff. Noteheads anchor directly to the stable note ID already present in MNX. A smart shape MNX cannot carry (a custom line, a glissando, a trill or vibrato line, a keyboard pedal line, a beat-attached slur, a bend) spans from its `anchor` to an `end`: an entry-attached shape names the events or notes it runs between, and a beat-attached shape names part measures with positions. Its payload is the classified shape, and the caps of a line that carry an arrowhead reference a report-level `arrowheads` table of SVG images drawn in staff spaces with the shape's origin at the line end.

```json
{
  "schemaVersion": 1,
  "producer": { "name": "denigma", "version": "4.0.0", "commit": "..." },
  "gaps": [{
    "type": "chord-symbol",
    "anchor": "P1.m1",
    "position": { "numerator": 0, "denominator": 1 },
    "chord": {
      "root": { "step": "C", "alteration": 0 },
      "rootLowerCase": false,
      "showRoot": true,
      "showSuffix": true,
      "suffix": {
        "strings": [{ "text": "m7", "position": "inline" }],
        "suffixText": "m7",
        "quality": "minor-seventh",
        "degrees": []
      }
    }
  }]
}
```

Serialized reports use these compatibility rules:

- Consumers should dispatch on `type` and ignore unknown fields and gap types.
- Anchors are MNX object IDs and remain valid as consumers modify surrounding arrays.
- Consumers should resolve chord positions before changing measure timing.
- A gap with an `end` spans from its anchor to that end; `end` has the same shape as the anchor fields.
- `arrowheads` keys are report-internal references, not MNX object IDs.
- Split-instrument export reports a chord once in each generated MNX part that contains its source staff, with the corresponding part-measure anchor.

The WebAssembly API opts into collection and exposes serialized report bytes through `denigma_result_gap_report_data` and `denigma_result_gap_report_size`. CLI users can pass `--gap-report` with MNX output to write `<output>.gaps.json` beside the score.

The serializer library is built by default and can be excluded with `denigma_BUILD_GAP_REPORT=OFF`. In such a build the CLI warns and ignores `--gap-report`, and the WebAssembly accessors return an empty report, while every converter and the collector remain available. Code that must compile either way serializes through `denigma::withGapReport(collector, [&]<typename Writer>(const Writer& writer) { ... writer.serialize(producer) ... })`, which runs the callback only when the serializer is present; `denigma::GAP_REPORT_AVAILABLE` says which.

Gap payloads are not intended to duplicate every missing exporter feature. Features that the target standard can represent should normally be implemented directly in that exporter or its dependency. Typed payloads are reserved for durable target-format limitations with a demonstrated downstream recovery use case.

The companion [denigma-examples](https://github.com/openmusx/denigma-examples) repository demonstrates this from separate native and WebAssembly projects using CMake `FetchContent` or a local Denigma checkout.

When Denigma is added as a CMake subproject, its CLI and tests are disabled by default. Consumers can override either option before making Denigma available:

```cmake
set(denigma_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(denigma_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(denigma_BUILD_GAP_REPORT OFF CACHE BOOL "" FORCE) # optional: omit the gap report serializer

include(FetchContent)
FetchContent_Declare(
	denigma
	GIT_REPOSITORY https://github.com/openmusx/denigma.git
	GIT_TAG        <release-tag-or-commit>
)
FetchContent_MakeAvailable(denigma)
```

Standalone Denigma builds continue to build the CLI and tests by default.

## Command line usage

Use the `--help` option to get a full list of commands:

```
denigma --help
```

## Setup instructions

Clone the GitHub repository and clone all submodules.

### macOS-Specific

Install the latest cmake:

```bash
brew install cmake
brew install ninja
```

---

### Windows-Specific

You must install cmake and xxd. The easiest way is with a package manager such as Choclatey (`choco`).

[Choclatey install instructions](https://chocolatey.org/install)

Install the latest cmake and xxd

```bat
choco install cmake
choco install ninja
choco install xxd
```
---

## Build instructions


```bash
cmake -P build.cmake
```

or (for Linux or macOS)

```bash
./build.cmake
```
---

You can clean the build directory with

```bash
cmake -P build.cmake -- clean
```

or (for Linux or macOS)

```bash
./build.cmake -- clean
```

## WebAssembly build

Denigma also builds a WebAssembly module exposing MUSX and legacy MUS inspection and conversion to EnigmaXML, MusicXML, and MNX through a small C ABI (`src/wasm/denigma_wasm.cpp`). It is the module that [denigma-online](https://github.com/openmusx/denigma-online) runs in the browser. Building it requires [Emscripten](https://emscripten.org/) (CI uses 5.0.7):

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release -DDENIGMA_CXX_STANDARD=20
cmake --build build-wasm --target denigma_wasm
node tests/wasm/smoke.mjs build-wasm/wasm/denigma.js build-wasm/wasm/denigma.wasm
```

Build the `denigma_wasm` target rather than `all`: the module links only the EnigmaXML, MusicXML, and MNX converters, and naming the target keeps the text-measuring converters (SVG and MSS) and their font dependencies out of the build. Under Emscripten the CLI and tests are off by default and `denigma_BUILD_WASM` is on. The module lands in `build-wasm/wasm/` as `denigma.js` and `denigma.wasm`; CI builds and smoke-tests the module on every pull request, uploads the pair as the `denigma-wasm` artifact of every push to `main`, and attaches it to every published release as `denigma.<tag>.wasm.zip` alongside the native binaries.

## Legacy Finale MUS input

Legacy MUS scores are recognized from their contents, regardless of filename or extension. CLI export accepts explicit paths and wildcard matches; directory inputs examine all regular files, with existing recursion and folder exclusions. Unrelated files are skipped. A recognized MUS score takes precedence over an extension such as `.musx` or `.enigmaxml`.

Legacy input defaults to MusicXML. MusicXML, MNX, MSS, and SVG use the existing exporter options and limitations (SVG exports shape definitions). EnigmaXML and MUSX output are unavailable for legacy input because musxdom does not serialize documents to EnigmaXML.

```bash
denigma old-score
denigma archive --recursive --musicxml converted
```

The optional CLI resource flags are `--mus-symbol-fonts path`, `--mus-percussion-map path`, `--mus-percussion-conversion-table path`, `--mus-soundmap path`, and `--mus-instrument-catalog path`. Repeat percussion maps and soundmaps in the desired order. Paths resolve from the working directory. The percussion map's basename identifies it to the conversion table. No Finale installation is located automatically; absent resources use reader defaults. An explicit `massage --finale-file` companion also uses content recognition; automatic companion lookup retains its existing MUSX/EnigmaXML filename search.

Native callers use `finale_mus_reader::recognize(bytes)` from `finale_mus_reader/reader.h` for recognition and link `finale_mus_reader::finale_mus_reader`. Each document exporter registers an adapter accepting `FormatId::Mus` and a memory span. For optional resources, include `denigma/mus_resources.h` and link `denigma::input`. `MusResources::create(resources)` parses named buffers once; put the returned shared handle in `options.common.musResources`. Input resource buffers can be released when creation returns. Singleton resource kinds reject duplicates; percussion maps and soundmaps preserve order and reader matching behavior.

Denigma supplies the reader's musxdom and zlib targets and builds it without instrumentation. `FINALE_MUS_READER_LOCAL_PATH` selects a local checkout. The fetched reader revision includes per-import source identity and part voicing, plus diagnostic formatting compatible with the macOS deployment target.

### WASM recognition and resources

Existing `denigma_inspect` and `denigma_convert` signatures also accept recognized MUS bytes with any source name. Existing output codes stay unchanged; requesting EnigmaXML for MUS returns an error result.

`denigma_recognize_mus(data, size)` returns `0` for unrelated input, `1` for a candidate legacy score, `2` for Finale input outside legacy score containers (including MUSX, ETF, and libraries), `3` for a damaged legacy container, and `-1` for a null pointer with nonzero size. Empty input is unrelated. Recognition does not import, decompress, or require resources; a candidate can still fail during import.

Resource functions use opaque caller-owned handles:

- `denigma_mus_resources_create()` returns an empty handle, or null on allocation failure.
- `denigma_mus_resources_add(handle, kind, name, data, size)` copies a named buffer. Kinds are `0` symbol fonts, `1` percussion mapping XML, `2` percussion conversion table, `3` soundmap XML, and `4` instrument catalog XML. Percussion maps require a name. Buffers must be nonempty.
- `denigma_mus_resources_finalize(handle)` validates and parses the resources once, freezes the handle, and releases the copied input buffers. Repeated successful finalization is harmless. A failed finalization leaves the handle unfinalized; destroy and recreate it to replace invalid resources.
- `denigma_mus_resources_destroy(handle)` releases a handle; null is harmless.

Add and finalize return ordinary result objects: check `denigma_result_success`, read diagnostics, and call `denigma_result_destroy`. Adding to a finalized handle fails. The empty finalized handle selects reader defaults.

`denigma_inspect_with_resources` and `denigma_convert_with_resources` take the same arguments as their existing counterparts, followed by the resource handle. A null handle uses defaults; a nonnull handle must be finalized. The caller retains ownership and may reuse a handle across files and conversions. Each operation constructs its own document, so differing resource handles and part-voicing settings do not share document state.

Resource acquisition belongs to the client: these functions accept bytes from uploads, drag-and-drop, network fetches, or another source. This integration does not change denigma-online's UI or choose its resource acquisition policy.

## Visual Studio Code setup

See [`.vscode_template/README.md`](.vscode_template/README.md) for OS-specific templates (`macos`, `linux`, `windows`) with `launch.json` and `tasks.json`.
