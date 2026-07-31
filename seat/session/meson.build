diaz_daemon_sources = files(
 '../../wayland/types.c',
 'impl/diazepam/daemon.c',
 'impl/diazepam/vt.c',
 'impl/diazepam/sock.c',
)

session_sources = files(
  'seat.c', 
  'session.c',
  'impl/libseat.c', 
  'impl/diazepam/seat.c',
  'impl/diazepam/sock.c',
)

executable('diazepam', diaz_daemon_sources,
  dependencies: [
    util_dep,
  ],
  c_args: c_args,
  install: true,
)
