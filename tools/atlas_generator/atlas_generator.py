#!/usr/bin/env python3
"""
Kraft Sprite Atlas Generator

Packs individual sprite images from one or more input folders into a single
texture atlas PNG and writes a companion .katlas metadata file.

Usage:
    python atlas_generator.py folder1/ [folder2/ ...] output_path [options]

Example:
    python atlas_generator.py sprites/tiles sprites/enemies res/textures/game_atlas
    -> generates res/textures/game_atlas.png and res/textures/game_atlas.katlas

    python atlas_generator.py sprites/ res/textures/atlas --padding 2
    -> 2px padding between sprites instead of the default 1px

    python atlas_generator.py sprites/ res/textures/atlas --scale 0.25
    -> downscale all sprites to 25% (e.g. 256x256 -> 64x64)
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow is required. Install it with: pip install Pillow")
    sys.exit(1)

IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".tga"}


def collect_images(folders):
    """Collect all image files from the given folders, sorted by name."""
    images = []
    for folder in folders:
        folder = Path(folder)
        if not folder.is_dir():
            print(f"Warning: '{folder}' is not a directory, skipping.")
            continue
        for file in sorted(folder.iterdir()):
            if file.suffix.lower() in IMAGE_EXTENSIONS:
                images.append(file)
    return images


def next_power_of_two(n):
    """Return the smallest power of two >= n."""
    p = 1
    while p < n:
        p *= 2
    return p


def pack_shelf(images, padding, scale):
    """
    Shelf packing algorithm.
    Sorts images by height (tallest first), packs left-to-right in rows.

    Returns: (atlas_width, atlas_height, placements)
        placements: list of (name, path, img, x, y, w, h)
    """
    # Load all images, optionally rescale, and get their sizes
    entries = []
    for path in images:
        img = Image.open(path)
        img = img.convert("RGBA")
        if scale != 1.0:
            new_w = max(1, int(img.width * scale))
            new_h = max(1, int(img.height * scale))
            img = img.resize((new_w, new_h), Image.NEAREST)
        name = path.stem  # filename without extension
        entries.append((name, path, img, img.width, img.height))

    # Sort by height descending, then width descending for better packing
    entries.sort(key=lambda e: (e[4], e[3]), reverse=True)

    # First pass: estimate atlas size
    total_area = sum((w + padding) * (h + padding) for _, _, _, w, h in entries)
    # Start with a square-ish power-of-two estimate
    side = next_power_of_two(int(total_area**0.5))
    atlas_width = side

    # Try packing, increase size if it doesn't fit
    while True:
        placements = []
        shelf_x = padding
        shelf_y = padding
        shelf_height = 0

        for name, path, img, w, h in entries:
            # Does this sprite fit on the current shelf?
            if shelf_x + w + padding > atlas_width:
                # Move to next shelf
                shelf_y += shelf_height + padding
                shelf_x = padding
                shelf_height = 0

            # Record placement
            placements.append((name, path, img, shelf_x, shelf_y, w, h))
            shelf_x += w + padding
            shelf_height = max(shelf_height, h)

        # Calculate total height needed
        atlas_height = next_power_of_two(shelf_y + shelf_height + padding)

        # Check if everything fits
        max_y = max(y + h for _, _, _, _, y, _, h in placements)
        if max_y + padding <= atlas_height and atlas_height <= 8192:
            break

        # Doesn't fit, try wider
        atlas_width *= 2
        if atlas_width > 8192:
            print("Error: Sprites don't fit in a 8192x8192 atlas!")
            sys.exit(1)

    return atlas_width, atlas_height, placements


def generate_atlas(folders, output_path, padding, scale):
    """Generate the atlas PNG and .katlas metadata file."""
    images = collect_images(folders)
    if not images:
        print("Error: No images found in the specified folders.")
        sys.exit(1)

    print(f"Found {len(images)} sprites across {len(folders)} folder(s)")
    if scale != 1.0:
        print(f"Scaling sprites by {scale}x")

    atlas_width, atlas_height, placements = pack_shelf(images, padding, scale)

    print(
        f"Atlas size: {atlas_width}x{atlas_height} ({len(placements)} sprites, padding={padding}px)"
    )

    # Create the atlas image
    atlas = Image.new("RGBA", (atlas_width, atlas_height), (0, 0, 0, 0))
    for name, path, img, x, y, w, h in placements:
        atlas.paste(img, (x, y))

    # Ensure output directory exists
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)

    # Write atlas PNG
    png_path = output.with_suffix(".png")
    atlas.save(png_path, "PNG")
    print(f"Written: {png_path}")

    # Write .katlas metadata
    katlas_path = output.with_suffix(".katlas")
    png_filename = png_path.name  # e.g. "tile_atlas.png"
    with open(katlas_path, "w") as f:
        f.write("// Auto-generated sprite atlas - do not edit manually\n")
        f.write(
            f"// Re-generate with: atlas_generator.py <input_folders> {output_path}\n"
        )
        f.write(f'texture "{png_filename}"\n')
        f.write(f"atlas_size {atlas_width} {atlas_height}\n")
        if scale != 1.0:
            f.write(f"scale {scale}\n")
        f.write(f"sprite_count {len(placements)}\n")
        f.write("\n")
        f.write(f"// {'name':<34s} {'x':>5s} {'y':>5s} {'w':>5s} {'h':>5s}\n")
        for name, path, img, x, y, w, h in placements:
            f.write(
                f'"{name}"{"":>{max(1, 34 - len(name) - 2)}} {x:5d} {y:5d} {w:5d} {h:5d}\n'
            )

    print(f"Written: {katlas_path}")
    print("Done!")


def main():
    parser = argparse.ArgumentParser(
        description="Pack sprite images into a texture atlas with metadata.",
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        help="Input folders containing sprite images, followed by the output path (without extension)",
    )
    parser.add_argument(
        "--padding",
        type=int,
        default=1,
        help="Padding in pixels between sprites (default: 1)",
    )
    parser.add_argument(
        "--scale",
        type=float,
        default=1.0,
        help="Scale factor for sprites before packing (e.g. 0.25 to downscale 4x)",
    )

    args = parser.parse_args()

    if len(args.inputs) < 2:
        parser.error("Need at least one input folder and one output path")

    folders = args.inputs[:-1]
    output_path = args.inputs[-1]

    generate_atlas(folders, output_path, args.padding, args.scale)


if __name__ == "__main__":
    main()
