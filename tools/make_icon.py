"""Generates res/Lviewer.ico (original artwork: rounded tile, photo frame and an 'L')."""
import sys
from PIL import Image, ImageDraw, ImageFont

def tile(size):
    s = size * 4  # supersample
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    r = s // 5
    # vertical gradient teal -> deep blue
    grad = Image.new("RGBA", (s, s))
    gd = ImageDraw.Draw(grad)
    for y in range(s):
        t = y / (s - 1)
        c = (int(18 + (24 - 18) * t), int(150 + (70 - 150) * t), int(170 + (160 - 170) * t), 255)
        gd.line([(0, y), (s, y)], fill=c)
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, s - 1, s - 1], radius=r, fill=255)
    im.paste(grad, (0, 0), mask)
    # mountains (photo glyph) in the lower right
    m = s * 0.12
    d.polygon([(s * 0.38, s - m), (s * 0.60, s * 0.50), (s * 0.78, s * 0.70), (s * 0.86, s * 0.62), (s - m, s - m)], fill=(255, 255, 255, 110))
    d.ellipse([s * 0.70, s * 0.20, s * 0.84, s * 0.34], fill=(255, 214, 90, 255))
    # bold "L"
    w = s * 0.13
    x0, y0, y1 = s * 0.18, s * 0.16, s * 0.84
    d.rectangle([x0, y0, x0 + w, y1], fill=(255, 255, 255, 255))
    d.rectangle([x0, y1 - w, s * 0.62, y1], fill=(255, 255, 255, 255))
    return im.resize((size, size), Image.LANCZOS)

def main(out):
    sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
    imgs = [tile(z) for z in sizes]
    imgs[-1].save(out, format="ICO", sizes=[(z, z) for z in sizes], append_images=imgs[:-1])

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "res/Lviewer.ico")
