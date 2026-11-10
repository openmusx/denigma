# Classification roadmap

## Parenthesized noteheads

Recognize a parenthesized notehead only when both a left and a right parenthesis
can be reliably associated with the same note. Individual parenthesis glyphs do
not establish that semantic classification and remain unclassified articulations.

Before implementing pairing, validate articulation handle coordinates independently
in Finale and establish symbol-specific alignment relative to noteheads. Reject
ambiguous chord associations, unmatched sides, and pairs enclosing a chord rather
than one note. Handle coordinates alone are not sufficient to identify the note.
Keep this analysis in the shared note-level classification layer, with export
format decisions in the exporters.

## Unmapped Finale AlphaNotes characters

smufl-mapping's legacy mapping for Finale AlphaNotes stops at character 119, so later characters
classify as unclassified noteheads and MNX reports each Finale notehead override that uses one as a gap.
Finale's sample files use two of them as note overrides: 130 (a surprised face) in the Primer
*Jingle Bells* and 141 (a smiling face) in every part of *Frankenstein's Monster Goes Jogging*. Once
smufl-mapping maps the rest of the font, bump its pin. Characters that receive a SMuFL glyph name then
export as custom noteheads with no further Denigma change; any that SMuFL cannot name remain gaps,
which is correct.
