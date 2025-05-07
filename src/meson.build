ga_sources = [
  'main.c',
  'ga-application.c',
  'ga-window.c',
  'ga-search-widget.c',
  'ga-background.c',
  'ga-flatpak-instance.c',
]

ga_deps = [
  cc.find_library('m', required: false),
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('libdex-1', version: '>= 0.9'),
  dependency('flatpak', version: '>= 1.9'),
]

ga_sources += gnome.compile_resources('ga-resources',
  'gnome-apps-next.gresource.xml',
  c_name: 'ga'
)

executable('gnome-apps-next', ga_sources,
  dependencies: ga_deps,
       install: true,
)
