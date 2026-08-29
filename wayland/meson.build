fs = import('fs')
python = find_program('python3')
src_root = meson.project_source_root()

xmls = run_command('find', src_root / 'protocols', '-name', '*.xml', check: true).stdout().strip().split('\n')

run_command('rm', '-rf', src_root / 'wayland' / 'proto', check: true)
run_command('mkdir', src_root / 'wayland' / 'proto', check: true)

proto_sources = []
foreach xml : xmls
  proto = fs.stem(xml)
  out = src_root / 'wayland' / 'proto' / proto
  run_command(python,
    src_root / 'scripts' / 'wayland-codegen.py',
    xml,
    out,
    check: true,
  )
  proto_sources += files('proto' / (proto + '.c'))
endforeach

wayland_sources = files(
  'server.c',
  'display.c',
  'util.c',
) + proto_sources + files(
  'impl/wayland.c',
  'impl/xdg-shell.c',
  'impl/linux-dmabuf-v1.c',
  'impl/xdg-decoration-unstable-v1.c',
  'impl/viewporter.c',
  'impl/cursor-shape-v1.c',
  'impl/presentation-time.c',
  'impl/fractional-scale-v1.c',
  'impl/ext-data-control-v1.c',
)

wayland_dep = declare_dependency(
  sources: wayland_sources,
)
