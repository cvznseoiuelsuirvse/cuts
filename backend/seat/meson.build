cutseatd_sources = files(
 'cutseat/daemon.c',
 'cutseat/vt.c',
 'cutseat/sock.c',
)

seat_sources = files(
  'seat.c', 
  'libseat.c', 
  'cutseat/cutseat.c',
  'cutseat/sock.c',
)

executable('cutseat', cutseatd_sources,
  dependencies: [
    util_dep
  ],
  c_args: c_args,
  install: true,
)
