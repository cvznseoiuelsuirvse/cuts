libdrm_dep = dependency('libdrm')

output_sources = files(
  'output.c',
  'cursor.c',
  'drm/drm.c',
  'drm/util.c',
)

output_dep = declare_dependency(
  sources: output_sources,
  dependencies: [
    libdrm_dep,
  ],
)
