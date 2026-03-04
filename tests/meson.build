deps  = []

deps += dependency('dbus-glib-1', required: true)
deps += dependency('glib-2.0', required: true)
deps += dependency('gio-2.0', required: true)
deps += dependency('gio-unix-2.0', required: true)

executable ('test-send-fd',
            ['test-send-fd.c'],
            dependencies: deps,
            install: true,
           )
