egl_dep = dependency('egl')
gles_dep = dependency('glesv2')
gbm_dep = dependency('gbm')

render_sources = files(
  'gl/egl.c',
  'gl/gles.c',
  'buffer.c',
  'render.c',
  'window.c',
)
     
render_dep = declare_dependency(
  sources: render_sources,
  dependencies: [
    egl_dep,
    gles_dep,
    gbm_dep,
  ]
)

