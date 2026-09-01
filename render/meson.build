egl_dep = dependency('egl')
gles_dep = dependency('glesv2')
gbm_dep = dependency('gbm')

subdir('shaders')

render_sources = files(
  'gl/egl.c',
  'gl/gles.c',
  'framebuffer.c',
  'renderer.c',
)

render_dep = declare_dependency(
  sources: render_sources,
  include_directories: include_directories('.'),
  dependencies: [
    egl_dep,
    gles_dep,
    gbm_dep,
    shaders_dep,
  ]
)

