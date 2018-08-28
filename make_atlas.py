import sys
import math
import os

import Image

dest = sys.argv[1]
almanac_path = dest + '.almanac'
target_size = int(sys.argv[2]) # Total altas size
paths = sys.argv[3:]
imgs = [(path, Image.open(path)) for path in paths]

for path, img in imgs:
    assert img.size[0] == img.size[1], 'Not square: %s' % path

dim = int(math.ceil(math.sqrt(len(imgs))))
tile_dim = target_size / dim

result = Image.new('RGB', (target_size, target_size))

with open(almanac_path, 'w') as almanac:
    almanac.write('scale %d\n' % dim)
    for i, (path, img) in enumerate(imgs):
        if tile_dim > img.size[0]:
            sys.stderr.write('%s: Upsampling from %d to %d\n' % (path, img.size[0], tile_dim))

        col = i % dim
        row = int(i / dim)
        offset_x = float(col) / dim
        offset_y = float(row) / dim
        almanac.write('%g %g %s\n' % (offset_x, offset_y, os.path.basename(path)))

        region = img.resize((tile_dim, tile_dim))
        region = region.crop((0, 0, img.size[0], img.size[1]))

        x = col * tile_dim
        y = row * tile_dim
        dest_box = (x, y)
        assert result.mode == region.mode
        result.paste(region, dest_box)

# Blender convention: origin is the bottom of the image
result.transpose(Image.FLIP_TOP_BOTTOM).save(dest, 'png')
