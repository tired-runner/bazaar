bz_sources = [
  'bz-application.c',
  'bz-async-texture.c',
  'bz-backend.c',
  'bz-background.c',
  'bz-browse-widget.c',
  'bz-content-provider.c',
  'bz-content-section.c',
  'bz-entry-group.c',
  'bz-entry.c',
  'bz-error.c',
  'bz-flatpak-entry.c',
  'bz-flatpak-instance.c',
  'bz-paintable-model.c',
  'bz-preferences-dialog.c',
  'bz-progress-bar.c',
  'bz-review.c',
  'bz-search-widget.c',
  'bz-section-view.c',
  'bz-share-dialog.c',
  'bz-transaction-manager.c',
  'bz-transaction-view.c',
  'bz-transaction.c',
  'bz-update-dialog.c',
  'bz-window.c',
  'bz-yaml-parser.c',
  'main.c',
]

bz_deps = [
  cc.find_library('m', required: false),
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.7'),
  dependency('libdex-1', version: '>= 0.9'),
  dependency('flatpak', version: '>= 1.9'),
  dependency('appstream', version : '>= 1.0'),
  dependency('xmlb', version : '>= 0.3.4'),
  dependency('glycin-1', version : '>= 1.0'),
  dependency('glycin-gtk4-1', version : '>= 1.0'),
  dependency('yaml-0.1', version : '>= 0.2.5'),
]

bz_sources += gnome.compile_resources('bz-resources',
  'bazaar.gresource.xml',
  c_name: 'bz'
)

executable('bazaar', bz_sources,
  dependencies: bz_deps,
       install: true,
)
