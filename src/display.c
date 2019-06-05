#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "base.h"

static SDL_Window      *pWindow = NULL;
static SDL_Renderer    *pRenderer = NULL;
static SDL_Texture     *pTexture;
static SDL_Rect fullRect;

int width,height, vids_width, vids_height;

void *thread_display(void *param);

typedef struct _rfb_vid
{
	int id;
	int fd;
	SDL_Rect rect;
}rfb_vid;

rfb_vid *vids = NULL;
void create_display(int window_size)
{
	int i, j, id, ret;
	pthread_t pthread_display;
#if 0
	/* 初始化SDL */
	if(SDL_Init(SDL_INIT_VIDEO) < 0) 
	{
		DIE("SDL_Init err");
	}   	
	/* 创建窗口 */
	pWindow = SDL_CreateWindow("vnc", 0, 0, 0 , 0, SDL_WINDOW_FULLSCREEN_DESKTOP);  //设置全屏
	if (NULL == pWindow)   
	{
		DIE("SDL_CreateWindow is NULL");
	}
	/* 创建Renderer 画板 */
	pRenderer = SDL_CreateRenderer(pWindow, -1, 0);	
	if (NULL == pRenderer)
	{ 
       DIE("pTexture is NULL");
	}
	
	pTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, vids_width, vids_height);
	if (NULL == pTexture)
    {   
        DIE("pTexture is NULL");
    }  
	
	SDL_GetDisplayBounds(0, &fullRect);	
	

	width = fullRect.w;
	height = fullRect.h;

#endif
	width = 1280;
	height = 720;
	
	vids_width = width / window_size;
	vids_height = height / window_size;
	
	/* 隐藏窗口 */
	//SDL_HideWindow(pWindow);
	//SDL_ShowWindow();
	
	vids = (rfb_vid *)malloc(sizeof(rfb_vid) * window_size * window_size);
	
	for(i = 0; i < window_size; i++)
	{
		for(j = 0; j < window_size; j++)
		{
			id = i + j * window_size;
			DEBUG("id %d", id);
			vids[id].id = id;
			vids[id].fd = i + j;
			vids[id].rect.x = i * vids_width;
			vids[id].rect.y = j * vids_height;
			vids[id].rect.w = vids_width;
			vids[id].rect.h = vids_height;
			//DEBUG("vids[%d].rect.x = %d", id ,vids[id].rect.x);
			//DEBUG("vids[%d].rect.y = %d", id ,vids[id].rect.y);
			//DEBUG("vids[%d].rect.w = %d", id ,vids[id].rect.w);
			//DEBUG("vids[%d].rect.h = %d", id ,vids[id].rect.h);
			//create_udp(h264_port + i +  j * id);
			//DEBUG("port %d",h264_port + id + 1);
			
			ret = pthread_create(&pthread_display, NULL, thread_display, &(vids[id]));
			if(0 != ret)
			{
				//DIE("ThreadDisp err %d,  %s",ret , strerror(ret));
			}
		}
	}
}

void *thread_display(void *param)
{
	rfb_vid vid = *(rfb_vid *)param;
	DEBUG("thread_display id %d", vid.id);
	DEBUG("thread_display rect.x %d", vid.rect.x);
	DEBUG("thread_display rect.y %d", vid.rect.y);
	DEBUG("thread_display rect.w %d", vid.rect.w);
	DEBUG("thread_display rect.h %d", vid.rect.h);


	uint8_t *cur_ptr;
    int cur_size;

    int first_time = 1;
	char filename[16] = {0};

	char *buf = NULL;
    int buf_len = 0;


    AVCodec *pCodec;
    AVCodecContext *pCodecCtx = NULL;
    AVCodecParserContext *pCodecParserCtx = NULL;

    int frame_count;
    FILE *fp_in;
    FILE *fp_out;
    AVFrame *pFrame, *pFrameYUV;
    uint8_t *out_buffer;
    const int in_buffer_size = 800000;
    uint8_t in_buffer[in_buffer_size];
    memset(in_buffer, 0, sizeof(in_buffer));

    AVPacket packet;
    int ret, got_picture;

    struct SwsContext *img_convert_ctx;
	
	pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!pCodec) {
        printf("Codec not found\n");
        return -1;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx){
        printf("Could not allocate video codec context\n");
        return -1;
    }

    pCodecParserCtx = av_parser_init(AV_CODEC_ID_H264);
    if (!pCodecParserCtx){
        printf("Could not allocate video parser context\n");
        return -1;
    }

    if (pCodec->capabilities&AV_CODEC_CAP_TRUNCATED)
        pCodecCtx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec\n");
        return -1;
    }

	sprintf(filename, "%d.h264", vid.id);
	DEBUG("filename %s", filename);
#if 1
    fp_in = fopen(filename, "rb");
    if (!fp_in) {
        printf("Could not open input stream\n");
        return -1;
    }
#endif

    pFrame = av_frame_alloc();
    av_init_packet(&packet);
	
	//QUEUE_INDEX *index;
    while(1)
    {
#if 0
        if(empty_queue(&vids_queue))
        {
            usleep(20000);
            continue;
        }
        index = de_queue(&vids_queue);

        cur_ptr = index->pBuf;
        cur_size = index->uiSize;
        de_queuePos(&vids_queue);
#else
        cur_size = fread(in_buffer, 1, in_buffer_size, fp_in);
        cur_ptr = in_buffer;

#endif
        if(cur_size == 0)
            sleep(3);

		        while (cur_size>0)
        {

            int len = av_parser_parse2(
                pCodecParserCtx, pCodecCtx,
                &packet.data, &packet.size,
                cur_ptr, cur_size,
                AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

            cur_ptr += len;
            cur_size -= len;

            if (packet.size == 0)
                continue;

            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, &packet);
            if (ret < 0) {
                printf("Decode Error.(解码错误)\n");
                //return ret;
                continue;
            }
            DEBUG("vids_width %d, vids_height %d", vids_width, vids_height);
            if (got_picture) {
                if (first_time){
                    //SwsContext
                    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                            vids_width, vids_height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
                    pFrameYUV = av_frame_alloc();
                    //out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
                    out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, vids_width, vids_height));
                    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, vids_width, vids_height);

                    first_time = 0;
                }

            sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,pFrameYUV->data, pFrameYUV->linesize);

            //pthread_mutex_lock(&renderer_mutex);
            SDL_UpdateYUVTexture(pTexture, NULL,
                pFrameYUV->data[0], pFrameYUV->linesize[0],
                pFrameYUV->data[1], pFrameYUV->linesize[1],
                pFrameYUV->data[2], pFrameYUV->linesize[2]);

            //SDL_RenderClear(pRenderer);

            SDL_RenderCopy(pRenderer, pTexture, NULL, &(vid.rect));
            SDL_RenderPresent(pRenderer);
            //pthread_mutex_unlock(&renderer_mutex);
            //SDL_Delay(40);
            }
        }
	    packet.data = NULL;
        packet.size = 0;

    }
    sws_freeContext(img_convert_ctx);
    av_parser_close(pCodecParserCtx);

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);

    return 0;
}


	






	//create_ffmpeg();




void close_display()
{
	SDL_DestroyWindow(pWindow);
    SDL_DestroyRenderer(pRenderer);
    SDL_DestroyTexture(pTexture);
    SDL_Quit();
}

