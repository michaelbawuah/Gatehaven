"""Check generated offline guides without launching a browser."""
from pathlib import Path
import importlib.util
import tempfile
from html.parser import HTMLParser

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("manual", root / "tools/build_manual.py")
manual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manual)
assert "&lt;script&gt;" in manual.render("<script>alert(1)</script>", "Safety")
assert "javascript:" not in manual.inline("[bad](javascript:alert)")
assert "<code>&lt;&amp;</code>" in manual.inline("`<&`")
class Links(HTMLParser):
    def __init__(self):
        super().__init__(); self.links = []
    def handle_starttag(self, tag, attrs):
        if tag == "a": self.links += [value for key, value in attrs if key == "href"]
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory)
    manual.build(root, output)
    first = {p.name: p.read_bytes() for p in output.glob("*.html")}
    manual.build(root, output)
    assert first == {p.name: p.read_bytes() for p in output.glob("*.html")}
    for path in output.glob("*.html"):
        parser = Links(); parser.feed(path.read_text(encoding="utf-8"))
        for link in parser.links:
            if link.endswith(".html") and ":" not in link:
                assert (output / link).is_file(), (path.name, link)
print("Offline manual escaping, local links and deterministic output passed")
