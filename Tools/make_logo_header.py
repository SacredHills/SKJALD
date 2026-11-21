#!/usr/bin/env python3
"""Regenerates Source/UI/Assets/LogoData.h from skjald_logo.png (run from the repo root)."""
d = open('Source/UI/Assets/skjald_logo.png', 'rb').read()
from PIL import Image
w, h = Image.open('Source/UI/Assets/skjald_logo.png').size
with open('Source/UI/Assets/LogoData.h', 'w') as f:
    f.write('#pragma once\n// Generated from Source/UI/Assets/skjald_logo.png (white logo with alpha). Regenerate with Tools/make_logo_header.py\nnamespace skjald::ui::assets\n{\ninline constexpr unsigned char skjaldLogoPng[] = {\n')
    for i in range(0, len(d), 24):
        f.write('    ' + ','.join(str(b) for b in d[i:i + 24]) + ',\n')
    f.write('};\ninline constexpr int skjaldLogoPngSize = %d;\ninline constexpr int skjaldLogoWidth = %d, skjaldLogoHeight = %d;\n}\n' % (len(d), w, h))
