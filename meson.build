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

# Create reader executable
executable(
      prjName
    , src
    , include_directories : inc
    , link_args : '-lwiringPi'
)

graph_inc = include_directories([
    'src'
])

cc = meson.get_compiler('c')
# m_dep = cc.find_library('m', required : false)

graph_deps = [
      dependency('lvgl')
    , cc.find_library('m', required : false)
    , cc.find_library('liblvgl_thorvg')
]

# Create graph_fb executable
executable(
      'graph_fb'
    , 'src/graph_fb.c'
    , include_directories: graph_inc
    , dependencies: graph_deps
)
