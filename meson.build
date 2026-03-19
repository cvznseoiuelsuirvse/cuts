project('cuts', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
    'optimization=0',
    'debug=true',
    # 'b_sanitize=address',
  ],
)

c_args = [
  '-Wno-unused-parameter',
  '-D_GNU_SOURCE',
]

libdrm_dep = dependency('libdrm')

subdir('util')
subdir('backend')
subdir('wayland')
subdir('render')

sources = [
  'cuts.c',
]

executable('main',
  sources,
  c_args: c_args,
  dependencies: [
    backend_dep,
    util_dep,
    wayland_dep,
    render_dep,
  ],
  install: true,
)
