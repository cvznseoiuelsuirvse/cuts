shaders = [
  'shader.vert',
  'solid.frag',
  'texture.frag',
  'textureext.frag',
]

shader_sources = []

foreach shader : shaders
  shader_stem = fs.stem(shader)
  shader_suffix = fs.suffix(shader).replace('.', '')

  shader_sources += custom_target(
    shader_stem + '-' + shader_suffix + '-shader',
    input:  shader,
    output: shader_stem + '-' + shader_suffix + '.h',
    command: [
      python, 
      src_root / 'scripts' / 'shader_to_header.py', 
      '@INPUT@', 
      '@OUTPUT@'
    ], 
  )
endforeach
     
shaders_dep = declare_dependency(
  sources: shader_sources,
  include_directories: include_directories('.'),
)
