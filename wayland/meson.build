fs = import('fs')
_python = find_program('python3')
_src_root = meson.project_source_root()

_xmls = run_command('find', _src_root / 'protocols', '-name', '*.xml', check: true).stdout().strip().split('\n')

_proto_sources = []
foreach _xml : _xmls
  _proto = fs.stem(_xml)
  _out = _src_root / 'wayland' / 'proto' / _proto
  run_command(_python,
    _src_root / 'scripts' / 'wayland-codegen.py',
    _xml,
    _out,
    check: true,
  )
  _proto_sources += files('proto' / (_proto + '.c'))
endforeach

wayland_sources = files(
  'server.c',
  'display.c',
  'util.c',
) + _proto_sources + files(
  'impl/wayland.c',
  'impl/xdg-shell.c',
  'impl/linux-dmabuf-v1.c',
  'impl/xdg-decoration-unstable-v1.c',
)

wayland_dep = declare_dependency(
  sources: wayland_sources,
)
