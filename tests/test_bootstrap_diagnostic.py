# SPDX-License-Identifier: GPL-3.0-or-later
import os,shutil,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='bootstrap-diag-',dir=root/'work') as temp:
    package=Path(temp)/'space & (test)!';package.mkdir()
    shutil.copyfile(root/'upstream/plasma-windows/tools/diagnose-startup.cmd',package/'diagnose-startup.cmd')
    env=os.environ.copy();env['DIAG_FIXTURE']=str(package/'diagnose-startup.cmd')
    driver=Path(temp)/'driver.cmd';driver.write_text('@echo off\nsetlocal DisableDelayedExpansion\ncall "%DIAG_FIXTURE%" --no-pause\nexit /b %errorlevel%\n',encoding='ascii')
    for body,code in [('@echo off\necho before-failure\necho fixture-stderr 1>&2\nexit 23\n',23),('@echo off\necho success\nexit /b 0\n',0)]:
        (package/'deploy-vm.cmd').write_text(body,encoding='ascii',newline='\r\n')
        p=subprocess.run(['cmd.exe','/d','/v:on','/c',str(driver)],env=env,capture_output=True,timeout=10)
        assert p.returncode==code,(p.returncode,p.stdout,p.stderr)
        text=(package/'startup-diagnostic.log').read_bytes()
        assert ('Bootstrap exit code: '+str(code)).encode() in text
        if code: assert b'before-failure' in text and b'fixture-stderr' in text
    (package/'deploy-vm.cmd').unlink()
    p=subprocess.run(['cmd.exe','/d','/c',str(driver)],env=env,capture_output=True,timeout=10)
    assert p.returncode==2 and b'is missing' in (package/'startup-diagnostic.log').read_bytes()
# Exercise the real entrypoint and service dispatch using harmless executable copies.
with tempfile.TemporaryDirectory(prefix='bootstrap-chain-',dir=root/'work') as temp:
    package=Path(temp)/'space & (test)!';(package/'bin/data/applications').mkdir(parents=True)
    tools=root/'upstream/plasma-windows/tools'
    for name in ['deploy-vm.cmd','diagnose-startup.cmd','write-session-menu.ps1']:
        shutil.copyfile(tools/name,package/name)
    common=(tools/'plasma-common.cmd').read_text()
    start=common.index(':pc_start_bus\n',common.index('rem :pc_start_bus'))
    end=common.index('rem ---------------------------------------------------------------------------',start)
    common=common[:start]+':pc_start_bus\nexit /b 0\n\n'+common[end:]
    (package/'plasma-common.cmd').write_text(common,encoding='ascii',newline='\r\n')
    for name in ['plasmashell','kbuildsycoca6','kde-session-host','kactivitymanagerd','kded6','trayhost','plasma-window-decorations']:
        shutil.copyfile(Path(os.environ['SystemRoot'])/'System32/whoami.exe',package/'bin'/f'{name}.exe')
    env=os.environ.copy();env.update(LOCALAPPDATA=str(package/'profile'),KDE_BOOTSTRAP_NO_PAUSE='1')
    for name in ['KDE_ON_WIN_SETTINGS','KDE_DEPLOY_CAPTURED','ERRORLEVEL']:env.pop(name,None)
    driver=Path(temp)/'run.cmd';env['FIXTURE_DEPLOY']=str(package/'deploy-vm.cmd')
    driver.write_text('@echo off\nsetlocal DisableDelayedExpansion\ncall "%FIXTURE_DEPLOY%"\nexit /b %errorlevel%\n',encoding='ascii')
    result=subprocess.run(['cmd.exe','/d','/v:on','/c',str(driver)],env=env,capture_output=True,timeout=25)
    assert result.returncode==0,(result.stdout,result.stderr)
    log=(package/'startup-diagnostic.log').read_bytes()
    assert b'5/5 Start session' in log and b'plasmashell exited with code 0' in log
    blocked=package/'blocked-profile';blocked.write_text('fixture');env['LOCALAPPDATA']=str(blocked)
    result=subprocess.run(['cmd.exe','/d','/c',str(driver)],env=env,capture_output=True,timeout=25)
    assert result.returncode!=0,(result.stdout,result.stderr)
    log=(package/'startup-diagnostic.log').read_bytes()
    assert b'3/5 applications.menu' in log and b'4/5 Rebuild' not in log
    assert b'Bootstrap exit code: 1' in log
print('PASS: diagnostic retains early stdout/stderr and raw child exit, survives EXIT, handles missing script, special paths. No real session launched.')
