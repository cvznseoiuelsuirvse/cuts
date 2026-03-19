util_sources = files(
  'helpers.c',
  'event_loop.c',
  'map.c',
  'list.c',
  'bitmap.c',
  'log.c',
)
util_include = include_directories('..')
util_dep = declare_dependency(
  include_directories: util_include,
  sources: util_sources
)
