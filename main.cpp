


#include <stdio.h>
#include <iostream>


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

//通过预处理指令导入库
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"swresample.lib")

#include "LX_Demux.h"
#include <thread>
#include <string>

using namespace std;

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};

int get_file_size(FILE * file_handle)
{
    //获取当前读取文件的位置 进行保存
    unsigned int current_read_position = ftell(file_handle);
    int file_size;
    fseek(file_handle, 0, SEEK_END);
    //获取文件的大小
    file_size = ftell(file_handle);
    //恢复文件原来读取的位置
    fseek(file_handle, current_read_position, SEEK_SET);
    return file_size;
}



static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);

    if (!buf_size)
        return AVERROR_EOF;
    //printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr += buf_size;
    bd->size -= buf_size;

    return buf_size;
}

int main()
{
    av_register_all();
    //264裸流 AVFormatContext打开
    ///////////////////////////////////////////////////////////
    FILE * fp = fopen("3.264", "ab+");
    int raw_data_size = get_file_size(fp);
    unsigned char*raw_data = new unsigned char[raw_data_size];
    int true_size = fread(raw_data, 1, raw_data_size, fp);
    printf("true_size = %d\n", true_size);

    struct buffer_data bd = { 0 };
    bd.ptr = raw_data;
    bd.size = raw_data_size;

    unsigned char*avio_ctx_buffer = NULL;
    int avio_ctx_buffer_size = 4096;
    avio_ctx_buffer = (unsigned char*)av_malloc(avio_ctx_buffer_size);

    AVFormatContext *fmt_ctx = NULL;
    fmt_ctx = avformat_alloc_context();
    AVIOContext *avio_ctx = NULL;
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, &bd, &read_packet, NULL, NULL);
    fmt_ctx->pb = avio_ctx;

    if (avformat_open_input(&fmt_ctx, NULL, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return -1;
    }

    int videoStreamIndex = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if ((avformat_find_stream_info(fmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        return -1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////



	unsigned char chs[100] = { 0 };

	memcpy(chs, fmt_ctx->streams[0]->codecpar->extradata, fmt_ctx->streams[0]->codecpar->extradata_size);











    //MP4目标AVFormatContext创建
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //rtmp flv 封装器
    AVFormatContext *ic = 0;
    std::string filename = "6.mp4";
    int ret = avformat_alloc_output_context2(&ic, 0, 0, filename.c_str());
    if (ret != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        cout << buf;
        return -1;                                                                       
    }



    //添加视频流 
    AVStream *st = avformat_new_stream(ic, NULL);
    if (!st)
    {
        cout << "avformat_new_stream failed" << endl;
        return -1;
    }
    ret = avcodec_parameters_copy(st->codecpar, fmt_ctx->streams[videoStreamIndex]->codecpar);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy codec parameters\n");
        return -1;
    }
    st->codecpar->codec_tag = 0;
    av_dump_format(ic, 0, filename.c_str(), 1);

    ret = avio_open(&ic->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        cout << buf << endl;
        return -1;
    }
    
    

    //写入封装头
    ret = avformat_write_header(ic, NULL);
    if (ret < 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        cout << buf << endl;
        return -1;
    }

    int pts = 0, dts = 0;
    int fps = 24;

    for (;;)
    {
        AVPacket * pkt = av_packet_alloc();
        //读取一帧并分配空间
        int re = av_read_frame(fmt_ctx, pkt);
        if (re != 0)
        {
            printf("av_read_frame 失败\n");
            break;
        }
        else
        {

			unsigned char ch1[100] = { 0 };

			memcpy(ch1, pkt->data, 100);


            pkt->pts = pts;
            pkt->dts = dts;
            pts += (double)(fmt_ctx->streams[pkt->stream_index]->time_base.den) / (fmt_ctx->streams[pkt->stream_index]->time_base.num) / fps;
            dts += (double)(fmt_ctx->streams[pkt->stream_index]->time_base.den) / (fmt_ctx->streams[pkt->stream_index]->time_base.num) / fps;
            printf("av_read_frame 成功\n");

            pkt->pts = av_rescale_q_rnd(pkt->pts,
                fmt_ctx->streams[pkt->stream_index]->time_base,
                ic->streams[pkt->stream_index]->time_base,
                (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
            );
            pkt->dts = av_rescale_q_rnd(pkt->dts,
                fmt_ctx->streams[pkt->stream_index]->time_base,
                ic->streams[pkt->stream_index]->time_base,
                (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
            );
            pkt->pos = -1;
            pkt->duration = av_rescale_q_rnd(pkt->duration,
                fmt_ctx->streams[pkt->stream_index]->time_base,
                ic->streams[pkt->stream_index]->time_base,
                (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
            );
            ret = av_interleaved_write_frame(ic, pkt);
            if (re == 0) printf("write success\n");
            else printf("write failed\n");
        }
        
    }

    //写入尾部信息索引
    if (av_write_trailer(ic) != 0)
    {
        cerr << "av_write_trailer failed!" << endl;
        return -1;
    }
    cout << "WriteEnd success!" << endl;

    if (ic)
    {
        avformat_close_input(&ic);
    }


    avformat_close_input(&fmt_ctx);
    if (avio_ctx)
        av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);

    return 0;
}

