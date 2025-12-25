#pragma once

#include <algorithm>
#include <string>
#include <cctype>

namespace Aetherion::Core::String
{
    inline std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    inline bool HasSuffix(const std::string& value, const std::string& suffix)
    {
        if (value.size() < suffix.size())
        {
            return false;
        }
        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    inline bool ContainsCaseInsensitive(const std::string& value, const std::string& token)
    {
        if (value.empty() || token.empty()) return false;
        std::string loweredValue = ToLower(value);
        std::string loweredToken = ToLower(token);
        return loweredValue.find(loweredToken) != std::string::npos;
    }
}
