# YAMNet model attribution

Google / TensorFlow Authors, YAMNet sound-event classifier, Apache License 2.0.
The upstream model listing identifies this artifact's license:
https://www.kaggle.com/models/google/yamnet/tfLite/classification-tflite/1?tfhub-redirect=true

Source artifact and SHA-256 are recorded in `provenance.json`.
`yamnet_int8.tflite` retains all CNN weights and all 521 output classes. Waveform
One removed the float waveform preprocessing operators and final dequantization
operator for use with the native C++ feature front-end and int8 microcontroller
runtime. The generated feature constants retain the source Hann/mel weights and
input quantization. The graph metadata was removed; tensor/buffer indices were
compacted. No retraining occurred.

Upstream architecture, feature definitions and class mapping:
https://github.com/tensorflow/models/tree/master/research/audioset/yamnet

The adjacent `LICENSE` contains the Apache License 2.0 text. Espressif's pinned
TFLite Micro dependency carries its own licenses in the managed component.
