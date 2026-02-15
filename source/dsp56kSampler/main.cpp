#include "dspHost.h"
#include "dspProgram.h"

#include "dsp56kEmu/dsp.h"
#include "dsp56kEmu/memory.h"
#include "dsp56kEmu/peripherals.h"
#include "dsp56kEmu/esai.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace
{
	// Write a minimal WAV file header and data
	void writeWav(const std::string& _filename, const float* _data, uint32_t _numFrames,
	              uint32_t _sampleRate = 48000, uint16_t _channels = 2)
	{
		const uint16_t bitsPerSample = 16;
		const uint32_t bytesPerSample = bitsPerSample / 8;
		const uint32_t dataSize = _numFrames * _channels * bytesPerSample;
		const uint32_t fileSize = 36 + dataSize;

		std::ofstream file(_filename, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "Failed to open " << _filename << " for writing" << std::endl;
			return;
		}

		// RIFF header
		file.write("RIFF", 4);
		const uint32_t chunkSize = fileSize - 8;
		file.write(reinterpret_cast<const char*>(&chunkSize), 4);
		file.write("WAVE", 4);

		// fmt sub-chunk
		file.write("fmt ", 4);
		const uint32_t fmtSize = 16;
		file.write(reinterpret_cast<const char*>(&fmtSize), 4);
		const uint16_t audioFormat = 1; // PCM
		file.write(reinterpret_cast<const char*>(&audioFormat), 2);
		file.write(reinterpret_cast<const char*>(&_channels), 2);
		file.write(reinterpret_cast<const char*>(&_sampleRate), 4);
		const uint32_t byteRate = _sampleRate * _channels * bytesPerSample;
		file.write(reinterpret_cast<const char*>(&byteRate), 4);
		const uint16_t blockAlign = _channels * bytesPerSample;
		file.write(reinterpret_cast<const char*>(&blockAlign), 2);
		file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

		// data sub-chunk
		file.write("data", 4);
		file.write(reinterpret_cast<const char*>(&dataSize), 4);

		// Convert float samples to 16-bit PCM and write
		const uint32_t totalSamples = _numFrames * _channels;
		for (uint32_t i = 0; i < totalSamples; ++i)
		{
			float s = _data[i];
			if (s > 1.0f) s = 1.0f;
			if (s < -1.0f) s = -1.0f;
			auto pcm = static_cast<int16_t>(s * 32767.0f);
			file.write(reinterpret_cast<const char*>(&pcm), 2);
		}

		file.close();
		std::cout << "Wrote " << _filename << " (" << _numFrames << " frames, "
		          << _sampleRate << " Hz, " << _channels << " ch)" << std::endl;
	}

	void dumpProgramMemory(dsp56k::Memory& _mem, uint32_t _start, uint32_t _count)
	{
		std::cout << "Program memory dump:" << std::endl;
		for (uint32_t i = 0; i < _count; ++i)
		{
			const auto addr = _start + i;
			const auto val = _mem.get(dsp56k::MemArea_P, addr);
			std::cout << "  P:$" << std::hex << std::setw(4) << std::setfill('0') << addr
			          << " = $" << std::setw(6) << std::setfill('0') << val
			          << std::dec << std::endl;
		}
	}
}

int main(const int argc, const char* argv[])
{
	const float frequency = 440.0f;
	const float sampleRate = 48000.0f;
	const uint32_t durationSamples = static_cast<uint32_t>(sampleRate * 2.0f); // 2 seconds
	const std::string outputFile = argc > 1 ? argv[1] : "sine_output.wav";

	std::cout << "DSP56300 Sampler - Phase 1: Sine Wave Test" << std::endl;
	std::cout << "Frequency: " << frequency << " Hz" << std::endl;
	std::cout << "Sample Rate: " << sampleRate << " Hz" << std::endl;
	std::cout << "Duration: " << (static_cast<float>(durationSamples) / sampleRate) << " seconds" << std::endl;
	std::cout << std::endl;

	// Create DSP host and load sine wave program
	dsp56kSampler::DspHost host;
	host.loadSineProgram(frequency, sampleRate);

	auto& dsp = host.getDSP();
	auto& mem = host.getMemory();

	// Dump key program addresses
	std::cout << "--- Program memory at key addresses ---" << std::endl;
	std::cout << "Reset vector (P:$0000-$0001):" << std::endl;
	dumpProgramMemory(mem, 0x0000, 2);
	std::cout << "ESAI TX vector (P:$0038-$0039):" << std::endl;
	dumpProgramMemory(mem, 0x0038, 2);
	std::cout << "Main program (P:$0040-$0042):" << std::endl;
	dumpProgramMemory(mem, 0x0040, 3);
	std::cout << "TX ISR (P:$0100-$0104):" << std::endl;
	dumpProgramMemory(mem, 0x0100, 5);

	// Check sine table
	std::cout << "\n--- Sine table (first 4 entries) ---" << std::endl;
	for (uint32_t i = 0; i < 4; ++i)
	{
		const auto val = mem.get(dsp56k::MemArea_Y, i);
		std::cout << "  Y:$" << std::hex << std::setw(4) << std::setfill('0') << i
		          << " = $" << std::setw(6) << std::setfill('0') << val << std::dec << std::endl;
	}

	// Run DSP synchronously for a number of cycles to see what happens
	std::cout << "\n--- Synchronous DSP execution (5000 instructions) ---" << std::endl;
	std::cout << "Initial PC: $" << std::hex << dsp.getPC().toWord() << std::dec << std::endl;

	uint64_t lastIctr = dsp.getInstructionCounter();
	for (int i = 0; i < 5000; ++i)
	{
		dsp.exec();

		// Print the first 10 instructions
		if (i < 10)
		{
			std::cout << "  Step " << i << ": PC=$" << std::hex << dsp.getPC().toWord()
			          << " ictr=" << std::dec << dsp.getInstructionCounter() << std::endl;
		}
	}

	// Check ESAI output buffer
	auto& esai = host.getPeriphX().getEsai();
	const auto outputSize = esai.getAudioOutputs().size();
	std::cout << "\nAfter 5000 instructions:" << std::endl;
	std::cout << "  PC: $" << std::hex << dsp.getPC().toWord() << std::dec << std::endl;
	std::cout << "  Instruction counter: " << dsp.getInstructionCounter() << std::endl;
	std::cout << "  ESAI output buffer size: " << outputSize << std::endl;

	// Run more instructions to reach at least one ESAI cycle (2133 default)
	for (int i = 0; i < 10000; ++i)
		dsp.exec();

	const auto outputSize2 = esai.getAudioOutputs().size();
	std::cout << "\nAfter 15000 instructions:" << std::endl;
	std::cout << "  PC: $" << std::hex << dsp.getPC().toWord() << std::dec << std::endl;
	std::cout << "  Instruction counter: " << dsp.getInstructionCounter() << std::endl;
	std::cout << "  ESAI output buffer size: " << outputSize2 << std::endl;

	if (outputSize2 == 0)
	{
		std::cerr << "\nERROR: No audio output after 15000 instructions! ESAI is not producing output." << std::endl;

		// Try even more
		for (int i = 0; i < 100000; ++i)
			dsp.exec();

		const auto outputSize3 = esai.getAudioOutputs().size();
		std::cout << "\nAfter 115000 instructions:" << std::endl;
		std::cout << "  PC: $" << std::hex << dsp.getPC().toWord() << std::dec << std::endl;
		std::cout << "  Instruction counter: " << dsp.getInstructionCounter() << std::endl;
		std::cout << "  ESAI output buffer size: " << outputSize3 << std::endl;

		if (outputSize3 == 0)
		{
			std::cerr << "ESAI still not producing output. Aborting." << std::endl;
			return 1;
		}
	}

	std::cout << "\nESAI producing output! Switching to threaded mode..." << std::endl;

	// Feed audio input and start threaded execution
	host.feedAudioInput(durationSamples + 1024);
	host.start();

	// Read audio output
	std::vector<float> audioBuffer(durationSamples * 2); // stereo
	host.readAudio(audioBuffer.data(), durationSamples);

	// Stop DSP
	host.stop();

	std::cout << "DSP execution complete." << std::endl;

	// Quick analysis: find peak amplitude
	float peak = 0.0f;
	for (uint32_t i = 0; i < durationSamples * 2; ++i)
	{
		const float abs = audioBuffer[i] < 0 ? -audioBuffer[i] : audioBuffer[i];
		if (abs > peak)
			peak = abs;
	}
	std::cout << "Peak amplitude: " << peak << std::endl;

	// Write WAV file
	writeWav(outputFile, audioBuffer.data(), durationSamples, static_cast<uint32_t>(sampleRate));

	// Verify: check if we got a non-silent signal
	if (peak < 0.01f)
	{
		std::cerr << "WARNING: Output appears to be silent!" << std::endl;
		return 1;
	}

	std::cout << "Success! Audio output written to " << outputFile << std::endl;
	return 0;
}
