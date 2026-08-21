cur_sources = files(
  'cursor.c',
  'xcursor.c',
  'hardware.c',
)

cur_dep = declare_dependency(
  sources: cur_sources,
  dependencies: [
    libdrm_dep,
  ],
)
