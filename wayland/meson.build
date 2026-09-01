xmls = run_command('find', src_root / 'protocols', '-name', '*.xml', check: true).stdout().strip().split('\n')

proto_sources = []
foreach xml : xmls
  proto = fs.stem(xml)
  proto_gen = custom_target(proto + '-proto',
    input: xml,
    output: [proto + '.c', proto + '.h'],
    build_subdir: 'proto',
    command: [
      python,
      src_root / 'scripts' / 'wayland_codegen.py',
      '@INPUT@',
      '@OUTDIR@' / proto,
    ],
  )
  proto_sources += proto_gen
endforeach


impls = run_command('ls', 'impl', check: true).stdout().strip().split('\n')
impl_sources = files()
foreach impl : impls
  impl_sources += 'impl' / impl
endforeach


wayland_sources = files(
  'server.c',
  'display.c',
  'util.c',
) + proto_sources + impl_sources

wayland_dep = declare_dependency(
  sources: wayland_sources,
)
