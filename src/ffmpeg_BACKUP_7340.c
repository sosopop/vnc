#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>

#include "base.h"
#include "queue.h"

void ffmpeg_encode(rfb_format *fmt)
{

	DEBUG("client encode start");
    int  i, videoindex, ret, quality, fps, bps, got_picture;
    int width = 0, height = 0;
    const char *decode_speed = NULL;
    uint8_t *out_buffer = NULL;

    AVFormatContext *format_ctx = NULL;
    AVCodecContext  *codec_ctx = NULL;
    AVCodec         *codec = NULL;
    AVCodecContext  *h264codec_ctx = NULL;
    AVCodec         *h264codec = NULL;
    AVFrame *frame = NULL,*frame_yuv = NULL;
    AVPacket *packet = NULL;
    struct SwsContext *img_convert_ctx = NULL;

    av_register_all();
    avformat_network_init();
    avdevice_register_all();

    format_ctx = avformat_alloc_context();

    quality = default_quality;
	bps = fmt->bps;
<<<<<<< HEAD
	//fps = fmt->fps;
	fps = 22;

=======
	fps = fmt->fps;
>>>>>>> b1a59b0fb8808c9519d1e4f9b62eeadb4672da9a
    const char *decode_speed_type[] = {"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow", "placebo"};
    if(quality >= 80 && quality < 90)
    {
        decode_speed = decode_speed_type[4];
    }
    else if( quality >= 90)
    {
        decode_speed = decode_speed_type[6];
    }
    else
    {
        decode_speed = decode_speed_type[0];
    }

    DEBUG("fps %d decode_speed %s bps %d width  %d height %d",fps, decode_speed, bps, width, height);

    AVDictionary* options = NULL;

#ifdef _WIN32
    /* 截屏 */
    av_dict_set(&options,"framerate","22",0);
    av_dict_set(&options,"draw_mouse","0",0);               //鼠标
    AVInputFormat *ifmt=av_find_input_format("gdigrab");
    if(avformat_open_input(&format_ctx,"desktop",ifmt, &options)!=0)
    {
        DEBUG("Couldn't open input stream. ");
    }
#else
<<<<<<< HEAD
	char video_size[12] = {0};
=======
	char video_size[10] = {0};
>>>>>>> b1a59b0fb8808c9519d1e4f9b62eeadb4672da9a

	sprintf(video_size, "%dx%d", screen_width, screen_height);	

    av_dict_set(&options,"framerate","22",0);
    //av_dict_set(&options,"follow_mouse","centered",0);        //鼠标
    av_dict_set(&options,"draw_mouse","0",0);               //鼠标
    av_dict_set(&options,"video_size",video_size,0);
    AVInputFormat *ifmt=av_find_input_format("x11grab");
    if(avformat_open_input(&format_ctx,":0.0+0,0",ifmt, &options)!=0)
	{
		DEBUG("Couldn't open input stream. ");
		goto run_end;
    }   
#endif

    if(avformat_find_stream_info(format_ctx,NULL)<0)
    {
        DEBUG("Couldn't find stream information.");
		goto run_end;
    }
    videoindex=-1;
    for(i=0; i<format_ctx->nb_streams; i++)
    {
        if(format_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            videoindex=i;
        }
    }
    if(videoindex==-1)
    {
        DEBUG("Couldn't find a video stream.");
		goto run_end;
    }
    /* 根据视频中的流打开选择解码器 */
    codec_ctx=format_ctx->streams[videoindex]->codec;
    codec=avcodec_find_decoder(codec_ctx->codec_id);

	codec_ctx->thread_count = 4;
	codec_ctx->thread_type = 2;

    if(codec==NULL)
    {
        DEBUG("Couldn't find a video stream.");
		goto run_end;
    }
    //打开解码器
    if(avcodec_open2(codec_ctx, codec,NULL)<0)
    {
        DEBUG("Could not open codec.");
		goto run_end;
    }



	width = codec_ctx->width;
    height = codec_ctx->height;
    frame=av_frame_alloc();
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    //frame->format = codec_ctx->pix_fmt;

    frame_yuv=av_frame_alloc();
    frame_yuv->width = width;
    frame_yuv->height = height;
    frame_yuv->format = AV_PIX_FMT_YUV420P;

    out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, width, height));
    avpicture_fill((AVPicture *)frame_yuv, out_buffer, AV_PIX_FMT_YUV420P, width, height);
    packet=(AVPacket *)av_malloc(sizeof(AVPacket));
#ifdef _WIN32
    img_convert_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, width, height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
#else

    img_convert_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt + 3, fmt->width, fmt->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
#endif

    /* 查找h264编码器 */
    h264codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if(!h264codec)
    {
        DEBUG("---------h264 codec not found----");
		goto run_end;
    }

    h264codec_ctx = avcodec_alloc_context3(h264codec);
    h264codec_ctx->codec_id = AV_CODEC_ID_H264;
    h264codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    h264codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    h264codec_ctx->width = width;
    h264codec_ctx->height = height;
    h264codec_ctx->time_base.num = 1;
    h264codec_ctx->time_base.den = fps;//帧率(既一秒钟多少张图片)
    h264codec_ctx->bit_rate = 2000000; //比特率(调节这个大小可以改变编码后视频的质量)
    h264codec_ctx->gop_size= 12;
    h264codec_ctx->qmin = 10;
    h264codec_ctx->qmax = 51;
    h264codec_ctx->max_b_frames = 0;
    if (h264codec_ctx->flags & AVFMT_GLOBALHEADER)
        h264codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    /* 设置编码质量 */
    AVDictionary *param = 0;
    av_dict_set(&param, "preset", decode_speed, 0);
    av_dict_set(&param, "tune", "zerolatency", 0);  //实现实时编码

    if (avcodec_open2(h264codec_ctx, h264codec,&param) < 0)
    {
        DEBUG("Failed to open video encoder");
		goto run_end;
    }
	while(run_flag)
    {
		if(client_display.play_flag == 0)
		{
			DEBUG("client encode end");
			break;
		}
		
        /* 读取截屏中的数据--->packet  */
        if(av_read_frame(format_ctx, packet) < 0)
        {
            DEBUG(" av_read_frame no data");
			goto run_end;
        }

        if(packet->stream_index==videoindex)
        {
            ret = avcodec_decode_video2(codec_ctx, frame, &got_picture, packet);
            if(ret < 0)
            {
                continue;
            }
            if(got_picture)
            {
                 sws_scale(img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, codec_ctx->height, frame_yuv->data, frame_yuv->linesize);
                ret = avcodec_encode_video2(h264codec_ctx, packet,frame_yuv, &got_picture);
                if(ret < 0)
                {
                     DEBUG("Failed to encode!");
                }
                h264_send_data((char *)packet->data, packet->size, frame->key_frame);
            }
        }
        av_free_packet(packet);
    }

run_end:
    if(img_convert_ctx)
        sws_freeContext(img_convert_ctx);
    if(out_buffer)
        av_free(out_buffer);
    if(frame_yuv)
        av_free(frame_yuv);
    if(frame)
        av_free(frame);
    if(codec_ctx)
        avcodec_close(codec_ctx);
    if(h264codec_ctx)
        avcodec_close(h264codec_ctx);
    if(format_ctx)
        avformat_close_input(&format_ctx);
	if(packet)
		av_free_packet(packet);
<<<<<<< HEAD

	img_convert_ctx = NULL;
	out_buffer = NULL;
	frame_yuv = NULL;
	frame = NULL;
	codec_ctx = NULL;
	h264codec_ctx = NULL;
	format_ctx = NULL;	
    codec = NULL;
    h264codec = NULL;
	packet = NULL;
=======
	DEBUG("encode end");
>>>>>>> b1a59b0fb8808c9519d1e4f9b62eeadb4672da9a
}

void ffmpeg_decode(rfb_display *vid)
{
    uint8_t *out_buffer;
    int ret, got_picture;

 	AVCodec *codec = NULL;
 	AVCodecContext *codec_ctx = NULL;
 	AVFrame *frame = NULL, *frame_yuv = NULL;
 	AVPacket  packet;
 	struct SwsContext *img_convert_ctx = NULL;

	QUEUE_INDEX *index = NULL;

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        DIE("Codec not found");
    }
    codec_ctx = avcodec_alloc_context3(codec);
	codec_ctx->thread_count = 4;
	codec_ctx->thread_type = 2;

    if (!codec_ctx)
    {
        DIE("Could not allocate video codec context");
    }
    if (codec->capabilities&AV_CODEC_CAP_TRUNCATED)
        codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

    if (avcodec_open2(codec_ctx, codec, NULL) < 0)
     {
        DIE("Could not open codec");
    }
    frame = av_frame_alloc();
    av_init_packet(&packet);
 
	while(run_flag)
    {
		if(vid->play_flag > 0)
		{
			if(empty_queue(&vids_queue[vid->id]))
	        {
				usleep(2000);
	            continue;
	        }
	        index = de_queue(&vids_queue[vid->id]);
			packet.size = index->uiSize;
			packet.data = index->pBuf;
	        de_queuePos(&vids_queue[vid->id]);
		}
		else
		{
			sleep(1);
			continue;
		}

		ret = avcodec_decode_video2(codec_ctx, frame, &got_picture, &packet);
        if (ret < 0)
        {
            DEBUG("Decode Error.(解码错误)");
            continue;
        }
        if (got_picture)
        {
			if(vid->play_flag == 2)
       			update_texture(frame, NULL);
			else
       			update_texture(frame, &(vid->rect));
        }
        packet.data = NULL;
        packet.size = 0;
    }
    if(img_convert_ctx)
        sws_freeContext(img_convert_ctx);
    if(out_buffer)
        av_free(out_buffer);
    if(frame_yuv)
        av_frame_free(&frame_yuv);
    if(frame)
        av_frame_free(&frame);
    if(codec_ctx)
        avcodec_close(codec_ctx);

	img_convert_ctx = NULL;
	out_buffer = NULL;
	frame_yuv = NULL;
	frame = NULL;
	codec_ctx = NULL;	
	
	clear_texture();
	DEBUG("decode end");

	return (void *)0;
}

void *thread_encode(void *param)
{
    int ret;
    pthread_attr_t st_attr;
    struct sched_param sched;
    
    rfb_format *fmt = (rfb_format *)param;

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
    
    ffmpeg_encode(fmt);
	return (void *)0;
}
void *thread_decode(void *param)
{
	int ret;
    pthread_attr_t st_attr;
    struct sched_param sched;

    rfb_display *vid = (rfb_display *)param;

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

    ffmpeg_decode(vid);
	return (void *)0;
}

