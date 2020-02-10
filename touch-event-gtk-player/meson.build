project('touch-event-gtk-player',
	'c',
	version: '0.1',
	default_options: [
		'warning_level=2',
		'c_std=gnu99',
		'b_lundef=false',
	],
	meson_version: '>= 0.47',
	license: 'GPL-2.0+',
)

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')

c_local_args = ['-DHAVE_CONFIG_H']

dep_gtk3 = dependency('gtk+-3.0', version: '>= 3.22.30')
dep_gst = dependency('gstreamer-1.0', version: '>= 1.14.0')
if dep_gst.version().version_compare('>= 1.14.0')
	c_local_args += [ '-DHAVE_GST']
endif

dep_gst_video = dependency('gstreamer-video-1.0', version: '>= 1.14.0')
dep_gst_wayland = dependency('gstreamer-wayland-1.0', version: '>= 1.14.0')
dep_gst_base = dependency('gstreamer-plugins-base-1.0', version: '>= 1.14.0')
dep_wayland = dependency('wayland-client', version: '>= 1.17.0')
dep_glib = dependency('glib-2.0', version: '>= 2.58.0')
dep_threads = dependency('threads')
libm = cc.find_library('m', required : false)

tegp_sources = [
	'main.c'
]
srcinc = include_directories('.')

exe_deps = [dep_gtk3, dep_gst, dep_gst_video, dep_gst_wayland, dep_gst_base, dep_wayland, dep_glib, dep_threads, libm]

executable('touch-event-gtk-player',
	tegp_sources,
	install: true,
	c_args : c_local_args,
	include_directories: [srcinc],
	dependencies : exe_deps,
)


