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
