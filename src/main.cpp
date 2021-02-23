#include <iostream>
#include <boost/version.hpp>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libavutil/mathematics.h>
}
typedef  struct BufferData {
    uint8_t *ptr;
    uint8_t *ori_ptr;
    size_t size;
    size_t file_size;
} ;

static int write_packet(void *opaque, uint8_t *buf, int buf_size) {
  int fd = *((int *)opaque);
  int ret = write(fd, buf, buf_size);
  return ret;
}

int main() {
    std::cout<< BOOST_LIB_VERSION <<std::endl;
    AVOutputFormat *out_format = NULL;
    AVFormatContext *in_format_ctx = NULL, *out_format_ctx = NULL;
    AVPacket *pkt = NULL;
    AVDictionary *out_format_ctx_opts = NULL;
    AVDictionary *in_format_ctx_opts = NULL;
    av_dict_set(&in_format_ctx_opts, "rtsp_transport", "tcp", 0);
    int in_video_index, in_audio_index, out_video_index, out_audio_index;
    int ret = 0;
    char *rtsp_address = "rtsp://127.0.0.1:8554/live";
    in_format_ctx = avformat_alloc_context();
    BufferData db;
    uint8_t *out_buf = NULL;
    size_t out_buf_size = 4096;
    if (!in_format_ctx) {
        std::cerr << "Failed to alloc input format context";
        goto error_end;
    }
    ret = avformat_open_input(&in_format_ctx, rtsp_address, NULL, &in_format_ctx_opts);
    if (ret < 0) {
        std::cerr << "Could not open rtsp: " << rtsp_address << std::endl;
        goto error_end;
    }
    ret = avformat_find_stream_info(in_format_ctx, NULL);
    if (ret < 0) {
        std::cerr << "Failed to get stream information" << std::endl;
        goto error_end;
    }
    // fd 还没有准备好
    AVIOContext *avio_out = avio_alloc_context(out_buf, out_buf_size, 1, NULL, NULL, write_packet, NULL);
    ret = avformat_alloc_output_context2(&out_format_ctx, NULL, "mp4", NULL);
    out_format_ctx->pb=avio_out;
    if (ret < 0) {
        std::cerr << "Failed to alloc output context";
        goto error_end;
    }
    for (int i = 0; i < in_format_ctx->nb_streams; i++) {
        if (in_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ||
            in_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AVStream *in_stream = in_format_ctx->streams[i];
            AVStream *out_stream = avformat_new_stream(out_format_ctx, NULL);
            if (!out_stream) {
                std::cerr << "Failed to new a output stream" << std::endl;
                goto error_end;
            }
            AVCodecParameters *in_codec_par = in_stream->codecpar;
            if (in_codec_par->codec_type == AVMEDIA_TYPE_VIDEO) {
                in_video_index = in_stream->index;
                out_video_index = out_stream->index;
            }
            if (in_codec_par->codec_type == AVMEDIA_TYPE_AUDIO) {
                in_audio_index = in_stream->index;
                out_audio_index = out_stream->index;
            }
            ret = avcodec_parameters_copy(out_stream->codecpar, in_codec_par);
            if (ret < 0) {
                std::cerr << "Failed to copy codec_par" << std::endl;
                goto error_end;
            }
            out_stream->codecpar->codec_tag = 0;

        }
    }

    av_dict_set(&out_format_ctx_opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+dash+frag_custom", 0);
    av_dict_set(&out_format_ctx_opts, "frag_duration", "0", 0);
    av_dict_set(&out_format_ctx_opts, "min_frag_duration", "0", 0);

    av_dump_format(out_format_ctx, 0, "", 1);
    ret = avformat_write_header(out_format_ctx, &out_format_ctx_opts);
    if (ret < 0) {
        std::cerr << "Failed to write header" << std::endl;
        goto error_end;
    }
    while(true) {
      AVStream *in_stream, *out_stream;
      ret = av_read_frame(in_format_ctx, &pkt);
      if (ret < 0) {
        break;
      }
      // in_format_ctx 是包含多个流的,需要对应好当前package是来自哪个流
      in_stream = in_format_ctx->streams[pkt->stream_index];
      if (pkt->stream_index != in_audio_index || pkt->stream_index != in_video_index) {
        av_packet_free(&pkt);
        continue;
      }
      out_stream = out_format_ctx->streams[pkt->stream_index];

      // copy packet
      pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
      pkt.dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
      pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
      pkt->pos = -1;

      ret = av_interleaved_write_frame(out_format_ctx, &pkt);
      if (ret < 0) {
        std::cerr << "Failed to write frame" << std::endl;
        break;
      }
    }

    av_write_frame(out_format_ctx);

    error_end:
    av_dict_free(&in_format_ctx_opts);
    avformat_free_context(in_format_ctx);
    avformat_free_context(out_format_ctx);
    std::cerr << "end" << std::endl;
    return 0;
}