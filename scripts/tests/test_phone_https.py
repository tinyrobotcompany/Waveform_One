"""Exercise the read-only probe with a real TLS handshake and issuer validation."""
import importlib.util
import http.server
from pathlib import Path
import ssl
import tempfile
import threading
import unittest

ROOT=Path(__file__).resolve().parents[2]
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,ROOT/'scripts'/file)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
identity=module('identity','phone-identity.py');probe=module('probe','check-phone-https.py')

class HTTPSProof(unittest.TestCase):
    def test_certificate_trust_hostname_and_authentication(self):
        with tempfile.TemporaryDirectory() as root:
            directory=Path(root)/'device';name='device-test.waveform.tinyrobotcompany.io'
            identity.create(directory,name);identity.development_certificate(directory)
            class Handler(http.server.BaseHTTPRequestHandler):
                code=401
                def do_GET(self):
                    self.send_response(self.code);self.end_headers()
                def log_message(self,*args):pass
            server=http.server.HTTPServer(('127.0.0.1',0),Handler)
            context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version=ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(directory/'fullchain.pem',directory/'device.key')
            server.socket=context.wrap_socket(server.socket,server_side=True)
            thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
            try:
                args=(name,'127.0.0.1',str(directory/'ca.pem'),server.server_port)
                self.assertIn(probe.check(*args),('TLSv1.2','TLSv1.3'))
                with self.assertRaises(ssl.SSLCertVerificationError):probe.check('other.example.com',*args[1:])
                with self.assertRaises(ssl.SSLCertVerificationError):probe.check(name,'127.0.0.1',None,server.server_port)
                Handler.code=200
                with self.assertRaises(ValueError):probe.check(*args)
            finally:
                server.shutdown();server.server_close();thread.join()

if __name__=='__main__':unittest.main()
