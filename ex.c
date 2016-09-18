/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static char *dev_name = "/dev/video0";
static int fd         = -1;
void *buffer_start    = NULL;
int length = -1;
int video_depth = 16;
enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
SDL_Surface *surface;
struct v4l2_format fmt;

static void errno_exit(const char *s)
{
        fprintf (stderr, "%s error %d, %s\n",
                 s, errno, strerror (errno));

        exit (EXIT_FAILURE);
}

static int read_frame(void)
{
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		printf("ioctl dqbuf is wrong !!!\n");
		switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				errno_exit ("VIDIOC_DQBUF");
		}
	}

	memcpy(surface->pixels, (unsigned char *)buffer_start, fmt.fmt.pix.width * fmt.fmt.pix.height * (video_depth / 8));
/*
	SDL_Rect pos = {.x = 0, .y = 0};
	SDL_RWops *buf_stream = SDL_RWFromMem(buffer_start, length);
	SDL_Surface *frame = IMG_Load_RW(buf_stream, 0);

	SDL_BlitSurface(frame, NULL, surface, &pos);
*/
	SDL_Flip(surface);

/*
	SDL_FreeSurface(frame);
	SDL_RWclose(buf_stream);
 */

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
		errno_exit ("VIDIOC_QBUF");

	return 1;
}

static void mainloop(void)
{
	unsigned long count = 100;
	while (count-- > 0)
		read_frame();
}

static void stop_capturing(void)
{
	if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
		errno_exit ("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
	struct v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
		errno_exit ("VIDIOC_QBUF ... !!!");

	if (ioctl(fd, VIDIOC_STREAMON, &type) == -1)
		errno_exit ("VIDIOC_STREAMON");
}

static void uninit_device(void)
{
	if (munmap(buffer_start, length) == -1)
		errno_exit ("munmap");
}

int init_view()
{
	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Unable to initialize SDL\n");
		return -1;
	}

	// load support for the JPG and PNG image formats
	if (!IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG)) {
		printf("IMG_Init: %s\n", IMG_GetError());
		return -1;
	}

	if ((surface = SDL_SetVideoMode(fmt.fmt.pix.width, fmt.fmt.pix.height, video_depth, SDL_HWSURFACE)) == NULL)
		return -1;

	return 0;
}

void close_view()
{
	if (surface)
		SDL_FreeSurface(surface);

	surface = (SDL_Surface *)NULL;
	if (SDL_WasInit(SDL_INIT_VIDEO)) 
		SDL_Quit();
}


static void init_mmap(void)
{
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count  = 1;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
		errno_exit ("VIDIOC_REQBUFS");

	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;

	if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
		errno_exit ("VIDIOC_QUERYBUF");

	length = buf.length;
	buffer_start = mmap(NULL /* start anywhere */,
			length,
			PROT_READ | PROT_WRITE /* required */,
			MAP_SHARED /* recommended */,
			fd, buf.m.offset);

	if (buffer_start == MAP_FAILED)
		errno_exit ("mmap");
}

static void init_device(void)
{
	struct v4l2_capability cap;

	memset(&cap, 0, sizeof(cap));
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
		errno_exit ("VIDIOC_QUERYCAP");

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "%s is no video capture device\n", dev_name);
		exit (EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf (stderr, "%s does not support streaming i/o\n", dev_name);
		exit (EXIT_FAILURE);
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = 640;
	fmt.fmt.pix.height      = 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
		errno_exit ("VIDIOC_S_FMT");

	init_mmap ();
}

static void close_device(void)
{
	if (close (fd) == -1)
		errno_exit ("close");
}

static void open_device(void)
{
	if ((fd = open(dev_name, O_RDWR)) == -1) {
		fprintf (stderr, "Cannot open '%s': %d, %s\n",
				dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}
}

int main()
{
	open_device ();
	init_device ();

	if(init_view()) {
		close_device();
		return -1;
	}		

	start_capturing ();
	mainloop ();
	stop_capturing ();
	close_view();
	uninit_device ();
	close_device ();
	return 0;
}
