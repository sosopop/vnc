#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>

#include "base.h"
#include "queue.h"

const char program_name[] = "ffplay";

static int default_width  = 1280;
static int default_height = 720;
static int screen_width  = 0;
static int screen_height = 0;
static int audio_disable = 1;
static int video_disable = 0;
static int display_disable = 0;
static int borderless = 0;
static int exit_on_keydown;
static int exit_on_mousedown;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_RendererInfo renderer_info = {0};
static SDL_AudioDeviceID audio_dev;
static SDL_Texture *texture = NULL;

int width,height, vids_width, vids_height;
rfb_display *displays = NULL;
pthread_t *pthread_decodes = NULL;
pthread_mutex_t renderer_mutex;

int control_mode = 0;

static void do_exit();


void *thread_display(void *param)
{
	int ret;
    pthread_attr_t st_attr;
    struct sched_param sched;

    ret = pthread_attr_init(&st_attr);
    if(ret)
    {   
       	DEBUG("ThreadMain attr init error ");
    }   
    ret = pthread_attr_setschedpolicy(&st_attr, SCHED_FIFO);
    if(ret)
    {   
        DEBUG("ThreadMain set SCHED_FIFO error");
    }   
    sched.sched_priority = SCHED_PRIORITY_UDP;
    ret = pthread_attr_setschedparam(&st_attr, &sched);

	create_display();
}


void create_display()
{
    int flags, ret;
    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if(audio_disable)
        flags &= ~SDL_INIT_AUDIO;
    else
    {
        /* Try to work around an occasional ALSA buffer underflow issue when the
         * period size is NPOT due to ALSA resampling by forcing the buffer size. */
        if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
            SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE","1", 1);
    }

    if(display_disable)
        flags &= ~SDL_INIT_VIDEO;

    if(SDL_Init(flags))
    {
        DIE("Could not initialize SDL - %s", SDL_GetError());
    }
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    if(!display_disable)
    {
        DEBUG("create window ");
        int flags = SDL_WINDOW_HIDDEN;  //SDL_WINDOW_SHOWN SDL_WINDOW_HIDDEN
        if(borderless)              //无边框
            flags |= SDL_WINDOW_BORDERLESS;
        else
            flags |= SDL_WINDOW_RESIZABLE;

        window = SDL_CreateWindow(program_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, default_width, default_height, 0);
		if(!window_flag)
		{
			SDL_Rect fullRect;
			SDL_GetDisplayBounds(0, &fullRect);
			DEBUG("full_width %d full_height %d", fullRect.w, fullRect.h);
	    	screen_width = fullRect.w;
	    	screen_height = fullRect.h;
			SDL_SetWindowSize(window, screen_width, screen_height);
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
		}
		else
		{
	    	screen_width = default_width;
	    	screen_height = default_height;
			//SDL_SetWindowPosition(window, fullRect.x, fullRect.y);
		}
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        if(window)
        {
     //       renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if(!renderer)
            {
                DEBUG("Failed to initialize a hardware accelerated renderer: %s", SDL_GetError());
                renderer = SDL_CreateRenderer(window, -1, 0);
            }
            if(renderer)
            {
                if (!SDL_GetRendererInfo(renderer, &renderer_info))
                    DEBUG("Initialized %s renderer.", renderer_info.name);
            }
        }
        if(!window || !renderer || !renderer_info.num_texture_formats)
        {
            DIE("Failed to create window or renderer: %s", SDL_GetError());
        }
    }
	partition_display();
    atexit(do_exit);
    event_loop();
    }

static void do_exit()
{
    if(renderer)
        SDL_DestroyRenderer(renderer);
    if(window)
        SDL_DestroyWindow(window);
    SDL_Quit();
	DIE("programe end");
}

#if 0
void event_loop()
{
    int done = 0;
    SDL_Event event;
    DEBUG("init ok loop start");
    while(!done)
    {
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_QUIT)
				do_exit();
            break;
        }
    }
}
#endif

void update_texture(AVFrame *frame_yuv, SDL_Rect rect)
{
	if(!texture || !renderer)
		return;
    pthread_mutex_lock(&renderer_mutex);
    SDL_UpdateYUVTexture(texture, NULL,
        frame_yuv->data[0], frame_yuv->linesize[0],
        frame_yuv->data[1], frame_yuv->linesize[1],
        frame_yuv->data[2], frame_yuv->linesize[2]);

    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_RenderPresent(renderer);
    pthread_mutex_unlock(&renderer_mutex);
}


void partition_display()
{
	int i , j, ret, id;

    vids_width = screen_width / window_size;
    vids_height = screen_height / window_size;

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, vids_width, vids_height);
	if(!texture)
	{
		DIE("create texture err");
	}

	pthread_mutex_init(&renderer_mutex, NULL);

    displays = (rfb_display *)malloc(sizeof(rfb_display) * window_size * window_size);
    memset(displays, 0, sizeof(rfb_display) * window_size * window_size);
    pthread_decodes = (pthread_t *)malloc(sizeof(pthread_t) * window_size * window_size);

    for(i = 0; i < window_size; i++)
    {   
        for(j = 0; j < window_size; j++)
        {   
            id = i + j * window_size;
            displays[id].id = id;
            displays[id].fd = -1;
            displays[id].rect.x = i * vids_width;
            displays[id].rect.y = j * vids_height;
            displays[id].rect.w = vids_width;
            displays[id].rect.h = vids_height;

            /* 创建对应窗口的显示线程 */
            ret = pthread_create(&(pthread_decodes[id]), NULL, thread_decode, &(displays[id]));
            if(0 != ret)
            {   
                DIE("ThreadDisp err %d,  %s",ret , strerror(ret));
            }   
        }   
    }   
}


static void refresh_loop_wait_event(SDL_Event *event)
{
	double remaining_time = 0.0;
	QUEUE_INDEX *index = NULL;
    SDL_PumpEvents();
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = default_width;
	rect.h = default_height;
	while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT))
	{
		SDL_PumpEvents();
	}
}


void event_loop()
{
	SDL_Event event;
	DEBUG("event_loop");
#if 0
	for(;;)
	{
		refresh_loop_wait_event(&event);
		switch(event.type)
		{
			case SDL_KEYDOWN:   //键盘事件
				if(event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q)
				{
					do_exit();
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:  //鼠标按下
				switch(event.button.button)
				{
					case SDL_BUTTON_LEFT:
						if(control_mode)
						{
							//point.mask | = (1<<2);
						}
						else
						{
							#if 0
							x = event.button.x;
							y = event.button.x;
							id = check_area(x,y);  //判断在哪个区域
							if(id == id)			//是否连点2次
							{
								contorl_mode = 1;
								//pthread_wait();
								//tranfrom_mode();	
							}	
							#endif
						}
						break;
					case SDL_BUTTON_RIGHT:
						break;
				}
				break;

			case SDL_MOUSEBUTTONUP:  //鼠标抬起
				break;
				
			case SDL_QUIT:			//关闭窗口
				do_exit();
				break;
			default:
				break;
		}
	}	
#endif
	int done = 0;
    DEBUG("init ok loop start");
    while(!done)
    {
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_QUIT)
                done = 1;
            break;
        }
    }
    SDL_Quit();

}

