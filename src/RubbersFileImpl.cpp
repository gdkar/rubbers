#include "RubbersFileImpl.h"
#include <thread>
#include <cstdio>
#include <iostream>
#include <string>
static inline std::string
ff_err2str ( int ret )
{
  char str[256];
  av_strerror ( ret, str, sizeof(str) );
  return std::string(str);
}
int
RubbersFile::Impl::channels() const{return m_channels;}
int
RubbersFile::Impl::rate() const { return m_rate;}
void
RubbersFile::Impl::set_channels(int nch )
{
  m_channels = nch;
}
void
RubbersFile::Impl::set_rate(int srate )
{
  m_rate = srate;
}
RubbersFile::Impl::Impl ( const char *filename, int nch, int srate)
{
  av_register_all();
  m_format_ctx = avformat_alloc_context ( );
  auto opts = (AVDictionary*)nullptr;
  auto ret = 0;
  if ( ( ret = avformat_open_input ( &m_format_ctx, filename, nullptr, &opts ) ) < 0 )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error opening file " << filename 
      << ", " << ff_err2str(ret) << std::endl;
    return;
  }
  av_dict_free ( &opts );
  if ( ( ret = avformat_find_stream_info ( m_format_ctx, nullptr ) ) < 0 )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error finding stream info for file " << filename 
      << ", " << ff_err2str(ret) << std::endl;
    return;
  }
  std::for_each ( &m_format_ctx->streams[0],
                  &m_format_ctx->streams[m_format_ctx->nb_streams],
                  ([=](auto &s ){ s->discard = AVDISCARD_ALL;})
    );
  if ( ( ret = av_find_best_stream ( m_format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &m_codec, 0 ) ) < 0 )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error finding audio stream for file " << filename 
      << ", " << ff_err2str(ret) << std::endl;
    return;
  }
  m_stream_index = ret;
  std::cerr << "found stream " << m_stream_index << " for " << filename << std::endl;
  m_stream       = m_format_ctx->streams[m_stream_index];
  m_stream_tb    = m_stream->time_base;
  m_stream->discard = AVDISCARD_NONE;
  av_dump_format ( m_format_ctx, 0, filename, false );
  if ( ! m_codec && !(m_codec = avcodec_find_decoder ( m_stream->codec->codec_id ) ) )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error finding codec for file " << filename 
      << ", " << ff_err2str(ret) << std::endl;
    return;
  }
  if ( !(m_codec_ctx       = avcodec_alloc_context3 ( m_codec ) ) )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error allocating codec context for file " << filename 
      << " with codec " << m_codec->long_name
      << ", " << ff_err2str(AVERROR(ENOMEM)) << std::endl;
    return;

  }
  if( (ret = avcodec_copy_context ( m_codec_ctx, m_stream->codec ) ) < 0 )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error copying codec context for " << filename 
      << " with codec " << m_codec->long_name
      << ", " << ff_err2str(ret) << std::endl;
    return;
  }
  av_dict_set(&opts,"refcounted_frames","1",0);
  if( (ret = avcodec_open2( m_codec_ctx, m_codec, &opts) ) < 0 )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error opening codec context for " << filename 
      << " with codec " << m_codec->long_name
      << ", " << ff_err2str(ret) << std::endl;
    return;
  }
  m_codec_tb = m_codec_ctx->time_base;
  av_init_packet ( &m_packet );
  m_packet.data = nullptr;
  m_packet.size = 0;
  auto discarded = size_t{0};
  while(ret != AVERROR_EOF)
  {
    ret = av_read_frame ( m_format_ctx, &m_packet);
    if ( ret < 0 )
    {
      if ( ret != AVERROR_EOF && ret != AVERROR(EAGAIN) ){
      std::cerr 
        << __FILE__ 
        << " line " << __LINE__ 
        << " in " << __FUNCTION__ 
        << ": error reading packet for " << filename 
        << " with codec " << m_codec->long_name
        << ", " << ff_err2str(ret) << std::endl;
      }
      av_free_packet ( &m_packet );
      m_packet.data = nullptr;
      m_packet.size = 0;
      if ( ret == AVERROR(EAGAIN) )
      {
        ret = 0;
      }
      continue;
    }
    if ( m_packet.stream_index != m_stream_index )
    {
      discarded ++;
      av_free_packet ( &m_packet );
    }
    else
    {
      av_dup_packet ( &m_packet);
      m_pkt_array.push_back(m_packet);
      av_init_packet ( &m_packet );
    }
  }
  if ( !(m_swr         = swr_alloc ( ) )
    || !(m_orig_frame  = av_frame_alloc ( ) )
    || !(m_frame       = av_frame_alloc ( ) ))
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error allocating codec context for file " << filename 
      << " with codec " << m_codec->long_name
      << ", " << ff_err2str(AVERROR(ENOMEM)) << std::endl;
    return;

  }
  if ( discarded )
  {
    std::cerr << "discarded " << discarded << " packets from non-audio streams for " << filename << std::endl;
  }
  if ( m_pkt_array.size() )
  {
    std::cerr << "accepted " << m_pkt_array.size() << " packets for audio stream " << m_stream_index << " for file " << filename << std::endl;
  }
  av_init_packet ( &m_packet );
  m_packet.data = nullptr;
  m_packet.size = 0;
  if ( nch <= 0 )
  {
    m_channels = m_codec_ctx->channels;
  }
  else
  {
    m_channels = nch;
  }
  if ( srate <= 0 )
  {
    m_rate = m_codec_ctx->sample_rate;
  }
  else
  {
    m_rate = srate;
  }
  m_output_tb = AVRational{1,m_rate };
  m_pkt_index = 0;
  if ( m_pkt_index < m_pkt_array.size () )
  {
    m_packet = m_pkt_array.at ( m_pkt_index );
  }
  decode_one_frame ( );
  m_offset    = 0;
  std::cerr << "demuxed " << m_pkt_array.size() << " packets for " << filename;
  std::cerr << filename << " " << channels() << " channels, " << rate() << " rate, " << m_pkt_array.size() << " packets, " << length() << " samples\n";
}

RubbersFile::Impl::~Impl ( )
{
  av_frame_free ( &m_orig_frame  );
  av_frame_free ( &m_frame       );
  swr_free      ( &m_swr         );
  for ( auto & pkt : m_pkt_array )
  {
    av_free_packet ( &pkt        );
  }
  m_pkt_array.clear ( );
  avcodec_close ( m_codec_ctx );
  avcodec_free_context ( &m_codec_ctx );
  avformat_close_input ( &m_format_ctx );
}
size_t RubbersFile::Impl::length ( ) const {
  return av_rescale_q ( m_pkt_array.back().pts + m_pkt_array.back().duration, m_stream_tb, m_output_tb );
}
off_t RubbersFile::Impl::seek ( off_t offset, int whence )
{
  if ( m_frame->pts == AV_NOPTS_VALUE && !decode_one_frame() ) return -1;
  auto first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
  if ( whence == SEEK_CUR )
  {
    offset += first_sample + m_offset;
  }else if ( whence == SEEK_END )
  {
    offset += length();
  }else if ( whence != SEEK_SET )
  {
    return first_sample + m_offset;
  }
  if ( first_sample <= offset && first_sample + m_frame->nb_samples > offset )
  {
    m_offset = offset - first_sample;
    return offset;
  }
  else if ( offset >= av_rescale_q ( m_pkt_array.back().pts, m_stream_tb, m_output_tb ) )
  {
    m_pkt_index = m_pkt_array.size() - 1;
    m_packet    = m_pkt_array.at(m_pkt_index);
    decode_one_frame ();
    first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
    m_offset = offset - first_sample;
    if ( m_offset < 0 ) m_offset = 0;
    return offset;
  }
  else if ( offset < av_rescale_q ( m_pkt_array.front().pts + m_pkt_array.front().duration, m_stream_tb, m_output_tb ) )
  {
    m_pkt_index = 0;
    m_packet    = m_pkt_array.at(m_pkt_index );
    decode_one_frame ();
    first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
    if ( offset < first_sample ) offset = first_sample;
    m_offset = offset - first_sample;
    return offset;
  }
  auto hindex = m_pkt_array.size() - 1;
  auto lindex = decltype(hindex){0};
  auto hpts   = m_pkt_array.at(hindex).pts;
  auto lpts   = m_pkt_array.at(lindex).pts;
  auto target_pts = av_rescale_q ( offset, m_output_tb, m_stream_tb );
  auto iteration = 0;
  auto bail = 0;
  while ( hindex > lindex + 1 )
  {
    iteration++;
    auto time_dist  = hpts       - lpts;
    auto index_dist = hindex     - lindex;
    auto time_frac  = target_pts - lpts;
    auto mindex     = decltype(hindex)(( ( time_frac * index_dist ) / time_dist )) + lindex;
    if ( mindex <= lindex ) 
    {
      if ( bail == 0 )
      {
        mindex = lindex + 1;
        bail   = 1;
      }
      else
      {
        mindex = ( index_dist / 2 ) + lindex;
      }
    }
    else
    {
      bail = 0;
    }
    auto mpts = m_pkt_array.at ( mindex ).pts;
    auto npts = m_pkt_array.at ( mindex + 1 ).pts;
    std::cerr << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__ << std::endl;
    std::cerr << "iteration " << iteration << ": lindex = " << lindex << " , hindex = " << hindex << ", time_dist = " << time_dist << 
      ", time_frac = " << time_frac << ", mindex = " << mindex << " target_pts = " << target_pts << " mpts = " << mpts << std::endl;
    if ( mpts > target_pts )
    {
      hindex = mindex;
      hpts   = mpts;
    }
    else if ( npts <= target_pts )
    {
      lindex = mindex + 1;
      lpts   = npts;
    }
    else
    {
      lindex = mindex;
      hindex = mindex + 1;
    }
  }
  m_pkt_index = lindex;
  m_packet    = m_pkt_array.at ( m_pkt_index );
  decode_one_frame ();
  if ( m_frame->pts > target_pts || m_frame->pts + av_rescale_q ( m_frame->nb_samples, m_output_tb, m_stream_tb ) <= target_pts )
  {
    std::cerr 
      << __FILE__ 
      << " line " << __LINE__ 
      << " in " << __FUNCTION__ 
      << ": error in seek: seeking to " << offset << " with pts " << target_pts << " failed\n";
  }
  first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
  m_offset = offset - first_sample;
  return offset;
}
bool
RubbersFile::Impl::decode_one_frame ( )
{
  while ( true )
  {
    if ( m_packet.size <= 0  )
    {
      m_pkt_index++;
      if ( m_pkt_index >= m_pkt_array.size() )
      {
        m_pkt_index    = m_pkt_array.size();
        av_init_packet ( &m_packet );
        m_packet.data = nullptr;
        m_packet.size = 0;
        return false;
      }
      m_packet = m_pkt_array.at ( m_pkt_index );
      continue;
    }
    else
    {
      auto ret = int{0}, got_frame = int{0};
      av_frame_unref ( m_orig_frame );
      ret = avcodec_decode_audio4 ( m_codec_ctx, m_orig_frame, &got_frame, &m_packet );
      if ( ret < 0 )
      {
        std::cerr 
          << __FILE__ 
          << " line " << __LINE__ 
          << " in " << __FUNCTION__ 
          << ": error decoding packet"
          << ", " << ff_err2str(ret) << std::endl;
        m_packet.size = 0;
        m_packet.data = nullptr;
        continue;
      }
      else
      {
        if ( ret == 0 || ret > m_packet.size )
        {
          ret = m_packet.size;
        }
        m_packet.size -= ret;
        m_packet.data += ret;
        if ( got_frame )
        {
          m_orig_frame->pts = av_frame_get_best_effort_timestamp ( m_orig_frame );
          av_frame_unref ( m_frame );
          m_frame->format         = AV_SAMPLE_FMT_FLTP;
          m_frame->channel_layout = av_get_default_channel_layout ( channels () );
          m_frame->sample_rate    = rate();
          if ( !swr_is_initialized ( m_swr ) )
          {
            if ( ( ret = swr_config_frame ( m_swr, m_frame, m_orig_frame ) ) < 0 
              || ( ret = swr_init (  m_swr ) ) < 0 )
            {
              std::cerr 
                << __FILE__ 
                << " line " << __LINE__ 
                << " in " << __FUNCTION__ 
                << ": error configuring SwrContext"
                << ", " << ff_err2str(ret) << std::endl;
              return false;
            }
          }
          auto delay = swr_get_delay ( m_swr, rate () );
          m_frame->pts = m_orig_frame->pts - av_rescale_q ( delay, m_output_tb, m_stream_tb );
          if ( ( ret = swr_convert_frame ( m_swr, m_frame, m_orig_frame ) ) < 0 )
          {
            if ( ( ret = swr_config_frame ( m_swr, m_frame, m_orig_frame ) ) < 0 
              || ( ret = swr_convert_frame ( m_swr, m_frame, m_orig_frame ) ) < 0 )
            {
              std::cerr 
                << __FILE__ 
                << " line " << __LINE__ 
                << " in " << __FUNCTION__ 
                << ": error converting frame"
                << ", " << ff_err2str(ret) << std::endl;
              return false;
            }
          }
          return true;
        }
        else
        {
          std::cerr << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__ << " failed to get a frame. " << std::endl;
        }
      }
    }
  }
  return false;
}
off_t 
RubbersFile::Impl::tell ( ) const
{
  return av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb ) + m_offset;
}
size_t 
RubbersFile::Impl::read ( float **buf, size_t req )
{
  auto number_done = decltype(req){0};
  while ( number_done < req )
  {
    if ( m_offset >= m_frame->nb_samples )
    {
      m_offset -= m_frame->nb_samples;
      if ( !decode_one_frame () ) break;
    }
    else
    {
      auto available_here = m_frame->nb_samples - m_offset;
      auto needed_here    = req - number_done;
      auto this_chunk     = std::min<off_t>(available_here, needed_here );
      if ( this_chunk <= 0 ) break;
      if ( buf && buf[0])
      {
        av_samples_copy (
            reinterpret_cast<uint8_t**>(buf),
            m_frame->data,
            number_done,
            m_offset,
            this_chunk,
            channels(),
            AV_SAMPLE_FMT_FLTP
            );
      }
      number_done += this_chunk;
      m_offset    += this_chunk;
    }
  }
  return number_done;
}
