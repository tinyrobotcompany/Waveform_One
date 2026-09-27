"""USB OTA transport. Caller must stop the display service before opening it."""
import os
import select
import termios
import time
import tty

class Esp:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(self.fd)
        settings = termios.tcgetattr(self.fd)
        settings[2] = (settings[2] | termios.CLOCAL | termios.CREAD) & ~termios.HUPCL
        termios.tcsetattr(self.fd, termios.TCSANOW, settings)
        self.buffer = bytearray()

    def close(self):
        os.close(self.fd)

    def request(self, command, prefix, timeout=10):
        data = (command+'\n').encode('ascii')
        deadline = time.monotonic()+timeout
        while data:
            if time.monotonic() >= deadline:
                raise TimeoutError('ESP USB write timed out')
            if select.select([], [self.fd], [], .1)[1]:
                data = data[os.write(self.fd, data):]
        while time.monotonic() < deadline:
            if select.select([self.fd], [], [], .1)[0]:
                data = os.read(self.fd,4096)
                if not data: raise ConnectionError('ESP disconnected')
                self.buffer.extend(data)
                if len(self.buffer)>16384: raise ValueError('Oversized ESP response')
                while b'\n' in self.buffer:
                    line,_,self.buffer = self.buffer.partition(b'\n')
                    line=line.decode('ascii',errors='replace').strip()
                    if line.startswith('WFU ERR'): raise RuntimeError(line)
                    if line.startswith(prefix): return line
        raise TimeoutError('ESP update protocol unavailable or timed out; USB provisioning may be required')

    def status(self):
        fields=self.request('WFU STATUS','WFU STATUS ').split()
        if len(fields)!=6 or fields[5]!='wf1-esp32s3-16mb':raise ValueError('Incompatible ESP hardware or protocol')
        return dict(digest=fields[2],state=fields[3],health=fields[4])

    def transfer(self, data, digest, progress=lambda _:None):
        if self.request(f'WFU BEGIN {len(data)} {digest}','WFU READY ',30)!='WFU READY 0':
            raise ValueError('Unexpected ESP start offset')
        for offset in range(0,len(data),24):
            packet=data[offset:offset+24]
            if self.request(f'WFU DATA {offset} {packet.hex()}','WFU READY ')!=f'WFU READY {offset+len(packet)}':
                raise ValueError('Unexpected ESP packet acknowledgement')
            if offset%2400==0:progress(offset*100//len(data))
        if self.request('WFU END','WFU STAGED')!='WFU STAGED':raise ValueError('ESP rejected image')
        self.request('WFU REBOOT','WFU REBOOTING')


def connect_healthy(path, digest, timeout=60):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        esp=None
        try:
            esp=Esp(path)
            state=esp.status()
            if state['digest']==digest and state['health']=='healthy':return esp
        except (OSError,ValueError,TimeoutError,RuntimeError):
            pass
        if esp:esp.close()
        time.sleep(1)
    raise TimeoutError('New ESP firmware did not become healthy')
