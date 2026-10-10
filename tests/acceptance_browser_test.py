"""Exercise the offline report in a fresh headless Chrome profile, never a user's browser session."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("report", type=Path)
parser.add_argument("chrome", type=Path)
parser.add_argument("--screenshot", type=Path)
args = parser.parse_args()
harness = r'''
<pre id="qa-results">RUNNING</pre><script>
(async()=>{
 const assert=(ok,message)=>{if(!ok)throw Error(message);};
 const pause=()=>new Promise(resolve=>setTimeout(resolve,40));
 const input=(id,value)=>{const e=document.getElementById(id);e.value=value;e.dispatchEvent(new Event(e.tagName==='SELECT'?'change':'input'));};
 const fixture=JSON.parse(document.getElementById('report-data').textContent);
 let exported;
 URL.createObjectURL=blob=>{exported=blob;return 'blob:acceptance-test';};
 URL.revokeObjectURL=()=>{};
 HTMLAnchorElement.prototype.click=function(){};
 assert(document.querySelectorAll('select').length===12,'missing scenarios');
 assert(document.getElementById('summary').textContent.includes('12 pending'),'manual results were pre-approved');
 input('result-first_launch','failed');document.getElementById('download').click();
 assert(document.getElementById('message').textContent.includes('Add notes'),'unexplained failure exported');
 input('notes-first_launch','Fixture: text appeared clipped');document.getElementById('download').click();
 let saved=JSON.parse(await exported.text());
 assert(saved.manual[0].status==='failed'&&saved.manual[0].notes.includes('clipped'),'export lost observations');
 assert(saved.release_acceptance_complete===false,'report claimed final release acceptance');
 async function importFile(data){const transfer=new DataTransfer();transfer.items.add(new File([JSON.stringify(data)],'results.json',{type:'application/json'}));const picker=document.getElementById('import');picker.files=transfer.files;picker.dispatchEvent(new Event('change'));await pause();}
 saved.manual[0].status='passed';saved.manual[0].notes='Explicit test observation';saved.tester_notes='Test setup';
 await importFile(saved);
 assert(document.getElementById('result-first_launch').value==='passed','valid import failed');
 assert(document.getElementById('tester-notes').value==='Test setup','setup was lost');
 const other=structuredClone(saved);other.candidate.binary_sha256='different-build';other.manual[0].status='failed';
 await importFile(other);
 assert(document.getElementById('message').textContent.includes('different build'),'wrong build imported');
 assert(document.getElementById('result-first_launch').value==='passed','invalid import changed observations');
 const malicious=structuredClone(saved);malicious.manual[0].notes='<img src=x onerror="window.injected=true">';
 await importFile(malicious);
 assert(window.injected!==true&&!document.querySelector('img'),'notes became executable markup');
 assert(JSON.parse(document.getElementById('evidence').textContent).automated_status===fixture.automated_status,'import changed automatic evidence');
 document.getElementById('qa-results').textContent='PASS: pending defaults, note validation, export, import, build identity and inert text';
})().catch(error=>{document.getElementById('qa-results').textContent='FAIL: '+error.message;});
</script>
'''
with tempfile.TemporaryDirectory(prefix="gatehaven-report-browser-") as directory:
    root = Path(directory)
    page = root / "test.html"
    page.write_text(args.report.read_text(encoding="utf-8").replace("</html>", harness + "</html>"), encoding="utf-8")
    command = [str(args.chrome), "--headless", "--no-first-run", "--no-default-browser-check", "--disable-background-networking",
               "--disable-extensions", "--user-data-dir=" + str(root / "profile"), "--virtual-time-budget=3000", "--dump-dom", page.as_uri()]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=45)
    marker = '<pre id="qa-results">PASS:'
    if result.returncode or marker not in result.stdout:
        raise RuntimeError("Offline report browser checks failed\n" + result.stdout[-3000:] + "\n" + result.stderr[-2000:])
    if args.screenshot:
        subprocess.run([str(args.chrome), "--headless", "--no-first-run", "--no-default-browser-check", "--disable-background-networking",
                        "--disable-extensions", "--user-data-dir=" + str(root / "screenshot-profile"), "--window-size=1280,1000",
                        "--screenshot=" + str(args.screenshot.resolve()), args.report.resolve().as_uri()],
                       check=True, capture_output=True, timeout=45)
print("Offline report browser interactions passed")
