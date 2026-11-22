# Classification roadmap

## Parenthesized notehead glyphs

A pair of delimiting articulations is classified as `articulation::BracketEnclosure`. Still undecided is what to do
with a notehead glyph that draws its own parentheses, such as `noteheadBlackParens`. These are optional glyphs of
Finale's SMuFL fonts (Bravura has them too), at private-use code points, not standard SMuFL glyphs. Today such a glyph
classifies as `notehead::Shape::Other` with its glyph name, so MusicXML writes it as
`<notehead smufl="…">other</notehead>`, and MNX writes it as a `custom` notehead naming the glyph.

There are two ways forward:

- Classify the glyph in `classifyNotehead` as a parenthesized notehead of its base shape, so that exporters write the
  regular shape with parentheses. Any reader can draw that.
- Keep exporting the glyph name. It draws exactly what Finale drew, but only for a reader whose font has Finale's
  optional glyphs. Other programs and fonts do not recognize the name, so they draw an `other` notehead, and MNX does
  not currently support non-standard glyphs at all.

The choice also depends on what an enclosure covers, which neither format has settled in merged text. A
parenthesized notehead glyph encloses only the notehead. The MNX enclosure proposed in w3c-cg/mnx#569 surrounds the
notehead, its augmentation dots, and any displayed accidental, and is defined for notes but not rests. Whether
MusicXML's `<notehead parentheses="yes">` encloses the accidental is the question of w3c-cg/musicxml#735. Wait for
both before deciding. A `BracketEnclosure` does not yet record whether its delimiters enclose the accidental, since
the opening delimiter only has to lie left of the notehead.

A related open question is how to tell a pair of delimiters enclosing a whole chord from one enclosing one of its
notes. Handle positions alone may not settle it, so for now a note entry's enclosure always encloses one note.

## Unmapped Finale AlphaNotes characters

smufl-mapping's legacy mapping for Finale AlphaNotes stops at character 119, so later characters
classify as unclassified noteheads and MNX reports each Finale notehead override that uses one as a gap.
Finale's sample files use two of them as note overrides: 130 (a surprised face) in the Primer
*Jingle Bells* and 141 (a smiling face) in every part of *Frankenstein's Monster Goes Jogging*. Once
smufl-mapping maps the rest of the font, bump its pin. Characters that receive a SMuFL glyph name then
export as custom noteheads with no further Denigma change; any that SMuFL cannot name remain gaps,
which is correct.
