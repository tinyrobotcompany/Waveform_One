"""Offline fixture/mixture evaluation; no room recordings or network requests."""
import argparse
from math import gcd
from pathlib import Path
import subprocess
import tempfile
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly
from ai_edge_litert.interpreter import Interpreter, OpResolverType


def audio(path):
    signal, rate = sf.read(path, dtype='float32', always_2d=True)
    signal = signal.mean(axis=1)
    factor = gcd(rate, 16000)
    return resample_poly(signal, 16000 // factor, rate // factor)


def evaluate(model, native, micro, speech, music, output):
    interpreter = Interpreter(model_path=str(model), experimental_op_resolver_type=OpResolverType.BUILTIN_WITHOUT_DEFAULT_DELEGATES)
    interpreter.allocate_tensors()
    voice = audio(speech)
    rng = np.random.default_rng(42)
    cases = {'speech': voice, 'silence': np.zeros(15600 * 8),
             'white-noise': rng.normal(0, .05, 15600 * 8)}
    for fixture in music:
        signal = audio(fixture)
        cases[fixture.stem] = signal
        # Equal and louder speech are deliberately difficult mixtures.
        tiled = np.resize(voice, len(signal))
        tiled *= np.sqrt(np.mean(signal**2) / np.mean(tiled**2))
        for ratio in (.5, 1., 2.):
            mix = signal + tiled * ratio
            cases[f'{fixture.stem}+speech-{ratio:g}'] = mix / max(1, np.max(np.abs(mix)))
        cases[f'{fixture.stem}-quiet'] = signal * .01
    lines = ['case\twindows\tmusic_strong\tmusic_weak\tother\tmin_music_score\tmax_music_score\tmax_micro_score_difference']
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for name, signal in cases.items():
            scores, errors = [], []
            # At most 32 evenly spaced windows per fixture; deterministic.
            windows = len(signal) // 15600
            for index in np.unique(np.linspace(0, windows - 1, min(32, windows), dtype=int)):
                pcm = np.clip(np.rint(signal[index*15600:(index+1)*15600] * 32768), -32768, 32767).astype('<i2')
                (root / 'pcm').write_bytes(pcm.tobytes())
                subprocess.run([str(native), str(root / 'pcm'), str(root / 'features'), '--normalize'], check=True)
                features = np.frombuffer((root / 'features').read_bytes(), dtype=np.int8).reshape(1,96,64,1)
                interpreter.set_tensor(interpreter.get_input_details()[0]['index'], features)
                interpreter.invoke()
                reference = interpreter.get_tensor(interpreter.get_output_details()[0]['index']).astype(np.int16).ravel()
                subprocess.run([str(micro), str(model), str(root / 'features'), str(root / 'scores')], check=True, stderr=subprocess.DEVNULL)
                result = np.frombuffer((root / 'scores').read_bytes(), dtype=np.int8).astype(np.int16)
                errors.append(np.max(np.abs(result - reference)) / 256)
                scores.append((max(result[132], result[24], result[31]) + 128) / 256)
            score = np.asarray(scores)
            row = f'{name}\t{len(score)}\t{np.sum(score >= .5)}\t{np.sum((score > .2) & (score < .5))}\t{np.sum(score <= .2)}\t{min(score):.6f}\t{max(score):.6f}\t{max(errors):.6f}'
            lines.append(row)
            print(row, flush=True)
    output.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('model', type=Path)
    parser.add_argument('native', type=Path)
    parser.add_argument('micro', type=Path)
    parser.add_argument('speech', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('music', nargs='+', type=Path)
    args = parser.parse_args()
    evaluate(args.model, args.native, args.micro, args.speech, args.music, args.output)
