#version 450

// Color ouput from subpass 1
layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;

// Depth output from subpass 1
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

layout(location = 0) out vec4 color;

void main()
{
    color = subpassLoad(inputColor).rgba;
}