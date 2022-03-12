//
// Created by zyc on 2022/3/12.
//

#include <iostream>
#include <Windows.h>

#include <libavformat\avformat.h>
#include <libavcodec\avcodec.h>
#include <libavutil\avutil.h>
#include <libswscale\swscale.h>
#include <libswresample\swresample.h>

#include <libavutil\opt.h>


#pragma  comment(lib,"avcodec.lib")
#pragma  comment(lib,"avformat.lib")
#pragma  comment(lib,"avutil.lib")
#pragma  comment(lib,"swresample.lib")
#pragma  comment(lib,"swscale.lib")

double Rtod(AVRational r);

int main()
{
    printf("%s\n",avcodec_configuration());

    //文件路径
    const char* path = "1.mp4";

    //电视直播流地址
    //const char* path = "http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8";
    //rtmp://58.200.131.2:1935/livetv/cctv1
    //http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8
    //http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/gear2/prog_index.m3u8
    //注册全部函数
    av_register_all();
    avcodec_register_all();
    //开启网络流功能
    avformat_network_init();



    //创建解封装上下文
    //AVFormatContext* ic  = avformat_alloc_context();;
    AVFormatContext* ic = NULL;

    //设置网络的参数 rtsp rtmp  http

    AVDictionary *opt = NULL;
    av_dict_set(&opt, "rtsp_transport", "ucp", 0);
    av_dict_set(&opt, "max_delay", "550", 0);
    int ret = avformat_open_input(&ic, path, NULL,&opt);

    if (ret!= 0)
    {
        printf("avformat_open_input error!\n");
        return -1;
    }
    printf("avformat_open_input success!\n");

    ret = avformat_find_stream_info(ic, NULL);

    if (ret<0)
    {

        printf("avformat_find_stream_info error!\n");
        return -2;
    }
    printf("avformat_find_stream_info  success!\n");

    int total_min = ((ic->duration) / 1000000) / 60;
    int total_s  = ((ic->duration) / 1000000) % 60;

    printf("%d分:%d秒\n", total_min, total_s);

    //新函数
    int VideoStream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, NULL);
    int AudioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, NULL);

    printf("宽:%d 高:%d\n",
           ic->streams[VideoStream]->codecpar->width,
           ic->streams[VideoStream]->codecpar->height);

    printf("采样率:%d 通道数:%d\n",
           ic->streams[AudioStream]->codecpar->sample_rate,
           ic->streams[AudioStream]->codecpar->channels);

    //*********************************************************************
    //寻找视频解码器

    AVCodec * avcode = avcodec_find_decoder(ic->streams[VideoStream]->codecpar->codec_id);
    if (!avcode)
    {

        printf("avcodec_find_decoder video  error!\n");
        return -3;
    }
    printf("avcodec_find_decoder  video success!\n");
    //创建视频解码器

    AVCodecContext* vc = avcodec_alloc_context3(avcode);

    //配置视频解码器设备上下文
    avcodec_parameters_to_context(vc, ic->streams[VideoStream]->codecpar);

    //设置多线程解码数量
    vc->thread_count = 16;

    //打开视频解码器
    ret = avcodec_open2(vc, NULL, NULL);
    if (ret!=0)
    {
        printf("avcodec_open2 video error!\n");
        return -4;
    }
    printf("avcodec_open2 video  success!\n");

    av_dump_format(ic, NULL, path, 0);

    /********************************************************************/
    //*********************************************************************
    //寻找音频解码器

    AVCodec * acodec = avcodec_find_decoder(ic->streams[AudioStream]->codecpar->codec_id);
    if (!acodec)
    {

        printf("avcodec_find_decoder Audio  error!\n");
        return -3;
    }
    printf("avcodec_find_decoder  Audio success!\n");
    //创建音频解码器

    AVCodecContext* ac = avcodec_alloc_context3(acodec);

    //配置音频解码器设备上下文
    avcodec_parameters_to_context(ac, ic->streams[AudioStream]->codecpar);

    //设置多线程解码数量
    vc->thread_count = 16;

    //打开音频解码器
    ret = avcodec_open2(ac, NULL, NULL);
    if (ret != 0)
    {
        printf("avcodec_open2 Audio error!\n");
        return -4;
    }
    printf("avcodec_open2 Audio  success!\n");

    //***************创建视频信息 ************************
    AVPacket* packet = av_packet_alloc();
    AVFrame*  frame  = av_frame_alloc();

    //像素转换
    SwsContext* vCtx = NULL;
    unsigned char* rgb = NULL;

    //***********************音频重新采样***********************

    SwrContext* aCtx = NULL;
    unsigned char* pcm= NULL;
    aCtx = swr_alloc_set_opts(aCtx,
                              av_get_default_channel_layout(2),
                              AV_SAMPLE_FMT_S16,
                              ac->sample_rate,
                              av_get_default_channel_layout(ac->channels),
                              ac->sample_fmt,
                              ac->sample_rate,
                              NULL,NULL);
    ret = swr_init(aCtx);

    if (ret)
    {
        printf("audio init failed");
        return -1;
    }
    printf("audio init success!");

    //*******************读取帧率**************************

    while (true)
    {
        int re = av_read_frame(ic, packet);

        if (re)
        {
            continue;
        }
        //获取数据包信息:
        printf("packet:%d\n",packet->size);


        //pts
        int ms = packet->pts* Rtod(ic->streams[packet->stream_index]->time_base) * 1000;
        printf("packet pts:%d\n", ms);

        //当前解码时间
        printf("packet pts:%d\n", packet->dts);

        //创建一个同用的解码器
        AVCodecContext* VAcc = NULL;

        if (packet->stream_index == VideoStream)
        {
            printf("视频");
            VAcc = vc;
        }
        if (packet->stream_index == AudioStream)
        {
            printf("视频");
            VAcc = ac;
        }

        //发送帧
        re = avcodec_send_packet(VAcc,packet);
        //删除packet引用计数
        av_packet_unref(packet);

        if (re)
        {
            printf("发送失败");
            continue;
        }

        while (true)
        {
            re = avcodec_receive_frame(VAcc,frame);
            if (re)
            {
                break;
            }
            printf("frame linesize:%d\n",frame->linesize[0]);

            //判断如果为视频
            if (VAcc == vc)
            {
                //printf("当前是视频转换！！！");

                vCtx = sws_getContext(
                        frame->width,
                        frame->height,
                        (AVPixelFormat)frame->format,
                        frame->width,
                        frame->height,
                        AV_PIX_FMT_RGBA,//4个通道
                        //去马赛克的
                        SWS_BILINEAR, 0, 0, 0);

                if (vCtx)
                {
                    printf("当前视频转换成功！！！");
                    uint8_t* data[2] = {0};
                    if (!rgb)
                        rgb = new unsigned char[frame->width*frame->height * 4];

                    data[0] = rgb;
                    int line[2] = {0};
                    line[0] = frame->width * 4;

                    //line

                    int h = sws_scale(vCtx,
                                      frame->data,
                                      frame->linesize,
                                      0,
                                      frame->height,
                                      ( uint8_t**)data,
                                      line);
                    printf("fps height:%d\n",h);
                }

            }
            else//音频
            {
                uint8_t* data[2] = { 0 };

                if (!pcm)
                    pcm = new unsigned char[frame->nb_samples * 2 * 2];
                data[0] = pcm;
                re = swr_convert(aCtx,
                                 data,
                                 frame->nb_samples,
                                 (const uint8_t**)frame->data,
                                 frame->nb_samples
                );
                printf("conver:%d\n", re);

                //printf("当前是音频转换！！！");
            }

        }
        Sleep(1000);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    if (ic)
    {
        avformat_close_input(&ic);
    }
    if (rgb)
    {
        delete rgb, rgb = NULL;
    }
    if (pcm)
    {
        delete pcm, pcm = NULL;
    }
    system("pause");
    //swr_free()
    return 0;
}

double Rtod(AVRational r)
{
    if (r.den == 0)
        return 0;
    else
        return (double)r.num / (double)r.den;
}//
// Created by zyc on 2022/3/12.
//

