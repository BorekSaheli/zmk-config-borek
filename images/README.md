# Sofle nice!view images

PNG sources for the rotating-image widget on the Sofle Choc Pro nice!view displays.

Every hour, the two halves display a different image from this folder — central
picks both indices and sends the peripheral's over a custom split BLE service.

## Adding / removing images

1. Drop a `.png` in this folder (any size; it will be auto-cropped + dithered
   to 68×140 1-bit mono).
2. Run the converter:

   ```bash
   # Windows (uv-managed Python)
   uv venv
   uv pip install Pillow
   .venv/Scripts/python.exe images/convert.py
   ```

3. Commit **both** the PNG and the generated files under
   `boards/shields/nice_view_disp/widgets/art/` — GitHub Actions builds the
   firmware from these committed C arrays (no Python runs in CI).

`ART_IMAGES_COUNT` in the generated `art_list.h` must be `>= 2` for the
left/right differ-guarantee to hold.
