project('v4l2_webcam', 'c')

sdl_dep = dependency('sdl2')
sdl_img_dep = dependency('SDL2_image')

executable('v4l2_webcam', 'v4l_webcam.c'
	, install: true
	, dependencies: [sdl_dep, sdl_img_dep])
