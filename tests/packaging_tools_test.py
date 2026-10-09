"""Check packaging dependency verification without downloads or executable code."""
import hashlib
import importlib.util
import io
from pathlib import Path
import tempfile
from unittest.mock import patch

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("appimage", root / "tools/build_appimage.py")
appimage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(appimage)

def reject(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("unverified packaging dependency was accepted")

with tempfile.TemporaryDirectory() as directory:
    cache = Path(directory)
    data = b"inert packaging dependency fixture"
    asset = {"sha256": hashlib.sha256(data).hexdigest(), "url": "https://invalid.example/unused"}
    with patch.object(appimage.urllib.request, "urlopen", return_value=io.BytesIO(data)) as download:
        target = appimage.download(asset, cache)
        assert target.read_bytes() == data and download.call_count == 1
    with patch.object(appimage.urllib.request, "urlopen", side_effect=AssertionError("cached asset used the network")):
        assert appimage.download(asset, cache) == target
        target.write_bytes(b"corrupted")
        reject(lambda: appimage.download(asset, cache))
        target.write_bytes(data)
        with patch.object(appimage, "MAX_ASSET_BYTES", len(data) - 1):
            reject(lambda: appimage.download(asset, cache))
    target.unlink()
    with patch.object(appimage.urllib.request, "urlopen", return_value=io.BytesIO(b"wrong bytes")):
        reject(lambda: appimage.download(asset, cache))
        assert not target.exists()
    with patch.object(appimage.urllib.request, "urlopen", return_value=io.BytesIO(data)), patch.object(appimage, "MAX_ASSET_BYTES", len(data) - 1):
        reject(lambda: appimage.download(asset, cache))
        assert not target.exists()
print("Packaging dependency downloads, cached corruption, and size limits passed")

assert appimage.glibc_requirement("GLIBC_2.9 GLIBC_2.34 GLIBCXX_3.4.30 GLIBC_2.4") == (2, 34)
assert appimage.glibc_requirement("GLIBC_PRIVATE") == (0, 0)
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    (root / "usr/lib").mkdir(parents=True)
    with patch.object(appimage, "run", return_value="GLIBC_2.39"):
        assert appimage.verify_linux_baseline(root) == "2.39"
    with patch.object(appimage, "run", return_value="GLIBC_2.40"):
        reject(lambda: appimage.verify_linux_baseline(root))
