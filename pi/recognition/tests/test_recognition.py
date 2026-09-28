import unittest
from worker import metadata, wav_clip, RecognitionState, offer_latest, recognition_clip
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
    def test_matches_have_bounded_retention_and_clear_on_session_change(self):
        s=RecognitionState();s.begin(3,100);s.finish({'title':'Song'},3,110)
        self.assertEqual(s.view(3,130)['track']['title'],'Song')
        self.assertIsNone(s.view(3,201)['track'])
        s.begin(4,202);s.finish({'title':'Old song'},3,203)
        self.assertIsNone(s.view(4,203)['track'])
        s.clear();self.assertIsNone(s.view(4,204)['track'])
    def test_no_match_preserves_previous_title_and_errors_back_off(self):
        s=RecognitionState();s.begin(1,0);s.finish({'title':'First'},1,1)
        s.begin(1,30);s.finish(None,1,40)
        self.assertEqual(s.view(1,40)['track']['title'],'First')
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
    def test_same_track_missing_art_preserves_cover_but_new_track_replaces_it(self):
        s=RecognitionState();s.begin(1,0)
        s.finish({'title':'First','artist':'Artist','artwork_url':'https://example.org/a.jpg'},1,1)
        s.finish({'title':'First','artist':'Artist','artwork_url':None},1,20)
        self.assertEqual(s.view(1,20)['track']['artwork_url'],'https://example.org/a.jpg')
        s.finish({'title':'Second','artist':'Artist','artwork_url':None},1,30)
        self.assertIsNone(s.view(1,30)['track']['artwork_url'])
    def test_short_audio_and_bounded_latest_queue(self):
        import asyncio
        with wave.open(io.BytesIO(wav_clip(bytes(64000)))) as audio:
            self.assertEqual(audio.getnframes(),32000)
        from queue import Queue
        q=Queue(maxsize=1)
        offer_latest(q,(1,b'old'));offer_latest(q,(1,b'new'))
        self.assertEqual(q.qsize(),1)
        self.assertEqual(q.get_nowait(),(1,b'new'))

    def test_fallback_uses_only_contiguous_same_session_clips(self):
        a=(1,10.,12.,b'a'*64000)
        b=(1,12.1,14.1,b'b'*64000)
        self.assertEqual(recognition_clip(a,b,True),a[3]+b[3])
        self.assertEqual(recognition_clip(a,b,False),b[3])
        self.assertEqual(recognition_clip(a,(2,*b[1:]),True),b[3])
        self.assertEqual(recognition_clip(a,(1,15.,17.,b[3]),True),b[3])
    def test_repeated_no_matches_clear_old_song_even_with_tv_keeping_session_active(self):
        s=RecognitionState();s.begin(1,0);s.finish({'title':'Gloria'},1,1)
        s.finish(None,1,4);self.assertIsNotNone(s.view(1,4)['track'])
        s.finish(None,1,7);self.assertIsNotNone(s.view(1,7)['track'])
        s.finish(None,1,12);self.assertIsNone(s.view(1,12)['track'])
        s.finish({'title':'Next'},1,14);self.assertEqual(s.view(1,14)['track']['title'],'Next')
        s.fail(15);self.assertIsNotNone(s.view(1,20)['track'])
        self.assertIsNone(s.view(1,60)['track'])

if __name__=='__main__' :unittest.main()

class PipelineTests(unittest.IsolatedAsyncioTestCase):
    async def test_next_capture_runs_during_identification_and_result_is_published(self):
        import asyncio, threading
        from worker import fast_run
        state=RecognitionState()
        identifying=threading.Event();second=threading.Event();stop=threading.Event()
        published=asyncio.Event();finished=asyncio.Event();calls=[]
        async def device():return {'phase':'playing','session':7}
        def request(path,post):
            calls.append(path)
            if len(calls)==2:
                assert identifying.wait(2)
                second.set()
            elif len(calls)>2:
                stop.wait(.1)
            return bytes(64000)
        class Recognizer:
            async def recognize(self,wav):
                identifying.set()
                assert await asyncio.to_thread(second.wait,2)
                return {'track':{'title':'Gloria','subtitle':'U2'}}
        def publish(session):
            if state.track:published.set();finished.set()
        task=asyncio.create_task(fast_run(state,Recognizer(),device,request,publish,finished))
        try:
            await asyncio.wait_for(published.wait(),3)
            self.assertEqual(state.track['title'],'Gloria')
            self.assertTrue(all(p=='/api/capture/short' for p in calls))
        finally:
            stop.set();finished.set()
            await asyncio.wait_for(task,3)
