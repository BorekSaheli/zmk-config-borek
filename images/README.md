# Corne nice_view_custom images

PNG sources for the rotating-image widget on the Corne Choc Pro nice!view
displays.

Every hour, the two halves display a different image from this folder — central
picks both indices and sends the peripheral's over a custom split BLE service.

## Adding / removing images

1. Drop a `.png` in this folder (any size; it's auto-cropped + dithered to
   68×68 1-bit mono).
2. Regenerate the baked byte arrays:

   ```bash
   # Windows (uv-managed Python)
   uv venv
   uv pip install Pillow
   .venv/Scripts/python.exe images/convert.py
   ```

3. Commit **both** the PNG and the generated files under
   `boards/shields/nice_view_custom/widgets/art/` — GitHub Actions builds the
   firmware from the committed C arrays (no Python runs in CI).

`ART_IMAGES_COUNT` in the generated `art_list.h` must be ≥ 2 so the
left-≠-right guarantee holds.
