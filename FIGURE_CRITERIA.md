# Figure-editing criteria (comprehensive)

## Font handling
1. **font 132 ONLY for math variables** — never for words in axis titles or legends. Use `#font[132]{#it{var}}` explicitly.
2. Global text font = **42 (Helvetica)**, not 132.
3. **Font sizes are dynamic** (scale with pad size) — pick absolute values that render at visually similar sizes across the figure. Titles ~1.2× labels, labels ~1.2× tick labels, legend text ~ tick label size.
4. **Similar rendered sizes** for related text (titles ↔ titles, labels ↔ labels, legend ↔ legend) within a figure.

## Layout
5. **Uniform panel margins** across all panels in a multi-panel figure.
6. **Pad proportions** — don't make pads short and wide unless there's a real reason (e.g. wide legends). Default to ~4:3 or square.
7. **No title/legend overlap** — panel title must not collide with the legend strip.
8. **No axis title/label overlap** — left margin ≥ 0.18 for y-titles, title offset ≥ 1.30.
9. **Legends in the top margin** (transparent), entirely above the plot area (i.e. above the frame top, not overlapping the topmost tick label or data).
10. **Multi-column legends** when there are ≥3 entries (not a single row with too many columns).
11. **No excessive whitespace** between legend, title, and plot axes — top margin should be just enough for the legend + title.

## Annotations
12. **TLatex inside the pad** (in the plot area), not floating in the top margin.
13. **TLatex inset from pad edges** (NDC x ≤ 0.85, NDC y ≥ 0.15) so text doesn't touch the frame.
14. **TLatex positioned to avoid data curves and reference lines** — choose a clear region (e.g. lower-right corner if curves decay there).
15. **Readable TLatex size** (~0.045–0.050 of pad height).

## Verification
16. **After regenerating, verify data is actually shown** (not empty axes) — read the PNG back.
17. **Check for overlap** between: legend ↔ frame, title ↔ legend, title ↔ frame, TLatex ↔ curves, TLatex ↔ reference lines, TLatex ↔ axis labels.
18. **Run the full build pipeline** (regenerate figures → rebuild PDF) to catch issues in context.
19. **Data integrity** — if a figure is empty, check the data files (corruption, wrong path) before assuming a layout bug.
