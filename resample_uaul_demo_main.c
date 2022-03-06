//
// Created by zyc on 2022/3/6.
//
*/

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include "resmpler_urual_demo.h"
static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
            { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
            { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
            { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
            { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
            { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "Sample format %s not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return AVERROR(EINVAL);
}

/**
 * Fill dst buffer with nb_samples, generated starting from t.
 */
static void fill_samples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t)
{
    int i, j;
    double tincr = 1.0 / sample_rate, *dstp = dst;
    const double c = 2 * M_PI * 440.0;

    /* generate sin tone with 440Hz frequency and duplicated channels */
    for (i = 0; i < nb_samples; i++) {
        *dstp = sin(c * *t);
        for (j = 1; j < nb_channels; j++)
            dstp[j] = dstp[0];
        dstp += nb_channels;
        *t += tincr;
    }
}

int main(int argc,char **argv){
    int64_t src_ch_layout = AV_CH_LAYOUT_STEREO;
    int src_rate = 48000;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_DBL;
    int src_nb_channels = 0;
    uint8_t **src_data = NULL;  // 二级指针
    int src_linesize;
    int src_nb_samples = 1024;


    // 输出参数
    int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO;
    int dst_rate = 44100;
    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;
    int dst_nb_channels = 0;
    uint8_t **dst_data = NULL;  //二级指针
    int dst_linesize;
    int dst_nb_samples = 1152; // 固定为1152   because mp3 format sample size is a changeless

    //output file
    const char *dst_filename = NULL;
    FILE *dst_file;
    int dst_bufsize;
    const char *fmt;

    audio_resampler_params_t resamplerParams;
    resamplerParams.src_channel_layout = src_ch_layout;
    resamplerParams.src_sample_rate = src_rate;
    resamplerParams.src_sample_fmt = src_sample_fmt;
    resamplerParams.dst_channel_layout= dst_ch_layout;
    resamplerParams.dst_sample_rate=dst_rate;
    resamplerParams.dst_sample_fmt=dst_sample_fmt;

    audio_resampler_t *audioResampler = audio_resampler_alloc(resamplerParams);
    if (!audioResampler){
        printf("audio_resampler_alloc failed\n");
        goto end;
    }
    double t;
    int ret;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s output_file\n"
                        "API example program to show how to resample an audio stream with libswresample.\n"
                        "This program generates a series of audio frames, resamples them to a specified "
                        "output format and rate and saves them to an output file named output_file.\n",
                argv[0]);
        exit(1);
    }
    dst_filename = argv[1];

    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        exit(1);
    }
    //query the number of channels of the input source by enumeration type
    src_nb_samples = av_get_channel_layout_nb_channels(src_nb_channels);
    // Allocate space for input source data through the number of channels queried above
    ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
                                             src_nb_samples, src_sample_fmt, 0);

    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        goto end;
    }
    //The same of above is just an output source
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
    //MP3 format
    dst_nb_samples = 1152;
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                             dst_nb_samples, dst_sample_fmt, 0);

    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        goto end;
    }

    t = 0;
    int64_t in_pts = 0;
    int64_t out_pts = 0;

    do {
        //generate synthetic audio

        fill_samples((double *)src_data[0], src_nb_samples, src_nb_channels, src_rate, &t);

        int ret_size = audio_resampler_send_frame2(audioResampler, src_data, src_nb_samples, in_pts);
        in_pts += src_nb_samples;
        ret_size = audio_resampler_receive_frame2(audioResampler, dst_data, dst_nb_samples, &out_pts);

        if(ret_size > 0) {
            dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                     ret_size, dst_sample_fmt, 1);
            if (dst_bufsize < 0) {
                fprintf(stderr, "Could not get sample buffer size\n");
                goto end;
            }
            printf("t:%f in:%d out:%d, out_pts:%"PRId64"\n", t, src_nb_samples, ret_size, out_pts);
            fwrite(dst_data[0], 1, dst_bufsize, dst_file);
        } else {
            printf("can't get %d samples, ret_size:%d, cur_size:%d\n", dst_nb_samples, ret_size,
                   audio_resampler_get_fifo_size(audioResampler));
        }
    } while (t < 10);  // We can adjust T check the memory usage
    // resample encapsulation   add  fifo_size relevant events
    // The resample data is transferred into FIFO  if it meets fifo_buffer size bigger the dis_sample size  read data otherwise add in fifo_buffer
    audio_resampler_send_frame2(audioResampler, NULL, 0, 0);
    int fifo_size = audio_resampler_get_fifo_size(audioResampler);
    int get_size = (fifo_size >dst_nb_samples ? dst_nb_samples : fifo_size);
    int ret_size = audio_resampler_receive_frame2(audioResampler, dst_data, get_size, &out_pts);

    if(ret_size > 0) {
        printf("flush ret_size:%d\n", ret_size);
        // 不够一帧的时候填充为静音, 这里的目的是补偿最后一帧如果不够采样点数不足1152，用静音数据进行补足
        av_samples_set_silence(dst_data, ret_size, dst_nb_samples - ret_size, dst_nb_channels, dst_sample_fmt);
        dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                 dst_nb_samples, dst_sample_fmt, 1);
        if (dst_bufsize < 0) {
            fprintf(stderr, "Could not get sample buffer size\n");
            goto end;
        }
        printf("flush in:%d out:%d, out_pts:%"PRId64"\n", 0, ret_size, out_pts);
        fwrite(dst_data[0], 1, dst_bufsize, dst_file);
    }




    if ((ret = get_format_from_sample_fmt(&fmt, dst_sample_fmt)) < 0)
        goto end;
    fprintf(stderr, "Resampling succeeded. Play the output file with the command:\n"
                    "ffplay -f %s -channel_layout %"PRId64" -channels %d -ar %d %s\n",
            fmt, dst_ch_layout, dst_nb_channels, dst_rate, dst_filename);
end:
    fclose(dst_file);
    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);

    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);


    return -1;
}