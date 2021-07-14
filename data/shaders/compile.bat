@echo off
for /r %%f in (*.vert, *.frag, *.comp) do (
    glslc --target-env=vulkan1.1 %%f -o %%f.spv
)