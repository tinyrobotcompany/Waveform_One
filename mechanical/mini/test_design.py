import unittest
from dataclasses import replace
from design import Dimensions
class MiniTests(unittest.TestCase):
 def test_current_parts_fit_print_envelope(self):
  d=Dimensions().validate()
  self.assertAlmostEqual(d.width,211.8);self.assertAlmostEqual(d.height,242.8)
  self.assertLess(d.led_y+d.led_h/2,d.lcd_y-d.lcd_h/2)
  self.assertGreater(d.report()['comparison']['width_reduction_percent'],45)
  self.assertEqual(d.report()['status'],'UNVALIDATED_PROTOTYPE')
 def test_rejects_unprintable_or_inadequate_geometry(self):
  for change in ({'depth':25},{'wall':8},{'lcd_w':300},{'screen_window_h':120},{'lcd_d':float('nan')},{'separator':0}):
   with self.assertRaises(ValueError):replace(Dimensions(),**change).validate()
if __name__=='__main__':unittest.main()
