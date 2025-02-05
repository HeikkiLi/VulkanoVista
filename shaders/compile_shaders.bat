glslangValidator -V shader.frag -o fragment_shader.spv
glslangValidator -V shader.vert -o vertex_shader.spv
glslangValidator -V second_pass.vert -o second_pass_vert.spv
glslangValidator -V second_pass.frag -o second_pass_frag.spv
pause