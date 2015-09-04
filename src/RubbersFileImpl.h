#ifndef _SRC_RUBBERSFILEIMPL_H_
#define _SRC_RUBBERSFILEIMPL_H_

#include "rubbers/RubbersFile.h"

extern "C" {
  #include <libavutil/avutil.h>
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libavdevice/avdevice.h>
  #include <libswresample/swresample.h>
};
#include <vector>
#include <memory>
#include <map>
class RubbersFile::Impl {
  AVFormatContext                *m_format_ctx  = nullptr;
  AVStream                       *m_stream      = nullptr;
  int                             m_stream_index= -1;
  AVCodecContext                 *m_codec_ctx   = nullptr;
  AVCodec                        *m_codec       = nullptr;
  AVRational                      m_stream_tb { 0, 1 };
  double                          m_stream_tb_d = 0;
  AVRational                      m_codec_tb  { 0, 1 };
  double                          m_codec_tb_d = 0;
  AVRational                      m_output_tb { 0, 1 };
  int                             m_channels;
  int                             m_rate;
  SwrContext                     *m_swr         = nullptr;
  AVPacket                        m_packet;
  std::vector<AVPacket>           m_pkt_array;
  off_t                           m_pkt_index   = 0;
  AVFrame                        *m_orig_frame  = nullptr;
  AVFrame                        *m_frame       = nullptr;
  off_t                           m_cache_pts   = 0;
  off_t                           m_offset      = 0;
  bool                            decode_one_frame ( );
public:
  Impl ( const char *filename, int channels = -1, int rate = -1);
  Impl ( Impl && other ) = default;
  Impl &operator = ( Impl&& other ) = default;
  virtual ~Impl ( );
  virtual void   set_channels(int nch);
  virtual int    channels () const;
  virtual void   set_rate(int srate);
  virtual int    rate() const;
  virtual off_t  seek ( off_t offset, int whence );
  virtual off_t  tell ( ) const;
  virtual size_t read ( float **buf, size_t req );
  virtual size_t length ( ) const;
};

#endif
