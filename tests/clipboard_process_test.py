"""Exercise the real session protocol across independently running processes."""

import concurrent.futures
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading


class Peer:
    def __init__(self, executable, root):
        self.process = subprocess.Popen(
            [executable, str(root)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding="utf-8")
        self.lines = queue.Queue()
        threading.Thread(target=self._read, daemon=True).start()
        assert self.response() == "READY"

    def _read(self):
        for line in self.process.stdout:
            self.lines.put(line.strip())
        self.lines.put("EOF " + self.process.stderr.read())

    def response(self):
        return self.lines.get(timeout=8)

    def command(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()
        return self.response()

    def close(self, crash=False):
        if self.process.poll() is None:
            if crash:
                self.process.kill()
            else:
                self.process.stdin.write("QUIT\n")
                self.process.stdin.flush()
            self.process.wait(timeout=8)
            if not crash:
                assert self.process.returncode == 0
        self.process.stdin.close()
        self.process.stdout.close()
        self.process.stderr.close()


def check(executable):
    peers = []
    with tempfile.TemporaryDirectory(prefix="gatehaven clipboard ") as temporary:
        root = Path(temporary) / "session"

        def join():
            peer = Peer(executable, root)
            peers.append(peer)
            return peer

        try:
            a, b = join(), join()
            assert a.command("GET 0") == "EMPTY"
            assert a.command("SET 3 17") == "OK"
            assert b.command("GET 0") == "17"
            assert b.command("GET 3") == "17"
            assert b.command("SET 7 29") == "OK"
            assert a.command("GET 0") == "29"
            assert a.command("GET 3") == "17"
            assert b.command("GET 0") == "17"  # Reading an explicit slot updates zero.
            assert b.command("SET 0 41") == "OK"
            assert a.command("GET 0") == "41"
            assert a.command("GET 7") == "29"
            assert a.command("GET 10").startswith("ERROR ")
            assert a.command("SET 10 1").startswith("ERROR ")

            def write_many(peer, slot):
                for value in range(40):
                    assert peer.command(f"SET {slot} {value}") == "OK"
                    assert peer.command(f"GET {slot}") == str(value)

            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
                jobs = [executor.submit(write_many, a, 2), executor.submit(write_many, b, 8)]
                for job in jobs:
                    job.result(timeout=20)

            a.close(crash=True)
            c = join()
            assert c.command("GET 3") == "17"  # One survivor keeps the session alive.
            b.close()
            assert c.command("GET 7") == "29"
            c.close()
            assert sorted(path.name for path in root.iterdir()) == ["members.lock", "transaction.lock"]

            d = join()
            assert d.command("GET 3") == "EMPTY"
            assert d.command("SET 1 99") == "OK"
            d.close(crash=True)
            e = join()
            assert e.command("GET 1") == "EMPTY"  # First member clears abandoned crash data.
            assert e.command("SET 1 15") == "OK"
            (root / "slot-1.ghclip").write_bytes(b"broken")
            assert e.command("GET 1").startswith("ERROR ")
            assert e.command("SET 1 16") == "OK"  # A damaged payload cannot poison the lock.
            assert e.command("GET 1") == "16"
            e.close()
        finally:
            for peer in peers:
                if peer.process.poll() is None:
                    peer.close(crash=True)
    print("Shared clipboard: exchange, contention, slot zero, cleanup and crash recovery passed")


if __name__ == "__main__":
    check(sys.argv[1])
