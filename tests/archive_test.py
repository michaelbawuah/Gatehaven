"""Verify the exact archives that CI uploads, including their checksums."""
import hashlib
from pathlib import Path, PurePosixPath
import sys
import tarfile
import zipfile

build = Path(sys.argv[1])
archives = sorted([*build.glob("Gatehaven-*.zip"), *build.glob("Gatehaven-*.tar.gz")])
assert archives, "No Gatehaven package was produced"
notice = (Path(__file__).resolve().parents[1] / "third_party" / "SDL3" / "LICENSE.txt").read_bytes()
for package in archives:
    expected = Path(str(package) + ".sha256").read_text().split()[0]
    assert hashlib.sha256(package.read_bytes()).hexdigest() == expected, package
    zipped = package.suffix == ".zip"
    with (zipfile.ZipFile(package) if zipped else tarfile.open(package)) as archive:
        names = archive.namelist() if zipped else archive.getnames()
        files = {name for name in names if not name.endswith("/")}
        assert all(not PurePosixPath(name).is_absolute() and ".." not in PurePosixPath(name).parts for name in names)
        roots = {PurePosixPath(name).parts[0] for name in names}
        assert len(roots) == 1
        prefix = roots.pop() + "/"
        resources = prefix + "share/gatehaven/"
        required = [resources + "README.md", resources + "docs/manual.md", resources + "docs/manual.html", resources + "third_party/SDL3/LICENSE.txt"]
        required += [resources + "samples/" + name + ".ghv" for name in
                     ("starter", "oscillator", "screen-switch", "positive-relay", "negative-relay", "gate-gallery")]
        suffix = ".exe" if "Windows" in package.name else ""
        required.append(prefix + "bin/gatehaven-cli" + suffix)
        if "Darwin" in package.name:
            required += [prefix + "gatehaven.app/Contents/MacOS/gatehaven",
                         prefix + "gatehaven.app/Contents/Info.plist",
                         prefix + "gatehaven.app/Contents/Resources/third_party/SDL3/LICENSE.txt"]
        else:
            required.append(prefix + "bin/gatehaven" + suffix)
        assert set(required) <= files, (package, set(required) - files)
        license_path = resources + "third_party/SDL3/LICENSE.txt"
        actual_notice = archive.read(license_path) if zipped else archive.extractfile(license_path).read()
        assert actual_notice == notice
    print(f"Archive verified: {package.name} ({package.stat().st_size} bytes, SHA-256 matches)")
