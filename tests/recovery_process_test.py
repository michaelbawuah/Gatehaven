import subprocess
import sys
import tempfile

binary = sys.argv[1]
with tempfile.TemporaryDirectory(prefix="gatehaven recovery ") as directory:
    def scan():
        return subprocess.run([binary, directory, "scan"], text=True, capture_output=True, check=True, timeout=10).stdout.splitlines()
    writer = subprocess.Popen([binary, directory, "hold"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    try:
        identity = writer.stdout.readline().strip()
        assert identity.startswith("session-")
        assert scan() == [], "Live snapshot appeared as abandoned"
        writer.kill()
        writer.wait(timeout=10)
        assert scan() == [identity], "Process crash failed to release recovery ownership"
        first = subprocess.Popen([binary, directory, "restore", identity], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        second = subprocess.Popen([binary, directory, "restore", identity], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        first.communicate(timeout=10)
        second.communicate(timeout=10)
        assert sorted((first.returncode, second.returncode)) == [0, 1], "Two consumers restored the same snapshot"
        assert scan() == [], "Successful restoration left an abandoned snapshot"
    finally:
        if writer.poll() is None:
            writer.kill()
            writer.wait(timeout=10)
print("Recovery survives process termination and protects live windows")
