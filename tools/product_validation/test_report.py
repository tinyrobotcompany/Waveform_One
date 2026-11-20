import csv
import math
from pathlib import Path
import tempfile
import unittest
from report import load_trials, summarize, wilson, percentile, cost, NUMBERS
class ReportTests(unittest.TestCase):
 def row(self,**kwargs):
  return dict(dict(run_id='run',trial_id='1',trial_type='acquisition',condition_id='quiet',corpus_recording_id='song',outcome='correct',any_wrong_answer=False,**{key:None for key in NUMBERS}),request_count=2,observation_duration_s=30,music_duration_s=30,**kwargs)
 def test_errors_in_denominator(self):
  a=self.row();b=self.row(outcome='service_error',corpus_recording_id='song2');result=summarize([a,b]);self.assertEqual(result['accuracy'],.5);self.assertEqual(result['answer_precision'],1);self.assertEqual(result['usage']['requests_per_music_hour'],240);self.assertIsNone(result['usage']['provider_billed_units'])
 def test_missing_is_not_zero(self):
  a=self.row(first_correct_ms=1000);b=self.row(outcome='no_match');value=summarize([a,b])['latency_ms']['first_correct_ms'];self.assertEqual(value['samples'],1);self.assertEqual(value['p50'],1000);self.assertEqual(value['missing_or_timeout'],1)
 def test_repeats_no_independent_interval(self):
  self.assertIsNone(summarize([self.row(),self.row()])['accuracy_wilson95'])
 def test_wilson_and_quantile(self):
  low,high=wilson(5,10);self.assertLess(low,.5);self.assertGreater(high,.5);self.assertEqual(percentile([10,20],.95),19.5);self.assertIsNone(percentile([],.5))
 def test_empty_rejected(self):
  with tempfile.TemporaryDirectory() as temp:
   path=Path(temp)/'trials.csv'
   with path.open('w',newline='') as f:csv.DictWriter(f,fieldnames=list(self.row())).writeheader()
   with self.assertRaises(ValueError):load_trials(path)
 def test_cost(self):
  plan=dict(monthly_fee=10,included_units=1000,overage_per_1000=2,devices=2,hours_per_day=1,days=30,requests_per_hour=100,usage_basis='assumption',currency='EUR')
  value=cost(plan);self.assertEqual(value['estimated_units'],6000);self.assertEqual(value['plan_cost'],20);self.assertFalse(value['terms_verified']);plan['hours_per_day']=float('inf')
  with self.assertRaises(ValueError):cost(plan)
if __name__=='__main__':unittest.main()
