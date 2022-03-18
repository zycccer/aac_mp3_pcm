//
// Created by zyc on 2022/3/4.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavcodec/avcodec.h>

#define VIDEO_INBUF_SIZE 20480
#define VIDEO_REFILL_THRESH 4096

static char err_buf[128] = {0};
static char* av_get_err(int errnum)
{
    av_strerror(errnum, err_buf, 128);
    return err_buf;
}

static void print_video_format(const AVFrame *frame)
{
    printf("width: %u\n", frame->width);
    printf("height: %u\n", frame->height);
    printf("format: %u\n", frame->format);// 格式需要注意
}

static void decode(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *frame,
                   FILE *outfile)
{
    int ret;
    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(dec_ctx, pkt);
    if(ret == AVERROR(EAGAIN))
    {
        fprintf(stderr, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
    }
    else if (ret < 0)
    {
        fprintf(stderr, "Error submitting the packet to the decoder, err:%s, pkt_size:%d\n",
                av_get_err(ret), pkt->size);
        return;
    }

    /* read all the output frames (infile general there may be any number of them */
    while (ret >= 0)
    {
        // 对于frame, avcodec_receive_frame内部每次都先调用
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
        {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
        static int s_print_format = 0;
        if(s_print_format == 0)
        {
            s_print_format = 1;
            print_video_format(frame);
        }

        // 一般H264默认为 AV_PIX_FMT_YUV420P, 具体怎么强制转为 AV_PIX_FMT_YUV420P 在音视频合成输出的时候讲解
        // frame->linesize[1]  对齐的问题
        // 正确写法  linesize[]代表每行的字节数量，所以每行的偏移是linesize[]
        for(int j=0; j<frame->height; j++)
            fwrite(frame->data[0] + j * frame->linesize[0], 1, frame->width, outfile);
        for(int j=0; j<frame->height/2; j++)
            fwrite(frame->data[1] + j * frame->linesize[1], 1, frame->width/2, outfile);
        for(int j=0; j<frame->height/2; j++)
            fwrite(frame->data[2] + j * frame->linesize[2], 1, frame->width/2, outfile);

        // 错误写法 用source.200kbps.766x322_10s.h264测试时可以看出该种方法是错误的
        //  写入y分量
//        fwrite(frame->data[0], 1, frame->width * frame->height,  outfile);//Y
//        // 写入u分量
//        fwrite(frame->data[1], 1, (frame->width) *(frame->height)/4,outfile);//U:宽高均是Y的一半
//        //  写入v分量
//        fwrite(frame->data[2], 1, (frame->width) *(frame->height)/4,outfile);//V：宽高均是Y的一半
    }
}
// 注册测试的时候不同分辨率的问题
// 提取H264: ffmpeg -i source.200kbps.768x320_10s.flv -vcodec libx264 -an -f h264 source.200kbps.768x320_10s.h264
// 提取MPEG2: ffmpeg -i source.200kbps.768x320_10s.flv -vcodec mpeg2video -an -f mpeg2video source.200kbps.768x320_10s.mpeg2
// 播放：ffplay -pixel_format yuv420p -video_size 768x320 -framerate 25  source.200kbps.768x320_10s.yuv
int main(int argc, char **argv)
{
    const char *outfilename;
    const char *filename;
    const AVCodec *codec;
    AVCodecContext *codec_ctx= NULL;
    AVCodecParserContext *parser = NULL;
    int len = 0;
    int ret = 0;
    FILE *infile = NULL;
    FILE *outfile = NULL;
    // AV_INPUT_BUFFER_PADDING_SIZE 在输入比特流结尾的要求附加分配字节的数量上进行解码
    uint8_t inbuf[VIDEO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data = NULL;
    size_t   data_size = 0;
    AVPacket *pkt = NULL;
    AVFrame *decoded_frame = NULL;

    if (argc <= 2)
    {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(0);
    }
    filename    = argv[1];
    outfilename = argv[2];

    pkt = av_packet_alloc();
    enum AVCodecID video_codec_id = AV_CODEC_ID_H264;
    if(strstr(filename, "264") != NULL)
    {
        video_codec_id = AV_CODEC_ID_H264;
    }
    else if(strstr(filename, "mpeg2") != NULL)
    {
        video_codec_id = AV_CODEC_ID_MPEG2VIDEO;
    }
    else
    {
        printf("default codec id:%d\n", video_codec_id);
    }

    // 查找解码器
    codec = avcodec_find_decoder(video_codec_id);  // AV_CODEC_ID_H264
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    // 获取裸流的解析器 AVCodecParserContext(数据)  +  AVCodecParser(方法)
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "Parser not found\n");
        exit(1);
    }
    // 分配codec上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }

    // 将解码器和解码器上下文进行关联
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    // 打开输入文件
    infile = fopen(filename, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    // 打开输出文件
    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        av_free(codec_ctx);
        exit(1);
    }

    // 读取文件进行解码
    data      = inbuf;
    data_size = fread(inbuf, 1, VIDEO_INBUF_SIZE, infile);

    while (data_size > 0)
    {
        if (!decoded_frame)
        {
            if (!(decoded_frame = av_frame_alloc()))
            {
                fprintf(stderr, "Could not allocate audio frame\n");
                exit(1);
            }
        }

        ret = av_parser_parse2(parser, codec_ctx, &pkt->data, &pkt->size,
                               data, data_size,
                               AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0)
        {
            fprintf(stderr, "Error while parsing\n");
            exit(1);
        }
        data      += ret;   // 跳过已经解析的数据
        data_size -= ret;   // 对应的缓存大小也做相应减小

        if (pkt->size)
            decode(codec_ctx, pkt, decoded_frame, outfile);

        if (data_size < VIDEO_REFILL_THRESH)    // 如果数据少了则再次读取
        {
            memmove(inbuf, data, data_size);    // 把之前剩的数据拷贝到buffer的起始位置
            data = inbuf;
            // 读取数据 长度: VIDEO_INBUF_SIZE - data_size
            len = fread(data + data_size, 1, VIDEO_INBUF_SIZE - data_size, infile);
            if (len > 0)
                data_size += len;
        }
    }

    /* 冲刷解码器 */
    pkt->data = NULL;   // 让其进入drain mode
    pkt->size = 0;
    decode(codec_ctx, pkt, decoded_frame, outfile);

    fclose(outfile);
    fclose(infile);

    avcodec_free_context(&codec_ctx);
    av_parser_close(parser);
    av_frame_free(&decoded_frame);
    av_packet_free(&pkt);

    printf("main finish, please enter Enter and exit\n");


    return 0;
}
