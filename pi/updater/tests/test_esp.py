import hashlib
import os
import threading
import unittest
from unittest.mock import Mock
from esp import Esp

class EspTests(unittest.TestCase):
    def test_transfer_checks_each_offset_before_committing(self):
        esp=object.__new__(Esp)
        esp.request=Mock(side_effect=['WFU READY 0','WFU READY 24','WFU READY 25','WFU STAGED','WFU REBOOTING'])
        data=bytes(range(25))
        esp.transfer(data,hashlib.sha256(data).hexdigest())
        self.assertEqual(esp.request.call_args_list[2].args[0],'WFU DATA 24 18')
        self.assertEqual(esp.request.call_args_list[-2].args[0],'WFU END')
        esp.request=Mock(side_effect=['WFU READY 0','WFU READY 23'])
        with self.assertRaises(ValueError):esp.transfer(data,'a'*64)
        self.assertEqual(esp.request.call_count,2)

    def test_serial_reply_ignores_diagnostics_and_accepts_fragmented_status(self):
        master,slave=os.openpty()
        path=os.ttyname(slave)
        esp=Esp(path)
        def device():
            os.read(master,512)
            os.write(master,b'noise\nWFU STATUS '+b'a'*64+b' valid ')
            os.write(master,b'healthy wf1-esp32s3-16mb\n')
        worker=threading.Thread(target=device,daemon=True);worker.start()
        try:self.assertEqual(esp.status()['health'],'healthy')
        finally:esp.close();os.close(master);os.close(slave);worker.join(1)
