# Copyright (C) 2026 Object Vision B.V.
# License: GNU GPL 3

"""Draw the branch lines of the tree view of GeoDmsGuiQt (#1389).

DmsTreeView::setDmsStyleSheet (qtgui/exe/src/DmsTreeView.cpp) stretches three images over the
branch area of each row of the tree view, through the border-image of the style sheet:

  TV_vline.png        a vertical line, for a row below which a sibling of an ancestor follows
  TV_branch_more.png  a vertical line with an arm to the right, for an item that has a next sibling
  TV_branch_end.png   the upper half of a vertical line with an arm to the right, for a last child

They are drawn here rather than taken from Qt's documentation, whose images are under the GNU Free
Documentation License, which does not combine with the GPL of GeoDMS.

Each image is drawn from rectangles in one colour, each with its own opacity; where a rectangle
covers part of a pixel, it covers that pixel in proportion, which smooths its edges. The rectangles
of an image are laid over each other in the order given, as one translucent stroke over another.
The images are written to qtgui/exe/res/images, which GeoDmsGuiQt.qrc compiles into GeoDmsGuiQt;
after a change here, rerun the script and rebuild GeoDmsGuiQt.

make_tree_line_images.py [output folder]
"""
import os
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "qtgui", "exe", "res", "images")

# The colour of the lines, a dark grey brown, as red, green and blue from 0 to 255.
COLOUR = (99, 86, 77)

# Per image: its width and height in pixels, and its rectangles, each as
# (left, top, right, bottom, opacity), with the edges in pixels from the top left corner and the
# opacity from 0 (transparent) to 255 (opaque). Qt stretches each image over the branch area of a
# row, so only the proportions matter, and the size sets how smooth the stretched edges look.
IMAGES = {
    "TV_vline.png": (81, 58, [
        (38.53, 0.0, 42.41, 58.0, 124),       # the vertical line, over the full height
    ]),
    "TV_branch_end.png": (81, 58, [
        (38.53, 0.0, 42.41, 28.815, 124),     # the vertical line, from the top to the arm
        (42.30, 25.815, 81.0, 28.72, 124),    # the arm, from the line to the right edge
    ]),
    "TV_branch_more.png": (24, 17, [
        (11.0, 0.0, 12.946, 17.0, 74),        # the vertical line, over the full height
        (12.67, 7.27, 24.0, 8.69, 74),        # the arm, from the line to the right edge
    ]),
}


def overlap(a0, a1, b0, b1):
    """The length of the overlap of the intervals [a0, a1] and [b0, b1]."""
    return max(0.0, min(a1, b1) - max(a0, b0))


def draw(width, height, rects):
    img = Image.new("RGBA", (width, height), COLOUR + (0,))
    px = img.load()
    for y in range(height):
        for x in range(width):
            alpha = 0.0
            for left, top, right, bottom, opacity in rects:
                stroke = overlap(x, x + 1, left, right) * overlap(y, y + 1, top, bottom) * opacity / 255
                alpha += stroke * (1 - alpha)  # one translucent stroke over another
            px[x, y] = COLOUR + (round(alpha * 255),)
    return img


def main():
    out = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else OUT
    os.makedirs(out, exist_ok=True)
    for name, (width, height, rects) in IMAGES.items():
        path = os.path.join(out, name)
        draw(width, height, rects).save(path, optimize=True)
        print("written", path)


if __name__ == "__main__":
    main()
