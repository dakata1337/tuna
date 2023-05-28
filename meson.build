project('tuna', 'c',
  default_options: [
    'warning_level=everything',
    'werror=true',
    'b_lto=true',
    'b_sanitize=address,undefined',
  ]
)

src = [
  'src/tuna.c',
  'src/config.c',
  'src/toml.c',
]

inc = include_directories('include')

executable(
  meson.project_name(),
  src,
  include_directories: [inc],
)
