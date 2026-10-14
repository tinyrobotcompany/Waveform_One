"""Silent, file-only recognition benchmark. Never opens audio devices or networks."""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import random
import statistics
import subprocess
import sys
import time
import wave

RATE = 16000
WINDOW = 15600
HOP = 7800


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_wav(path):
    with wave.open(str(path)) as audio:
        if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate(), audio.getcomptype()) != (1, 2, RATE, 'NONE'):
            raise ValueError('Expected uncompressed mono 16-bit 16 kHz WAV: ' + str(path))
        data = array.array('h', audio.readframes(audio.getnframes()))
    if sys.byteorder != 'little':
        data.byteswap()
    return data


def pcm_bytes(samples):
    data = array.array('h', samples)
    if sys.byteorder != 'little':
        data.byteswap()
    return data.tobytes()


def rms(samples):
    return math.sqrt(sum(float(x) * x for x in samples) / max(1, len(samples))) / 32768


def mix(music, speech, relative_db):
    if not speech or not music:
        raise ValueError('Mix requires nonempty inputs')
    gain = rms(music) / max(rms(speech), 1e-12) * 10 ** (relative_db / 20)
    mixed = [float(x) + speech[i % len(speech)] * gain for i, x in enumerate(music)]
    peak = max(1, max(abs(x) for x in mixed))
    scale = min(1, 30000 / peak)
    return array.array('h', (round(x * scale) for x in mixed))


class MusicDecision:
    """Reference policy only; thresholds are proposals, not calibrated probabilities."""
    def __init__(self):
        self.active = False
        self.positive = 0
        self.negative = 0
        self.uncertain = 0

    def update(self, music, silence=False):
        if silence:
            self.active = False
            self.positive = self.negative = self.uncertain = 0
        elif music >= .5:
            self.positive += 1
            self.negative = self.uncertain = 0
            if self.positive >= 2:
                self.active = True
        else:
            self.positive = 0
            self.uncertain += 1
            if music < .15:
                self.negative += 1
                if self.negative * HOP / RATE >= 2:
                    self.active = False
            else:
                self.negative = 0
            if self.uncertain * HOP / RATE >= 4:
                self.active = False
        return self.active


def build_fingerprints(root, output):
    # Freeze the eight-second reference so later firmware changes cannot rewrite
    # the benchmark baseline or shrink its accepted input lengths.
    baseline_commit = '29f8bcd35256b6f07f382117c8ca9d689243ca40'
    main = output / 'generated' / 'baseline-source'
    main.mkdir(parents=True, exist_ok=True)
    for name in ('fingerprint.cpp', 'fingerprint.h', 'audio_capture.h'):
        source = subprocess.check_output(['git', 'show', baseline_commit + ':firmware/esp32/p4_display/main/' + name], cwd=root)
        (main / name).write_bytes(source)
    original = (main / 'fingerprint.cpp').read_text()
    guard = 'if (pcm == nullptr || bytes != audio_capture::kBytes) return false;'
    if original.count(guard) != 1:
        raise ValueError('Baseline input guard changed; review the experimental adapter')
    experiment = original.replace(guard, 'if (pcm == nullptr || bytes < 64000 || bytes > 256000 || bytes % 256 != 0) return false;')
    generated = output / 'generated'
    generated.mkdir(exist_ok=True)
    adapted = generated / 'fingerprint-short.cpp'
    adapted.write_text(experiment)
    executables = {}
    for name, source in [('baseline', main / 'fingerprint.cpp'), ('short', adapted)]:
        executable = generated / ('fingerprint-' + name)
        command = ['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(main),
                   str(root / 'tools/music_benchmark/fingerprint_cli.cpp'), str(source), '-o', str(executable)]
        subprocess.run(command, check=True, capture_output=True)
        executables[name] = executable
    return executables, {'baseline_commit': baseline_commit, 'baseline_source_sha256': digest(main / 'fingerprint.cpp'),
                         'adapter_source_sha256': digest(adapted),
                         'adapter_change': 'Input-length guard only, in generated source; historical eight-second baseline'}


def fingerprints(root, output, cases):
    executables, provenance = build_fingerprints(root, output)
    rows = []
    for case in cases:
        audio = load_wav(case['wav'])
        for offset in (0, 8, 16):
            for seconds in (2, 3, 4, 6, 8):
                samples = audio[offset * RATE:(offset + seconds) * RATE]
                if len(samples) != seconds * RATE:
                    continue
                clip = output / 'generated' / 'current-clip.pcm'
                clip.write_bytes(pcm_bytes(samples))
                results = [json.loads(subprocess.check_output([str(executables['short']), str(clip)], text=True)) for _ in range(3)]
                row = {'case': case['id'], 'offset_s': offset, 'clip_s': seconds,
                       'pcm_sha256': digest(clip), 'accepted': results[0]['accepted']}
                if row['accepted']:
                    value = results[0]
                    if any(r['uri'] != value['uri'] for r in results):
                        raise AssertionError('Nondeterministic fingerprint')
                    row.update(samples=value['samples'], fingerprint_audio_s=value['samples'] / RATE,
                               peaks=value['peaks'], cpu_ms=statistics.median(r['cpu_ms'] for r in results),
                               signature_sha256=hashlib.sha256(value['uri'].encode()).hexdigest())
                    signatures = output / 'signatures'
                    signatures.mkdir(exist_ok=True)
                    (signatures / (case['id'] + '-' + str(offset) + '-' + str(seconds) + '.json')).write_text(json.dumps(value))
                    if seconds == 8:
                        baseline = json.loads(subprocess.check_output([str(executables['baseline']), str(clip)], text=True))
                        row['baseline_equal'] = baseline['uri'] == value['uri'] and baseline['samples'] == value['samples']
                        if not row['baseline_equal']:
                            raise AssertionError('Eight-second adapter differs from historical eight-second baseline')
                rows.append(row)
        print('Fingerprint cases complete:', case['id'], flush=True)
    return {'provenance': provenance, 'rows': rows}


def classification(output, cases, args):
    labels = Path(args.labels).read_text().splitlines()
    if len(labels) != 521 or labels[132] != 'Music':
        raise ValueError('Unexpected model class map')
    if digest(args.model) != '4117ec5b22de5e6187c9c28f6aa83783debb53076f94fd693f4e6f3d73f64c70':
        raise ValueError('Unexpected reference model')
    datasets = [(case['id'], 'music', load_wav(case['wav'])) for case in cases]
    speech = load_wav(args.speech)
    datasets.append(('speech', 'non_music', speech))
    if args.non_music_manifest:
        for case in json.loads(Path(args.non_music_manifest).read_text()):
            if digest(case['wav']) != case['wav_sha256']:
                raise ValueError('Noise fixture checksum mismatch: '+case['id'])
            datasets.append((case['id'], 'non_music', load_wav(case['wav'])))
    datasets.append(('silence', 'non_music', array.array('h', [0]) * (8 * RATE)))
    random_source = random.Random(20261012)
    datasets.append(('white_noise', 'non_music', array.array('h', (random_source.randrange(-3000, 3001) for _ in range(8 * RATE)))))
    for case in cases:
        if case['id'] not in ('jazz', 'classical', 'rock'):
            continue
        music = load_wav(case['wav'])[:12 * RATE]
        datasets.append((case['id'] + '_quiet', 'music', array.array('h', (round(x * .1) for x in music))))
        for relative_db in (-6, 0, 6):
            datasets.append((case['id'] + '_speech_' + str(relative_db), 'music', mix(music, speech, relative_db)))
    jazz = next(load_wav(c['wav']) for c in cases if c['id']=='jazz')
    datasets.append(('jazz_then_speech', 'transition', jazz[:6*RATE] + array.array('h', (speech[i % len(speech)] for i in range(6*RATE)))))
    datasets.append(('jazz_then_silence', 'transition', jazz[:6*RATE] + array.array('h', [0])*(6*RATE)))
    rows = []
    summary = []
    generated = output / 'generated'
    for name, expected, data in datasets:
        state = MusicDecision()
        windows = []
        for start in range(0, len(data) - WINDOW + 1, HOP):
            window = data[start:start + WINDOW]
            clip, features, scores_file = (generated / ('classifier.' + suffix) for suffix in ('pcm', 'features', 'scores'))
            clip.write_bytes(pcm_bytes(window))
            begun = time.perf_counter()
            subprocess.run([args.extractor, str(clip), str(features), '--normalize'], check=True, capture_output=True)
            subprocess.run([args.classifier, args.model, str(features), str(scores_file)], check=True, capture_output=True)
            cpu_ms = (time.perf_counter() - begun) * 1000
            values = array.array('b', scores_file.read_bytes())
            if len(values) != 521:
                raise ValueError('Wrong inference output size')
            scores = [(v + 128) / 256 for v in values]
            legacy_music = max(scores[132], scores[24], scores[31])
            broad_music = max(scores[24:33] + scores[132:277])
            top = max(range(521), key=lambda i: scores[i])
            active = state.update(broad_music, rms(window) < 1e-5)
            truth = expected if expected != 'transition' else 'music' if start+WINDOW<=6*RATE else 'non_music' if start>=6*RATE else 'boundary'
            row = dict(case=name, expected=truth, window_end_s=(start + WINDOW) / RATE,
                       music=round(broad_music, 6), legacy_music=round(legacy_music, 6),
                       speech=round(max(scores[:4]), 6), top=labels[top], cpu_ms=round(cpu_ms, 3), active=active)
            rows.append(row); windows.append(row)
        positive = [w for w in windows if w['active']]
        drops = sum(windows[i-1]['active'] and not windows[i]['active'] for i in range(1, len(windows)))
        summary.append(dict(case=name, expected=expected, windows=len(windows),
                            active_windows=len(positive), first_on_s=positive[0]['window_end_s'] if positive else None,
                            dropouts=drops, first_off_after_stop_s=next((w['window_end_s']-6 for w in windows if w['window_end_s']>=6 and not w['active']), None) if expected=='transition' else None,
                            legacy_positive_windows=sum(w['legacy_music'] >= .5 for w in windows),
                            broad_positive_windows=sum(w['music'] >= .5 for w in windows),
                            host_pipeline_ms=round(statistics.median(w['cpu_ms'] for w in windows), 3) if windows else None))
        (output / 'classification-progress.json').write_text(json.dumps(summary, indent=2))
        print('Classification complete:', name, len(positive), '/', len(windows), 'active', flush=True)
    return dict(provenance={'model_sha256': digest(args.model), 'labels_sha256': digest(args.labels),
                            'extractor_sha256': digest(args.extractor), 'classifier_sha256': digest(args.classifier),
                            'speech_sha256': digest(args.speech)}, rows=rows, summary=summary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--non-music-manifest')
    parser.add_argument('--extractor'); parser.add_argument('--classifier')
    parser.add_argument('--model'); parser.add_argument('--labels'); parser.add_argument('--speech')
    args = parser.parse_args()
    optional = (args.extractor, args.classifier, args.model, args.labels, args.speech)
    if any(optional) and not all(optional):
        parser.error('Classification requires extractor, classifier, model, labels and speech together')
    root = Path(__file__).resolve().parents[2]
    output = Path(args.output).resolve(); output.mkdir(parents=True, exist_ok=True)
    cases = json.loads(Path(args.manifest).read_text())
    for case in cases:
        if digest(case['wav']) != case['wav_sha256']:
            raise ValueError('Fixture checksum mismatch: ' + case['id'])
    results = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
               'fixtures': cases, 'non_music_fixtures': json.loads(Path(args.non_music_manifest).read_text()) if args.non_music_manifest else [], 'fingerprints': fingerprints(root, output, cases),
               'limitations': ['CPU times are MacBook measurements, not MCU timings.',
                               'Peak counts are not catalogue matches; no external service requests are made.',
                               'Classification is a reference experiment, not a validated deployable filter.',
                               'No audio playback, microphone access or hardware interaction.']}
    (output / 'results.json').write_text(json.dumps(results, indent=2))
    if all(optional):
        results['classification'] = classification(output, cases, args)
    (output / 'results.json').write_text(json.dumps(results, indent=2))
    print('Results:', output / 'results.json', flush=True)


if __name__ == '__main__':
    main()
