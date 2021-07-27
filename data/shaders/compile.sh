shopt -s nullglob
for each in *.{vert,frag,comp}; do
  glslc --target-env=vulkan1.1 "$each" -o "$each".spv
done
shopt -u nullglob
