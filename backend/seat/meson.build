seat_sources = files('seat.c', 'sock.c', 'util.c')
seat_daemon_sources = files('daemon.c', 'sock.c', 'util.c', 'vt.c')
executable('cuts-seat', seat_daemon_sources,
  dependencies: [
    util_dep
  ],
  c_args: c_args,
  install: true,
)
