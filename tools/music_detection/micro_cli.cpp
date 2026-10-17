#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    std::ifstream model_file(argv[1], std::ios::binary), input_file(argv[2], std::ios::binary);
    std::vector<char> model_bytes((std::istreambuf_iterator<char>(model_file)), {});
    std::vector<char> input((std::istreambuf_iterator<char>(input_file)), {});
    if (model_bytes.empty() || input.size() != 6144) return 3;
    flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t *>(model_bytes.data()), model_bytes.size());
    if (!tflite::VerifyModelBuffer(verifier)) return 4;
    const auto *model = tflite::GetModel(model_bytes.data());
    tflite::MicroMutableOpResolver<5> resolver;
    if (resolver.AddConv2D() != kTfLiteOk || resolver.AddDepthwiseConv2D() != kTfLiteOk ||
        resolver.AddMean() != kTfLiteOk || resolver.AddFullyConnected() != kTfLiteOk ||
        resolver.AddLogistic() != kTfLiteOk) return 5;
    std::vector<uint8_t> arena(1024 * 1024);
    tflite::MicroInterpreter interpreter(model, resolver, arena.data(), arena.size());
    if (interpreter.AllocateTensors() != kTfLiteOk) return 6;
    auto *tensor = interpreter.input(0);
    if (tensor->type != kTfLiteInt8 || tensor->bytes != input.size()) return 7;
    std::copy(input.begin(), input.end(), tensor->data.int8);
    const auto start = std::chrono::steady_clock::now();
    if (interpreter.Invoke() != kTfLiteOk) return 8;
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    const auto *output = interpreter.output(0);
    if (output->type != kTfLiteInt8 || output->bytes != 521) return 9;
    std::ofstream file(argv[3], std::ios::binary);
    file.write(reinterpret_cast<const char *>(output->data.int8), output->bytes);
    std::fprintf(stderr, "arena=%zu bytes; inference=%lld us (host reference kernels)\n", interpreter.arena_used_bytes(), static_cast<long long>(elapsed));
    return file ? 0 : 10;
}
