#pragma once

#include "sample/sampleData.h"
#include "sfz/instrument.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <filesystem>

namespace sfz
{
    // Manages loading, caching, and lifetime of audio samples.
    // Samples are loaded on demand and cached by file path.
    // Thread-safe: samples are loaded on a background thread and become
    // available to the audio thread once complete.
    class SampleManager
    {
    public:
        SampleManager() = default;
        ~SampleManager() = default;

        // Non-copyable
        SampleManager(const SampleManager&) = delete;
        SampleManager& operator=(const SampleManager&) = delete;

        // Load all samples referenced by an instrument.
        // basePath is the directory of the .sfz file.
        // Returns the number of samples successfully loaded.
        int loadInstrumentSamples(Instrument& instrument,
                                  const std::filesystem::path& basePath);

        // Load a single sample by file path (relative to basePath).
        // Returns the sample index, or -1 on failure.
        int loadSample(const std::string& relativePath,
                       const std::filesystem::path& basePath);

        // Get a loaded sample by index.
        const SampleData* getSample(int index) const;

        // Get total number of loaded samples.
        size_t getSampleCount() const { return m_samples.size(); }

        // Get total memory usage of all loaded samples (in bytes).
        size_t getMemoryUsage() const;

        // Clear all cached samples.
        void clear();

    private:
        // Loaded samples (indexed by integer for O(1) lookup from audio thread)
        std::vector<std::unique_ptr<SampleData>>    m_samples;

        // Path -> index cache (used during loading to avoid duplicates)
        std::unordered_map<std::string, int>        m_pathToIndex;
    };

} // namespace sfz
