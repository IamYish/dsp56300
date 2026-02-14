#include "sampleManager.h"
#include "wavLoader.h"

#include <algorithm>

namespace sfz
{
    int SampleManager::loadInstrumentSamples(Instrument& instrument,
                                             const std::filesystem::path& basePath)
    {
        int loaded = 0;

        for (auto& region : instrument.regions)
        {
            if (region.sample.empty())
                continue;

            int index = loadSample(region.sample, basePath);
            if (index >= 0)
            {
                region.sampleIndex = index;
                ++loaded;
            }
        }

        return loaded;
    }

    int SampleManager::loadSample(const std::string& relativePath,
                                  const std::filesystem::path& basePath)
    {
        // Normalize the path
        auto fullPath = basePath / relativePath;
        std::string normalizedPath = fullPath.string();

        // Replace backslashes with forward slashes for consistent hashing
        std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        // Check cache
        auto it = m_pathToIndex.find(normalizedPath);
        if (it != m_pathToIndex.end())
            return it->second;

        // Load the WAV file
        auto sample = WavLoader::load(normalizedPath);
        if (!sample)
        {
            // Try with original path (might be absolute already)
            sample = WavLoader::load(relativePath);
            if (!sample)
                return -1;
        }

        // Add to collection
        const int index = static_cast<int>(m_samples.size());
        m_samples.push_back(std::move(sample));
        m_pathToIndex[normalizedPath] = index;

        return index;
    }

    const SampleData* SampleManager::getSample(int index) const
    {
        if (index < 0 || index >= static_cast<int>(m_samples.size()))
            return nullptr;
        return m_samples[index].get();
    }

    size_t SampleManager::getMemoryUsage() const
    {
        size_t total = 0;
        for (const auto& sample : m_samples)
        {
            if (sample)
                total += sample->getDataSize() * sizeof(float);
        }
        return total;
    }

    void SampleManager::clear()
    {
        m_samples.clear();
        m_pathToIndex.clear();
    }

} // namespace sfz
