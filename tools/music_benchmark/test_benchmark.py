import array
import importlib.util
from pathlib import Path
import tempfile
import unittest
import wave
spec=importlib.util.spec_from_file_location('benchmark',Path(__file__).with_name('benchmark.py'))
bench=importlib.util.module_from_spec(spec);spec.loader.exec_module(bench)
class BenchmarkTests(unittest.TestCase):
    def test_uncertainty_and_speech_do_not_immediately_drop_music(self):
        policy=bench.MusicDecision()
        self.assertFalse(policy.update(.8));self.assertTrue(policy.update(.8))
        self.assertTrue(policy.update(.02));self.assertTrue(policy.update(.3))
        self.assertTrue(policy.update(.9))
        for _ in range(4):self.assertTrue(policy.update(.0))
        self.assertFalse(policy.update(.0))
    def test_uncertain_hold_is_bounded(self):
        policy=bench.MusicDecision();policy.update(.8);policy.update(.8)
        for _ in range(8):self.assertTrue(policy.update(.3))
        self.assertFalse(policy.update(.3))
    def test_silence_ends_session_and_requires_fresh_evidence(self):
        policy=bench.MusicDecision();policy.update(.8);policy.update(.8)
        self.assertFalse(policy.update(.8,silence=True))
        self.assertFalse(policy.update(.8));self.assertTrue(policy.update(.8))
    def test_mixing_preserves_relative_rms_without_clipping(self):
        music=array.array('h',[10000,-10000]*100)
        speech=array.array('h',[10000,10000,-10000,-10000]*50)
        mixed=bench.mix(music,speech,6)
        self.assertEqual(len(music),len(mixed));self.assertLessEqual(max(abs(x) for x in mixed),30000)
        self.assertEqual(bench.mix(music,speech,0),bench.mix(music,speech,0))
        with self.assertRaises(ValueError):bench.mix(music,[],0)
    def test_wrong_wav_format_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'stereo.wav'
            with wave.open(str(path),'wb') as audio:
                audio.setnchannels(2);audio.setsampwidth(2);audio.setframerate(16000);audio.writeframes(b'\0'*100)
            with self.assertRaises(ValueError):bench.load_wav(path)
if __name__=='__main__':unittest.main()
