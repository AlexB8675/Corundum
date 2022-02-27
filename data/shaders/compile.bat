@echo off
for /r %%f in (*.vert, *.frag, *.geom, *.comp) do (
    glslc -g --target-env=vulkan1.2 %%f -o %%f.spv
)