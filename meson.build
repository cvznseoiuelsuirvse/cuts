project('cuts', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
    'optimization=0',
    'debug=true',
  ],
)

libdrm_dep = dependency('libdrm')

subdir('util')
subdir('backend')
subdir('wayland')

root_sources = files('cuts.c')

c_args = [
  '-Wno-unused-parameter',
]

executable('main',
  root_sources + util_sources + backend_sources + server_sources,
  c_args: c_args,
  dependencies: libdrm_dep,
  install: true,
)
