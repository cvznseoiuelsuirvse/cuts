subdir('seat')

libdrm_dep = dependency('libdrm')
libinput_dep = dependency('libinput')
libudev_dep = dependency('libudev')
libxkb_dep = dependency('xkbcommon')

backend_sources = files(
  'drm/drm.c',
  'drm/cursor.c',
  'input.c',
  'backend.c',
) + seat_sources

backend_dep = declare_dependency(
  sources: backend_sources,
  dependencies: [
    libxkb_dep,
    libdrm_dep,
    libinput_dep, 
    libudev_dep
  ],
)
