import unittest
from worker import metadata, wav_clip, RecognitionState
import io, wave
class RecognitionTests(unittest.TestCase):
    def test_match_extracts_album_and_safe_artwork(self):
        result={'track':{'title':'Song','subtitle':'Artist','images':{'coverart':'https://example.org/art.jpg'},'sections':[{'metadata':[{'title':'Album','text':'Record'}]}]}}
        self.assertEqual(metadata(result),dict(title='Song',artist='Artist',album='Record',artwork_url='https://example.org/art.jpg'))
        result['track']['images']['coverart']='javascript:evil'
        self.assertIsNone(metadata(result)['artwork_url'])
        self.assertIsNone(metadata({'matches':[]}))
    def test_pcm_becomes_eight_second_mono_wave(self):
        with wave.open(io.BytesIO(wav_clip(bytes(256000)))) as audio:
            self.assertEqual((audio.getframerate(),audio.getnchannels(),audio.getsampwidth(),audio.getnframes()),(16000,1,2,128000))
        with self.assertRaises(ValueError):wav_clip(b'partial')
    def test_expiry_silence_and_session_change_clear_matches(self):
        s=RecognitionState();s.begin(3,100);s.finish({'title':'Song'},3,110)
        self.assertEqual(s.view(3,130)['track']['title'],'Song')
        self.assertIsNone(s.view(3,201)['track'])
        s.begin(4,202);s.finish({'title':'Old song'},3,203)
        self.assertIsNone(s.view(4,203)['track'])
        s.clear();self.assertIsNone(s.view(4,204)['track'])
    def test_no_match_clears_previous_title_and_errors_back_off(self):
        s=RecognitionState();s.begin(1,0);s.finish({'title':'First'},1,1)
        s.begin(1,30);s.finish(None,1,40)
        self.assertIsNone(s.view(1,40)['track'])
        s.fail(40);self.assertGreaterEqual(s.next_attempt,100)
        s.fail(100);self.assertGreaterEqual(s.next_attempt,220)
    def test_next_capture_starts_two_seconds_after_a_result(self):
        s=RecognitionState();s.begin(1,0);s.finish({'title':'Song'},1,10)
        self.assertEqual(s.next_attempt,12)
        s.begin(1,12);s.finish(None,1,22)
        self.assertEqual(s.next_attempt,24)

    def test_disconnect_does_not_cancel_failure_backoff(self):
        s=RecognitionState();s.begin(1,0);s.fail(10)
        deadline=s.next_attempt
        s.clear()  # ESP reconnects after capture failure
        self.assertEqual(s.next_attempt,deadline)
        s.fail(20)
        self.assertGreaterEqual(s.next_attempt,140)
if __name__=='__main__':unittest.main()
