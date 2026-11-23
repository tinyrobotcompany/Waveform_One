"""Keep custom P4 production modules in C++; vendor/generated C stays unchanged."""
from pathlib import Path
import re
import unittest

APP = Path(__file__).resolve().parents[2] / 'firmware/esp32/p4_display'

class FirmwareLanguageTests(unittest.TestCase):
    def test_custom_production_sources_use_cpp(self):
        cmake=(APP/'main/CMakeLists.txt').read_text().split('SRCS',1)[1].split('INCLUDE_DIRS',1)[0]
        c_sources=[name for name in re.findall(r'"([^"]+)"',cmake) if name.endswith('.c')]
        unexpected=[name for name in c_sources if not name.startswith(('fonts/','../third_party/'))]
        self.assertEqual(unexpected,[], 'Custom production code must compile as C++')
        for name in ('network_activity','update_policy','update_service','phone_tls','phone_dns'):
            self.assertTrue((APP/'main'/(name+'.cpp')).is_file())
