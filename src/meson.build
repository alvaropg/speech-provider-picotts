deps  = []
deps += dependency('speech-provider-1.0',
                   version: '>= 1.0.1',
                   required: true)

deps += dependency('dbus-glib-1',
                   required: true)
deps += dependency('glib-2.0',
                   required: true)
deps += dependency('gio-2.0',
                   required: true)
deps += dependency('gio-unix-2.0',
                   required: true)

executable (meson.project_name(),
            ['main.c', 'picotts-speech-provider.c', 'picotts-speech-provider.h'],
            dependencies: deps,
            link_args: '-lttspico',
            install: true,
           )
