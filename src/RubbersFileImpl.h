#ifndef _SRC_RUBBERSFILEIMPL_H_
#define _SRC_RUBBERSFILEIMPL_H_

#include "rubbers/RubbersFile.h"
#include "ff/ff.h"

#include <vector>
#include <memory>
#include <map>

class RubbersFile::Impl {

  avformat_ctx_ptr                m_format_ctx;
  AVStream                       *m_stream      = nullptr;
  int                             m_stream_index= -1;
  avcodec_ctx_ptr                 m_codec_ctx;
  AVCodec                        *m_codec       = nullptr;
  AVRational                      m_stream_tb { 0, 1 };
  double                          m_stream_tb_d = 0;
  AVRational                      m_codec_tb  { 0, 1 };
  double                          m_codec_tb_d = 0;
  AVRational                      m_output_tb { 0, 1 };
  int                             m_channels;
  int                             m_rate;
  swr_ctx_ptr                     m_swr;
  std::vector<avpacket_ptr>         m_pkt_array;
  off_t                           m_pkt_index   = 0;
  avframe_ptr                       m_orig_frame;
  avframe_ptr                       m_frame;
  off_t                           m_cache_pts   = 0;
  off_t                           m_offset      = 0;
  bool                            decode_one_frame ( );
  static std::once_flag           register_once_flag;
  static void                     register_once ( );
public:
  Impl ( const char *filename, int channels = -1, int rate = -1);
  Impl ( Impl && other ) = default;
  Impl &operator = ( Impl&& other ) = default;
  virtual ~Impl ( );
  virtual void   channels(int nch);
  virtual int    channels () const;
  virtual void   rate(int srate);
  virtual int    rate() const;
  virtual off_t  seek ( off_t offset, int whence );
  virtual off_t  tell ( ) const;
  virtual size_t read ( float **buf, size_t req );
  virtual avframe_ptr read_frame ( );
  virtual avframe_ptr  read_frame ( size_t req);
  virtual size_t pread ( float **buf, size_t req, off_t pts);
  virtual size_t length ( ) const;
};

#endif
