"""Generate a small, valid EPUB 3 book for the Crosspoint emulator SD card."""

import os
import sys
import zipfile
from xml.sax.saxutils import escape


def build_epub(path, title, author, chapters):
    """chapters: list of (chapter_title, [paragraph, ...])"""
    uuid = "urn:uuid:12345678-1234-4321-8432-1234567890ab"

    manifest_items = []
    spine_items = []
    nav_items = []

    for i, (chap_title, _paras) in enumerate(chapters, start=1):
        manifest_items.append(
            f'<item id="ch{i}" href="text/ch{i}.xhtml" media-type="application/xhtml+xml"/>'
        )
        spine_items.append(f'<itemref idref="ch{i}"/>')
        nav_items.append(
            f'<li><a href="text/ch{i}.xhtml">{escape(chap_title)}</a></li>'
        )

    opf = f"""<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="bookid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">{uuid}</dc:identifier>
    <dc:title>{escape(title)}</dc:title>
    <dc:creator>{escape(author)}</dc:creator>
    <dc:language>en</dc:language>
    <meta property="dcterms:modified">2026-01-01T00:00:00Z</meta>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
    <item id="css" href="style.css" media-type="text/css"/>
    {''.join(manifest_items)}
  </manifest>
  <spine toc="ncx">
    {''.join(spine_items)}
  </spine>
</package>
"""

    nav = f"""<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Contents</title></head>
<body>
<nav epub:type="toc" id="toc">
<h1>Contents</h1>
<ol>{''.join(nav_items)}</ol>
</nav>
</body>
</html>
"""

    nav_points = []
    for i, (chap_title, _paras) in enumerate(chapters, start=1):
        nav_points.append(
            f"""    <navPoint id="np{i}" playOrder="{i}">
      <navLabel><text>{escape(chap_title)}</text></navLabel>
      <content src="text/ch{i}.xhtml"/>
    </navPoint>"""
        )

    ncx = f"""<?xml version="1.0" encoding="UTF-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <head>
    <meta name="dtb:uid" content="{uuid}"/>
    <meta name="dtb:depth" content="1"/>
    <meta name="dtb:totalPageCount" content="0"/>
    <meta name="dtb:maxPageNumber" content="0"/>
  </head>
  <docTitle><text>{escape(title)}</text></docTitle>
  <navMap>
{os.linesep.join(nav_points)}
  </navMap>
</ncx>
"""

    css = """body { font-family: serif; line-height: 1.5; margin: 1em; }
h1 { font-size: 1.4em; margin-bottom: 0.8em; }
p { text-indent: 1.2em; margin: 0.35em 0; }
p.first { text-indent: 0; }
"""

    with zipfile.ZipFile(path, "w") as z:
        # mimetype must be first and uncompressed
        z.writestr(
            zipfile.ZipInfo("mimetype", (2026, 1, 1, 0, 0, 0)),
            "application/epub+zip",
            compress_type=zipfile.ZIP_STORED,
        )
        z.writestr(
            "META-INF/container.xml",
            """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
""",
        )
        z.writestr("OEBPS/content.opf", opf)
        z.writestr("OEBPS/nav.xhtml", nav)
        z.writestr("OEBPS/toc.ncx", ncx)
        z.writestr("OEBPS/style.css", css)

        for i, (chap_title, paras) in enumerate(chapters, start=1):
            body = "".join(
                f'<p class="first">{escape(p)}</p>' if idx == 0 else f"<p>{escape(p)}</p>"
                for idx, p in enumerate(paras)
            )
            z.writestr(
                f"OEBPS/text/ch{i}.xhtml",
                f"""<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>{escape(chap_title)}</title>
<link rel="stylesheet" type="text/css" href="../style.css"/>
</head>
<body>
<h1>{escape(chap_title)}</h1>
{body}
</body>
</html>
""",
            )


if __name__ == "__main__":
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out_dir, exist_ok=True)

    chapters = [
        (
            "Chapter 1 — The Quiet Shelf",
            [
                "The e-reader woke to a blank page and a soft grey sky beyond the window. "
                "It was a small machine, no bigger than a paperback, and it held more books "
                "than the entire town library had owned in 1974.",
                "E-ink does not glow. It reflects. That is the whole trick, and it is the reason "
                "you can read for hours without your eyes aching — the light you see is the "
                "light in the room, bent slightly by the page.",
                "This paragraph exists to make sure the text wraps across several lines so that "
                "line breaking, hyphenation and justification can all be observed in one go. "
                "The quick brown fox jumps over the lazy dog, again and again, until the page "
                "is full.",
            ],
        ),
        (
            "Chapter 2 — Pages That Remember",
            [
                "Every time the reader is closed, the machine writes a few bytes to the card: "
                "which book, which page, which percentage. It is a small promise that tomorrow "
                "you will start exactly where you stopped.",
                "Some formats store that progress inside the file itself; others keep a sidecar "
                "index. Either way the result is the same: a page that remembers its reader.",
            ],
        ),
        (
            "Chapter 3 — On Turning",
            [
                "A page turn on an e-ink screen is not an animation. It is a flash, a brief "
                "inversion of black and white, and then a new page. People complained about it "
                "for a decade and then stopped noticing.",
                "What remains is the pause: a half second in which the reader, too, turns over.",
            ],
        ),
    ]

    build_epub(
        os.path.join(out_dir, "The Quiet Shelf.epub"),
        "The Quiet Shelf",
        "Ada Lindqvist",
        chapters,
    )
    print("wrote", os.path.join(out_dir, "The Quiet Shelf.epub"))
