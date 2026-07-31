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

c_args = [
  '-Wno-unused-parameter',
  '-D_GNU_SOURCE',
]

subdir('util')
subdir('compositor')
subdir('seat')
subdir('wayland')
subdir('render')
subdir('output')

sources = [
  'cuts.c',
]

executable('cuts',
  sources,
  c_args: c_args,
  dependencies: [
    output_dep,
    seat_dep,
    compositor_dep,
    util_dep,
    wayland_dep,
    render_dep,
  ],
  install: true,
)
