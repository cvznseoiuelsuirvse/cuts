egl_dep = dependency('egl')
gles_dep = dependency('glesv2')
gbm_dep = dependency('gbm')

render_sources = files(
  'egl.c',
  'render.c',
  'window.c',
)
     
render_dep = declare_dependency(
  sources: render_sources,
  dependencies: [
    libdrm_dep,
    egl_dep,
    gles_dep,
    gbm_dep,
  ]
)

