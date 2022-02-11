@echo off
for /r %%f in (*.vert, *.frag, *.geom, *.comp) do (
    glslc -g -O --target-env=vulkan1.1 %%f -o %%f.spv
)