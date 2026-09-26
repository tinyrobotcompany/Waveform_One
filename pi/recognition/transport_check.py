"""Linux integration check: simulated ESP, real serial worker and HTTP capture.

No physical serial port or external recognition service is used. macOS PTYs
are not supported by the serialport crate, so run this on Linux.
"""
import os, pty, select, socket, subprocess, tempfile, threading, time, urllib.request, urllib.error, json
from pathlib import Path
master, slave=pty.openpty()
port=socket.socket();port.bind(('127.0.0.1',0)); number=port.getsockname()[1];port.close()
stop=threading.Event(); errors=[]
def fake_esp():
    buf=b'';last=0; captures=0
    try:
        while not stop.is_set():
            if time.monotonic()-last>.15:
                os.write(master,b'OPEN DISPLAY=BARS |333333333333333333333333|\n');last=time.monotonic()
            if not select.select([master],[],[],.01)[0]:continue
            buf+=os.read(master,4096)
            while b'\n' in buf:
                line,buf=buf.split(b'\n',1); parts=line.split()
                if len(parts)<3 or parts[0]!=b'WF1':continue
                ident=parts[1].decode()
                if parts[2]==b'CAPTURE':
                    captures+=1
                    if captures==1:
                        os.write(master,f'\nWF1 {ident} ERR AUDIO_LOST\n'.encode())
                        continue
                    os.write(master,f'\nWF1 {ident} AUDIO 16000 128000\n'.encode())
                    for seq in range(1000):
                        payload=bytes([seq%256])*256
                        h=2166136261
                        for b in payload:h=((h^b)*16777619)&0xffffffff
                        os.write(master,f'\nWF1 {ident} PCM {seq} {payload.hex()} {h:08x}\n'.encode())
                        if seq%20==0:os.write(master,b'OPEN DISPLAY=BARS |333333333333333333333333|\n')
                    os.write(master,f'\nWF1 {ident} END 1000\n'.encode())
                else:os.write(master,f'\nWF1 {ident} OK MODE mirrored\n'.encode())
    except Exception as e:
        if not stop.is_set():errors.append(str(e))
with tempfile.TemporaryDirectory() as directory:
    Path(directory,'remote-token').write_text('a'*48)
    env={**os.environ,'WAVEFORM_BIND':f'127.0.0.1:{number}','WAVEFORM_CONFIG_DIR':directory}
    proc=subprocess.Popen([os.environ.get('WAVEFORM_TEST_BIN','pi/core/target/debug/waveform-display'),os.ttyname(slave)],env=env,stderr=subprocess.PIPE)
    worker=threading.Thread(target=fake_esp,daemon=True);worker.start()
    def request(path,post=False):
        r=urllib.request.Request(f'http://127.0.0.1:{number}'+path,data=b'' if post else None,headers={'Authorization':'Bearer '+'a'*48})
        return urllib.request.urlopen(r,timeout=25).read()
    try:
        deadline=time.monotonic()+8
        while True:
            try:
                if json.loads(request('/api/state'))['device']['phase']=='playing':break
            except OSError:pass
            if time.monotonic()>deadline:raise RuntimeError('fake serial did not become ready: '+request('/api/state').decode()+' errors='+str(errors))
            time.sleep(.1)
        session=json.loads(request('/api/state'))['device']['session']
        try:
            request('/api/capture',True)
            raise AssertionError('ESP audio loss must reject the clip')
        except urllib.error.HTTPError as error:
            assert error.code==503
        after=json.loads(request('/api/state'))['device']
        assert after['connected'], 'A rejected clip must not disconnect the visualizer'
        assert after['session']==session, 'A rejected clip must preserve the listening session'
        audio=request('/api/capture',True)
        assert len(audio)==256000
        assert all(audio[i*256:(i+1)*256]==bytes([i%256])*256 for i in range(1000))
        assert json.loads(request('/api/state'))['device']['connected']
        assert not errors,errors
        print('PASS: real serial worker + HTTP capture reassembled 1000 interleaved packets correctly')
    finally:
        stop.set();proc.terminate();proc.wait(timeout=3);os.close(master);os.close(slave)
