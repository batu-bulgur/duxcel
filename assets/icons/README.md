# Application identity

`duxcel.svg` is the canonical vector reconstruction of the **upper symbol only** in the user-approved `output/logo-concepts-2026-10-16/05-modular-grid-duck.png`. It preserves the rounded white head, modular white neck/body, blue/green spreadsheet cells and yellow bill. The wordmark and presentation background are excluded. All artwork is paths, rectangles and a circle with solid fills and a transparent surround.

`tools/generate_icons.py` uses only Python 3's standard library. It flattens the controlled SVG subset, triangulates each contour, derives C geometry and rasterizes the same outlines offline. It does not embed or trace the reference bitmap. `data-optical` attributes preserve grid and neck gaps at small physical sizes; the generated C transform applies the same spacing rules. The SVG remains the sole geometry/color source.

- `png/duxcel-SIZE.png`: 23 straight-alpha, sRGB RGBA sizes from 16 to 1024px, including common 125/150/200% Windows sizes.
- `duxcel.iconset/` and `duxcel.icns`: standard 1x/2x macOS representations, including 512 points at 2x. `Info.plist` registers `duxcel.icns` in the bundle's Resources directory.
- `duxcel.ico`: multiple 32-bit PNG representations through 256px. The Windows make branch embeds `packaging/windows/duxcel.rc` when an existing `windres` is available; runtime window icons also use the generated PNGs. No Windows compiler is installed by this task.
- `packaging/linux/icons/hicolor/`: common sizes at 1x/2x and the identical scalable SVG. `packaging/linux/duxcel.desktop` references `dev.duxcel.desktop`. These are packaging inputs; no new Linux package format or system installation is performed.
- `src/ui/brand_geometry.h`: generated contours/triangles and physical-pixel spacing. `brand_draw(bounds,pixel_ratio)` draws immediately without parsing, file I/O, textures or heap allocation.
- `src/ui/brand_bitmap.h`: a generated 512px display cache. Settings uses one startup texture with mipmaps, keeping small curved edges antialiased across DPI changes; there is no per-frame decoding or cache allocation.
- `src/platform/brand_images.h`: embedded OS identity. AppKit reads the ICNS for Dock/About in both direct binaries and the bundle. Other platforms decode the PNG set once and call raylib `SetWindowIcons`.
- `manifest.json`: hashes of the source, generator and every generated derivative.

Normal builds use the prebuilt assets with the existing C toolchain. A changed SVG/generator or missing derivative invokes the offline Python generator. Do not edit generated headers or raster files.

```sh
make icons          # regenerate all derivatives offline
make icons-check    # hashes, genuine vectors, PNG dimensions/alpha/sRGB, ICNS/ICO contents
make icons-verify   # regenerate and compare every byte on this toolchain
make mac-icon-check # native AppKit decode and bundle registration, macOS only
make icon-smoke     # native vector/raster contact sheets and Settings screenshots
```

The native contact sheets are in `tests/results/icons/`. Percentages vary icon scale; the recorded host display is Retina 2x. Windows/Linux assets are validated structurally, without claiming native platform execution. The implementation follows [Apple app icon guidance](https://developer.apple.com/design/human-interface-guidelines/app-icons), [Microsoft's icon size/scale guidance](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-construction), the [hicolor icon theme specification](https://specifications.freedesktop.org/icon-theme/latest/) and [raylib's window-icon API](https://www.raylib.com/cheatsheet/cheatsheet.html). Newer Apple layer/composer tooling is not required by this macOS 12+ C99 bundle.
