# Component artwork

The SVG originals in this directory are bundled into the app; no image requests
or network connection are needed at runtime. `sources.json` records the exact
upstream revisions, URLs, and file hashes.

- Gate, source, signal, and open/closed relay images: [Electronic Symbols](https://github.com/chris-pikul/electronic-symbols), MIT.
- Wire route, screen, and file-port images: [Lucide](https://lucide.dev), ISC.
- Crossing: Gatehaven's insulation-gap drawing, kept consistent with the live
  crossing paths on the canvas.

License notices are in `third_party/electronic-symbols` and `third_party/Lucide`
and travel with installed packages. The source SVGs are unchanged. The atlas
generator normalizes stroke widths, trims empty margins, preserves aspect ratios,
and embeds transparent masks. Six filtered texture levels keep the images clear
on ordinary and Retina displays. Live wires retain their connection-aware paths;
relay contacts switch between the two source images, with a polarity indicator.

To regenerate after changing an SVG, install `CairoSVG==2.8.2` and `Pillow==12.3.0` in a
development environment and run `python tools/build_symbol_atlas.py`. Ordinary
builds consume the committed `src/app/symbol_atlas.inc` and need neither package.
