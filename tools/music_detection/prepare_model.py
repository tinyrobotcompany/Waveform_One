"""Extract the unchanged int8 YAMNet CNN and native front-end constants.

Offline development tool; never runs on the device. Input is Google's pinned
classification model. No training, class removal or weight changes take place.
"""
import argparse
import hashlib
import json
from pathlib import Path
import flatbuffers
import numpy as np
from ai_edge_litert import schema_py_generated as schema


def prepare(source, output):
    raw = source.read_bytes()
    expected = '10c95ea3eb9a7bb4cb8bddf6feb023250381008177ac162ce169694d05c317de'
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError('Source model does not match the pinned Google artifact')
    model = schema.ModelT.InitFromObj(schema.Model.GetRootAsModel(raw, 0))
    graph = model.subgraphs[0]
    assert len(model.subgraphs) == 1 and len(graph.operators) == 47
    assert graph.tensors[32].shape.tolist() == [1, 96, 64, 1]
    assert graph.tensors[119].shape.tolist() == [1, 521]
    assert model.operatorCodes[graph.operators[16].opcodeIndex].builtinCode == schema.BuiltinOperator.CONV_2D
    assert model.operatorCodes[graph.operators[-1].opcodeIndex].builtinCode == schema.BuiltinOperator.DEQUANTIZE

    def floats(index):
        return np.frombuffer(model.buffers[graph.tensors[index].buffer].data, dtype='<f4')

    hann = floats(11)
    mel = floats(22).reshape(64, 257)
    bias = floats(23)
    scale = float(graph.tensors[32].quantization.scale[0])
    zero = int(graph.tensors[32].quantization.zeroPoint[0])
    lines = ['// Generated from the pinned Google YAMNet model; see model provenance.',
             '#pragma once', '#include <cstdint>', 'namespace music_detection::constants {',
             f'inline constexpr float kInputScale = {scale:.9g}f;',
             f'inline constexpr int kInputZero = {zero};',
             'inline constexpr float kHann[400] = {',
             ',\n'.join(f'{value:.9e}f' for value in hann), '};',
             'inline constexpr float kMelBias[64] = {',
             ', '.join(f'{value:.9e}f' for value in bias), '};',
             'struct MelBin { uint8_t first, second; float first_weight, second_weight; };',
             'inline constexpr MelBin kMel[257] = {']
    for index in range(257):
        active = np.flatnonzero(mel[:, index])
        assert len(active) <= 2
        entries = [(int(i), float(mel[i, index])) for i in active]
        entries += [(0, 0.0)] * (2 - len(entries))
        (a, aw), (b, bw) = entries
        lines.append(f'{{{a}, {b}, {aw:.9e}f, {bw:.9e}f}},')
    lines += ['};', '} // namespace music_detection::constants', '']
    output.mkdir(parents=True, exist_ok=True)
    (output / 'music_feature_constants.h').write_text('\n'.join(lines))

    # Remove the unsupported float STFT front-end; retain its exact quantization.
    graph.operators = graph.operators[16:-1]
    graph.inputs = np.array([32], dtype=np.int32)
    graph.outputs = np.array([119], dtype=np.int32)
    tensors = {32, 119}
    for op in graph.operators:
        tensors.update(int(i) for i in op.inputs if i >= 0)
        tensors.update(int(i) for i in op.outputs if i >= 0)
    ordered = sorted(tensors)
    remap = {old: new for new, old in enumerate(ordered)}
    graph.tensors = [graph.tensors[i] for i in ordered]
    graph.inputs = np.array([remap[32]], dtype=np.int32)
    graph.outputs = np.array([remap[119]], dtype=np.int32)
    for op in graph.operators:
        op.inputs = np.array([remap[int(i)] if i >= 0 else -1 for i in op.inputs], dtype=np.int32)
        op.outputs = np.array([remap[int(i)] for i in op.outputs], dtype=np.int32)
    buffers = sorted({0} | {int(t.buffer) for t in graph.tensors})
    buffer_remap = {old: new for new, old in enumerate(buffers)}
    model.buffers = [model.buffers[i] for i in buffers]
    for tensor in graph.tensors:
        tensor.buffer = buffer_remap[int(tensor.buffer)]
    codes = sorted({int(op.opcodeIndex) for op in graph.operators})
    code_remap = {old: new for new, old in enumerate(codes)}
    model.operatorCodes = [model.operatorCodes[i] for i in codes]
    for op in graph.operators:
        op.opcodeIndex = code_remap[int(op.opcodeIndex)]
    model.metadata = None
    model.metadataBuffer = None
    model.signatureDefs = None
    model.description = b'Waveform One: unchanged int8 YAMNet CNN; native feature front-end'
    builder = flatbuffers.Builder(4 * 1024 * 1024)
    root = model.Pack(builder)
    builder.Finish(root, file_identifier=b'TFL3')
    result = bytes(builder.Output())
    (output / 'yamnet_int8.tflite').write_bytes(result)
    info = dict(source_url='https://storage.googleapis.com/download.tensorflow.org/models/tflite/task_library/audio_classification/rpi/lite-model_yamnet_classification_tflite_1.tflite',
                source_sha256=hashlib.sha256(raw).hexdigest(), bytes=len(result),
                model_sha256=hashlib.sha256(result).hexdigest(), input_scale=scale,
                input_zero=zero, classes=521, sample_rate=16000, samples=15600)
    (output / 'provenance.json').write_text(json.dumps(info, indent=2) + '\n')
    print(json.dumps(info, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.output)
