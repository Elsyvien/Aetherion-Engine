#version 450

// Shadow pass fragment shader - outputs depth only
// No color output needed for shadow mapping

void main()
{
    // Depth is written automatically by the rasterizer
    // This shader intentionally left minimal for shadow pass performance
}
