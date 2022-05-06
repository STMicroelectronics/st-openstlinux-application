project('weston-st-egl-cube-tex',
	'c',
	version: '0.1',
	default_options: [
		'warning_level=2',
		'c_std=gnu99',
		'b_lundef=false',
	],
	meson_version: '>= 0.47',
	license: 'MIT',
)

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')

c_local_args = ['-DHAVE_CONFIG_H']

config_h = configuration_data()

dep_wayland_client = dependency('wayland-client', version: '>= 1.16.0')
dep_wayland_cursor = dependency('wayland-cursor', version: '>= 1.16.0')
dep_wayland_egl = dependency('wayland-egl', version: '>= 1.16.0')
wl_protocol_dep = dependency('wayland-protocols', version: '>= 1.16.0')

# libdrm for drm_fourcc
dep_libdrm = dependency('libdrm', version: '>= 2.4.94')
dep_pixman = dependency('pixman-1', version: '>= 0.34.0')
dep_png = dependency('libpng', version: '>= 1.6.36')
dep_jpeg = dependency('libjpeg', version: '>= 2.0.0')
if dep_jpeg.version().version_compare('>= 2.0.0')
	config_h.set('HAVE_JPEG', '1')
#	c_local_args += [ '-DHAVE_JPEG']
endif

dep_libgbm = dependency('gbm', version: '>= 18.1.0')
if dep_libgbm.version().version_compare('>= 18.1.0')
	config_h.set('HAVE_GBM_BO_MAP', '1')
#	c_local_args += [ '-DHAVE_GBM_BO_MAP']
endif

dep_libegl = dependency('egl', version: '>= 18.1.0')
if dep_libegl.version().version_compare('>= 18.1.0')
	config_h.set('ENABLE_EGL', '1')
#	c_local_args += [ '-DENABLE_EGL']
endif

dep_libgles2 = dependency('glesv2', version: '>= 18.1.0')
dep_gst = dependency('gstreamer-1.0', version: '>= 1.14.0')
if dep_gst.version().version_compare('>= 1.14.0')
	config_h.set('HAVE_GST', '1')
#	c_local_args += [ '-DHAVE_GST']
endif

dep_gst_video = dependency('gstreamer-video-1.0', version: '>= 1.14.0')
dep_gst_app = dependency('gstreamer-app-1.0', version: '>= 1.14.0')
dep_gst_alloc = dependency('gstreamer-allocators-1.0', version: '>= 1.14.0')
dep_gst_base = dependency('gstreamer-plugins-base-1.0', version: '>= 1.14.0')
dep_glib = dependency('glib-2.0', version: '>= 2.58.0')
dep_threads = dependency('threads')
libm = cc.find_library('m', required : false)

wl_sources = [
	'shared/image-loader.c',
	'src/cube-common.c',
	'src/cube-tex.c',
	'src/cube-smooth.c',
	'src/esTransform.c',
	'src/gst-decoder.c',
	'src/cube-video.c',
	'src/simple-st-egl-tex.c'
]
configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : config_h)
configinc = include_directories('.')
sharedinc = include_directories('shared')
srcinc = include_directories('src')

wl_scanner = find_program('wayland-scanner')
use_wayland = wl_protocol_dep.found() and dep_wayland_client.found() and wl_scanner.found() and dep_libdrm.found()

if use_wayland
    protocols_datadir = wl_protocol_dep.get_pkgconfig_variable('pkgdatadir')

    protocol_defs = [
        ['/stable/xdg-shell/xdg-shell.xml',
         'xdg-shell-protocol.c', 'xdg-shell-client-protocol.h'],
        ['/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml',
         'linux-dmabuf-unstable-v1-protocol.c', 'linux-dmabuf-unstable-v1-client-protocol.h'],
    ]
    protocols_files = []

    foreach protodef: protocol_defs
        xmlfile = protocols_datadir + protodef.get(0)

        protocols_files += [custom_target(protodef.get(1),
          output : protodef.get(1),
          input : xmlfile,
          command : [wl_scanner, 'code', '@INPUT@', '@OUTPUT@'])]

        protocols_files += [custom_target(protodef.get(2),
          output : protodef.get(2),
          input : xmlfile,
          command : [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])]
    endforeach

	exe_deps = [dep_wayland_client, dep_wayland_cursor, dep_wayland_egl, dep_libdrm, dep_pixman, dep_png, dep_jpeg, dep_libgbm, dep_libegl, dep_libgles2, dep_gst, dep_gst_video, dep_gst_app, dep_gst_alloc, dep_gst_base, dep_glib, dep_threads, libm]

	executable('weston-st-egl-cube-tex',
		wl_sources + protocols_files,
		install: true,
		c_args : c_local_args,
		include_directories: [configinc, sharedinc, srcinc],
		dependencies : exe_deps,
	)

endif


