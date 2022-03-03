@echo off
for /r %%f in (*.vert, *.frag, *.geom, *.comp, *.rgen, *.rmiss, *.rchit) do (
    glslc -g -O --target-env=vulkan1.2 %%f -o %%f.spv
)