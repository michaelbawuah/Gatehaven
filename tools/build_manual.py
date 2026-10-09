"""Build a deterministic, dependency-free offline edition of repository guides."""
import html
from pathlib import Path
import re
import sys


def inline(text):
    pattern = r"(`[^`]+`|\[[^\]]+\]\([^)]+\))"
    result = []
    for part in re.split(pattern, text):
        if part.startswith("`") and part.endswith("`"):
            result.append("<code>" + html.escape(part[1:-1]) + "</code>")
        elif match := re.fullmatch(r"\[([^\]]+)\]\(([^)]+)\)", part):
            label, target = match.groups()
            if target.endswith(".md"):
                target = Path(target).stem + ".html"
            if ":" in target and not target.startswith(("https://", "http://")):
                result.append(html.escape(label))
            else:
                result.append('<a href="' + html.escape(target, quote=True) + '">' + html.escape(label) + "</a>")
        else:
            result.append(html.escape(part))
    return "".join(result)


def render(text, title):
    body, paragraph, code = [], [], None
    listing = False
    def flush():
        if paragraph:
            body.append("<p>" + inline(" ".join(paragraph)) + "</p>")
            paragraph.clear()
    for line in text.splitlines():
        if line.startswith("```"):
            flush()
            if code is None:
                code = []
            else:
                body.append("<pre><code>" + html.escape("\n".join(code)) + "</code></pre>")
                code = None
            continue
        if code is not None:
            code.append(line)
            continue
        bullet = re.match(r"^\s*(?:[-*]|\d+\.)\s+(.*)", line)
        if listing and not bullet:
            body.append("</ul>")
            listing = False
        if bullet:
            flush()
            if not listing:
                body.append("<ul>")
                listing = True
            body.append("<li>" + inline(bullet[1]) + "</li>")
        elif heading := re.match(r"^(#{1,6})\s+(.*)", line):
            flush()
            level = len(heading[1])
            body.append(f"<h{level}>" + inline(heading[2]) + f"</h{level}>")
        elif not line.strip():
            flush()
        else:
            paragraph.append(line.strip())
    flush()
    if listing:
        body.append("</ul>")
    if code is not None:
        body.append("<pre><code>" + html.escape("\n".join(code)) + "</code></pre>")
    return """<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>""" + html.escape(title) + """ — Gatehaven</title>
<style>body{font:18px/1.65 system-ui,sans-serif;max-width:850px;margin:2rem auto;padding:0 1.5rem;color:#1f2d40;background:#f6f8f8}a{color:#006e5c}nav{border-bottom:1px solid #a7b6bd;padding-bottom:1rem}pre{overflow:auto;padding:1rem;background:#e8eeee}code{font-size:.92em}h1,h2,h3{line-height:1.25}a:focus-visible{outline:3px solid #dc6228;outline-offset:3px}@media print{nav{display:none}body{font-size:11pt;background:white}}</style>
<nav aria-label="Manual navigation"><a href="manual.html">Manual</a> · <a href="communicators.html">File ports</a> · <a href="architecture.html">Simulation</a> · <a href="verification.html">Release status</a></nav>
<main>""" + "\n".join(body) + "</main></html>\n"


def build(source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    for path in sorted((source / "docs").glob("*.md")):
        text = path.read_text(encoding="utf-8")
        title = text.splitlines()[0].lstrip("# ")
        (destination / (path.stem + ".html")).write_text(render(text, title), encoding="utf-8")
    # The manual links to the sample index; keep that link local too.
    samples = source / "samples" / "README.md"
    (destination / "README.html").write_text(render(samples.read_text(encoding="utf-8"), "Circuit lessons"), encoding="utf-8")


if __name__ == "__main__":
    build(Path(sys.argv[1]), Path(sys.argv[2]))
