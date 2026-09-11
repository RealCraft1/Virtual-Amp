#include <iostream>
#include <vector>
#include <cmath>
#include <portaudio.h>

constexpr int SAMPLE_RATE = 96000;
constexpr int FRAMES_PER_BUFFER =  128;
//distortion stuff
constexpr float GAIN = 40.0f;
constexpr float MASTER = 0.5f;
//reverb stuff

constexpr float REVERB_MIX = 0.20f;      // Wet/Dry mix
constexpr float REVERB_FEEDBACK = 0.65f; // Tail length

constexpr int REVERB_LENGTH = SAMPLE_RATE / 4; // 250 ms

static std::vector<float> reverbBuffer(REVERB_LENGTH, 0.0f);
static int reverbPos = 0;

static int audioCallback(
    const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void*)
{
    const float* input = static_cast<const float*>(inputBuffer);
    float* output = static_cast<float*>(outputBuffer);

    if (!input)
    {
        for (unsigned long i = 0; i < framesPerBuffer; i++)
            output[i] = 0.0f;
        return paContinue;
    }

    for (unsigned long i = 0; i < framesPerBuffer; i++)
    {
        float sample = input[i];

        // Preamp gain
        sample *= GAIN;

        // Tube-like soft clipping
        sample = std::tanh(sample * 2.5f);

        // Simple speaker smoothing
        static float last = 0.0f;
        sample = 0.8f * sample + 0.2f * last;
        last = sample;

        // Master volume
        sample *= MASTER;

        // Reverb
        // Use one feedback delay per sample instead of recalculating the
        // entire reverb tail. This keeps the callback fast enough to avoid
        // audio dropouts, which can sound like the gain is cutting out.
        float reverbSample = reverbBuffer[reverbPos];
        reverbBuffer[reverbPos] = sample + reverbSample * REVERB_FEEDBACK;
        reverbPos = (reverbPos + 1) % REVERB_LENGTH;

        output[i] = sample * (1.0f - REVERB_MIX) + reverbSample * REVERB_MIX;
    }

    return paContinue;
}

int main()
{
    
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError)
    {
        std::cerr << "PortAudio initialization failed.\n";
        return 1;
    }

    std::cout << "Available devices:\n\n";

    int numDevices = Pa_GetDeviceCount();

    for (int i = 0; i < numDevices; i++)
    {
        const PaDeviceInfo* info =
            Pa_GetDeviceInfo(i);

        std::cout << i << ": "
                  << info->name << '\n';
    }

    int inputDevice;
    int outputDevice;

    std::cout << "\nInput device: ";
    std::cin >> inputDevice;

    std::cout << "Output device: ";
    std::cin >> outputDevice;

    PaStreamParameters inputParams{};
    inputParams.device = inputDevice;
    inputParams.channelCount = 1;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency =
        Pa_GetDeviceInfo(inputDevice)
            ->defaultLowInputLatency;

    PaStreamParameters outputParams{};
    outputParams.device = outputDevice;
    outputParams.channelCount = 1;
    outputParams.sampleFormat = paFloat32;
    outputParams.suggestedLatency =
        Pa_GetDeviceInfo(outputDevice)
            ->defaultLowOutputLatency;

    PaStream* stream;

    err = Pa_OpenStream(
        &stream,
        &inputParams,
        &outputParams,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,
        audioCallback,
        nullptr);

  if (err != paNoError)
{
    std::cerr << "PortAudio Error: "
              << Pa_GetErrorText(err)
              << std::endl;

    std::cout << "Press ENTER to exit...";
    std::cin.get();

    Pa_Terminate();
    return 1;
}
    Pa_StartStream(stream);

    std::cout << "\nVirtual Amp Running\n";
    std::cout << "Press ENTER to quit...\n";

    std::cin.ignore();
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}