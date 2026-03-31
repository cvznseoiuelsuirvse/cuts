daemon_sources = files(
 '../../wayland/types.c',
 'diazepam/daemon.c',
 'diazepam/vt.c',
 'diazepam/sock.c',
)

seat_sources = files(
  'seat.c', 
  'libseat.c', 
  'diazepam/seat.c',
  'diazepam/sock.c',
)

executable('diazepam', daemon_sources,
  dependencies: [
    util_dep,
  ],
  c_args: c_args,
  install: true,
)
