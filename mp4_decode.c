//
// Created by zyc on 2022/3/12.
//

//#include <iostream>
//1、目录要对应上
//2、头文件和lib   版本要对  ffmpeg3.42  ffmpeg4.2
//3、c++调用C语言的时候  需要一个extern

//extern "C"
//{
//
//}


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>




int main()
{
    //const char* url = "http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8";
    const char *url = "/Users/zyc1/Desktop/商务ppt（点击进入）/mk217 - PyTorch入门到进阶 实战计算机视觉与自然语言处理项目/第1章 课程介绍-选择Pytorch的理由/1-1 课程导学~1.mp4";
    int ret = -1;

    //测试和查看操作系统使用情况和支持库
    printf("%s\n", avcodec_configuration());

    av_register_all();//注册所有的库

    //建立空间
    AVFormatContext * pFormat = avformat_alloc_context();

    avformat_network_init();//网络流初始化
    //打开网络流
    //////////////////////////////
    ////////////网络代码在这里进行
    //网络传输的时候  雷神  librtmp  不稳定的  随时断开

    //暂时这么写   多线程解码
    AVDictionary* opt = NULL; //=》专用头文件 opt.h
    av_dict_set(&opt, "rtsp_transport", "tcp", 0);
    av_dict_set(&opt, "max_delay", "550", 0);

    //input =>output(输出的流处理)   打开输入文件  打开外部文件 把流输入进来
    ret = avformat_open_input(&pFormat, url,0, &opt);
    if (ret!=0)
    {
        printf("avformat_open_input error");
        system("pause");
        return -1;
    }
    //寻找流文件   新版本和老版本的方法
    //老的用的是遍历   新的是直接用新接口来做
    printf("avformat_open_input success\n");
    ret  = avformat_find_stream_info(pFormat, 0);
    if (ret < 0)
    {
        printf("avformat_find_stream_info error");
        system("pause");
        return -1;
    }
    printf("avformat_find_stream_info success");

    //总时长
    int time = pFormat->duration;
    int mbtimer = (time / 1000000) / 60;
    int mintimer = (time / 1000000) % 60;

    printf("%d分:%d秒\n", mbtimer, mintimer);

    av_dump_format(pFormat, 0, url, 0);

    ////AVStream  => int  寻找流
    int videoStream = av_find_best_stream(pFormat, AVMEDIA_TYPE_VIDEO, -1, -1, 0,0);
    int AudioStream = av_find_best_stream(pFormat, AVMEDIA_TYPE_AUDIO, -1, -1, 0,0);


    ////寻找解码器
    AVCodecContext *avctx = pFormat->streams[videoStream]->codec;
    AVCodec * avcodec = avcodec_find_decoder(avctx->codec_id);

    //const AVCodec *codec, AVDictionary **options
    //打开解码器
    ret  = avcodec_open2(avctx, avcodec, 0);
    if (ret !=0)
    {
        printf("avcodec_open2 error");
        system("pause");
        return -1;
    }
    printf("avcodec_open2 success");


    AVFrame* Frame	  = av_frame_alloc();
    AVFrame* Frameyuv = av_frame_alloc();
    int width = avctx->width;
    int heigt = avctx->height;
    int fmt = avctx->pix_fmt;//AVPixelFormat
    printf("宽:%d,高:%d\n", width, heigt);
    //分配空间  进行图像转换

    int nSize = avpicture_get_size(AV_PIX_FMT_YUV420P, width, heigt);

    //数据具体格式转换
    uint8_t* buff =(uint8_t*) av_malloc(nSize);

    //一帧图像大小
    avpicture_fill((AVPicture *)Frameyuv,
                   buff,
                   AV_PIX_FMT_YUV420P, width, heigt);

    //AVPacket  包
    AVPacket*packet = (AVPacket*)av_malloc(sizeof(AVPacket));

    //转换上下文  多路  单路(新版本)  其它已经C++格式  ffmpeg把以前的抛弃掉了
    // 实际上就是一个矩阵到另外一个矩阵过程
    struct SwsContext* sct = NULL;
    sct =  sws_getContext (width, heigt,
                           (enum AVPixelFormat)fmt,
                           width, heigt, AV_PIX_FMT_YUV420P,
                           SWS_BICUBIC,0,0,0);

    //读帧
    int FrameCount = 0;
    int go;
    while (av_read_frame(pFormat,packet)>=0)
    {
        //判断是否是视频流
        if (packet->stream_index == AVMEDIA_TYPE_VIDEO)
        {
            //解码视频流
            ret = avcodec_decode_video2(avctx,Frame,&go,packet);

            if (ret < 0)
            {
                printf("avcodec_decode_video2");
                return -1;
            }
            //解码成功就转换数据
            if (go)
            {
                int h = sws_scale(sct, (const uint8_t**)Frame->data,
                                  Frame->linesize,
                                  0,
                                  heigt,
                                  Frameyuv->data,
                                  Frameyuv->linesize);

                ///////////////////////文件写入
                ////////////////////文件编码
                //fwrite  查看码流

                FrameCount++;
                printf("当前高度为:%d,当前转换第%d帧\n",h, FrameCount);

            }
        }
        av_free_packet(packet);
    }
    //free context();

    sws_freeContext(sct);
    av_frame_free(&Frame);
    av_frame_free(&Frameyuv);
    avformat_close_input(&pFormat);
    avformat_free_context(pFormat);

    return 0;
}
