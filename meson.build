project('speech-provider-picotts', 'c',
        version: '0.1.0',
        license: 'LGPL-2.1-or-later',
	meson_version : '>= 0.53')

version = meson.project_version()

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
localedir = prefix / get_option('localedir')
datadir = prefix / get_option('datadir')
pkgdatadir = datadir / meson.project_name()

application_id = 'org.picotts.Speech.Provider'

subdir('src')

config = configuration_data()
config.set('bindir', bindir)

# DBus service
configure_file(
  input: 'data/org.picotts.Speech.Provider.service.in',
  output: 'org.picotts.Speech.Provider.service',
  configuration: config,
)
install_data(
  meson.current_build_dir() / 'org.picotts.Speech.Provider.service',
  install_dir: get_option('datadir') / 'dbus-1/services',
)

install_data(
  'data' / 'org.picotts.Speech.Provider.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo',
)
