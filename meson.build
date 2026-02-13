project(
    'reader', 'c',
    version : '0.1.0',
    meson_version : '>= 0.56.0')

prjName = meson.project_name()
confData = configuration_data()
confData.set('PROJECT',    prjName)
confData.set('VERSION',    meson.project_version())

configure_file(
      input  : 'config.hh'
    , output : 'config.h'
    , configuration : confData
)

# Includes
inc = include_directories([
      'src'
    , 'lib'
])

# Sources
src = [
      'lib/pn532.c'
    , 'lib/pn532_rpi.c'
    , 'src/main.c'
]

# Create executable
executable(
      prjName
    , src
    , include_directories : inc
    , link_args : '-lwiringPi'
)
