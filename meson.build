project('cuts', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
    'optimization=0',
    'debug=true',
    'b_sanitize=address',
  ],
)

subdir('util')
subdir('backend')
subdir('wayland')
subdir('render')

c_args = [
  '-Wno-unused-parameter',
  '-DCUTS_LOGS',
  '-DCUTS_WL_LOGS'
]

sources = [
  'cuts.c',
  util_sources,
  backend_sources,
  server_sources,
  render_sources,
]
executable('main',
  sources,
  c_args: c_args,
  dependencies: [
    libdrm_dep, 
    egl_dep, 
    gbm_dep, 
    gles_dep, 
    libinput_dep, 
    libseat_dep,
    libudev_dep,
  ],
  install: true,
)
