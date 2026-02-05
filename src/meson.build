sources = files(
  'main.c',
)

speech_provider_dep = dependency('speech-provider-1.0',
                                 version: '>= 1.0.1',
                                 required: false)

executable (meson.project_name(),
            ['src/main.c'],
            dependencies: speech_provider_dep,
            install: true,
           )
