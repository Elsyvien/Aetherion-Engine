#include "Aetherion/Core/UUID.h"
#include <random>

namespace Aetherion::Core
{
    std::string GenerateUUID()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dist(0, 15);
        const char* digits = "0123456789abcdef";

        std::string guid;
        guid.reserve(36);
        for (int i = 0; i < 32; ++i)
        {
            if (i == 8 || i == 12 || i == 16 || i == 20)
            {
                guid.push_back('-');
            }
            guid.push_back(digits[dist(gen)]);
        }
        return guid;
    }
}
