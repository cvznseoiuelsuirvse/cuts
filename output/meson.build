libdrm_dep = dependency('libdrm')

output_sources = files(
  'output.c',
  'drm/drm.c',
  'drm/util.c',
  'drm/syncobj.c',
)

output_dep = declare_dependency(
  sources: output_sources,
  dependencies: [
    libdrm_dep,
  ],
)
