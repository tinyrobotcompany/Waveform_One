"""Offline parity check: native PCM front-end and unchanged quantized CNN."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly
from math import gcd
from ai_edge_litert.interpreter import Interpreter, OpResolverType


def verify(original, extracted, native, fixtures):
    full = Interpreter(model_path=str(original), experimental_preserve_all_tensors=True,
                       experimental_op_resolver_type=OpResolverType.BUILTIN_WITHOUT_DEFAULT_DELEGATES)
    cnn = Interpreter(model_path=str(extracted),
                      experimental_op_resolver_type=OpResolverType.BUILTIN_WITHOUT_DEFAULT_DELEGATES)
    full.allocate_tensors()
    cnn.allocate_tensors()
    source_input = full.get_input_details()[0]['index']
    target_input = cnn.get_input_details()[0]['index']
    target_output = cnn.get_output_details()[0]['index']
    rng = np.random.default_rng(42)
    samples = {'silence': np.zeros(15600, dtype='<i2'),
               'white-noise': rng.integers(-8000, 8000, 15600, dtype=np.int16),
               'tone': (12000 * np.sin(np.arange(15600) * (440 * 2 * np.pi / 16000))).astype('<i2')}
    for fixture in fixtures:
        audio, rate = sf.read(fixture, dtype='float32', always_2d=True)
        audio = audio.mean(axis=1)
        factor = gcd(rate, 16000)
        audio = resample_poly(audio, 16000 // factor, rate // factor)
        for index in range(len(audio) // 15600):
            samples[f'{fixture.stem}:{index}'] = np.clip(np.rint(audio[index*15600:(index+1)*15600] * 32768), -32768, 32767).astype('<i2')
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for name, pcm in samples.items():
            (root / 'pcm').write_bytes(pcm.tobytes())
            subprocess.run([str(native), str(root / 'pcm'), str(root / 'features')], check=True)
            actual = np.frombuffer((root / 'features').read_bytes(), dtype=np.int8).reshape(1, 96, 64, 1)
            full.set_tensor(source_input, pcm.astype(np.float32) / 32768)
            full.invoke()
            reference = full.get_tensor(32)
            expected = full.get_tensor(119)
            cnn.set_tensor(target_input, reference)
            cnn.invoke()
            np.testing.assert_array_equal(cnn.get_tensor(target_output), expected)
            error = np.abs(actual.astype(np.int16) - reference.astype(np.int16))
            assert error.max() <= 1, (name, error.max())
            assert np.count_nonzero(error) <= 62, (name, np.count_nonzero(error))
            cnn.set_tensor(target_input, actual)
            cnn.invoke()
            scores = (cnn.get_tensor(target_output).astype(np.int16).ravel() + 128) / 256
            score_error = np.max(np.abs(cnn.get_tensor(target_output).astype(np.int16) - expected.astype(np.int16))) / 256
            # Report score drift separately: one-step input rounding can be
            # amplified by this quantized network. Feature parity and unchanged
            # CNN weights do not by themselves establish classifier accuracy.
            print(f'{name}: feature differences={np.count_nonzero(error)}/6144 max={error.max()} score error={score_error:.6f} '
                  f'music={scores[132]:.3f} singing={scores[24]:.3f} rap={scores[31]:.3f} speech={scores[0]:.3f}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('extracted', type=Path)
    parser.add_argument('native', type=Path)
    parser.add_argument('fixtures', nargs='*', type=Path)
    args = parser.parse_args()
    verify(args.original, args.extracted, args.native, args.fixtures)
