from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1];g=r'C:/Users/micha/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe'
paths=subprocess.check_output([g,'ls-files','-m','-o','--exclude-standard'],cwd=r,text=True).splitlines()
allowed={'.md','.java','.cpp','.py','.gradle','.json','.jsonl','.log','.txt','.toml'}
paths=[x for x in paths if Path(x).suffix.lower() in allowed and all('/'+p+'/' not in '/'+x for p in ('bundle-cache','component','stock-runtime')) and '/native/' not in x.replace('wisteria/native/dlssg/','native-source/')]
subprocess.run([g,'add','--',*paths],cwd=r,check=True)
subprocess.run([g,'diff','--cached','--check','--','wisteria','tools','runtime-baseline','docs','experiments'],cwd=r,check=True)
subprocess.run([g,'commit','-m','wisteria: record DLSS-G x3 index2 status failure'],cwd=r,check=True)
