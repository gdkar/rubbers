#include "RubbersFileImpl.h"
#include <thread>
#include <cstdio>
#include <iostream>
#include <string>
/* static */
std::once_flag RubbersFile::Impl::register_once_flag{};
namespace {
    inline std::string
    ff_err2str ( int ret )
    {
        char str[256];
        av_strerror ( ret, str, sizeof(str) );
        return std::string(str);
    }
}
int
RubbersFile::Impl::channels() const
{
    return m_channels;
}
int
RubbersFile::Impl::rate() const
{
    return m_rate;
}
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
/*static*/void
RubbersFile::Impl::register_once ( )
{
  avcodec_register_all ( );
  avdevice_register_all ( );
  avformat_network_init ( );
  av_register_all ( );
}
RubbersFile::Impl::Impl ( const char *filename, int nch, int srate)
{
  std::call_once ( register_once_flag, &RubbersFile::Impl::register_once );
  auto ret = 0;
  auto dump_msg = [&](const auto &msg){
    std::cerr << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__
              << "(for file " << filename << "):\terror " << ret << " ( " << ff_err2str(ret) <<")\t" << msg << std::endl;
    return std::ref(std::cerr);
  };
  if ( ( ret = m_format_ctx.open_input(filename, nullptr, nullptr) ) < 0 ) {
      dump_msg("error opening file.");
      return;
  }
  if ( ( ret = m_format_ctx.find_stream_info(nullptr)) < 0 ){
    dump_msg("error finding stream info");
    return;
  }
  std::for_each ( &m_format_ctx->streams[0],
                  &m_format_ctx->streams[m_format_ctx->nb_streams],
                  ([](auto &s ){ s->discard = AVDISCARD_ALL;})
    );
  if ( ( m_stream_index = ret = m_format_ctx.find_best_stream( AVMEDIA_TYPE_AUDIO, -1, -1, &m_codec, 0 ) ) < 0 ){
    dump_msg("error finding audio stream.");
    return;
  }
  std::cerr << "found stream " << m_stream_index << " for " << filename << std::endl;
  m_stream       = m_format_ctx->streams[m_stream_index];
  m_stream_tb    = m_stream->time_base;
  m_stream->discard = AVDISCARD_NONE;
  m_format_ctx.dump(0, filename, false );
  if ( ! m_codec && !(m_codec = avcodec_find_decoder ( m_stream->codecpar->codec_id ) ) ) {
    ret=AVERROR(ENOENT);dump_msg("error finding codec.");
    return;
  }
  m_codec_ctx.alloc(m_stream->codecpar);
  if ( !(m_codec_ctx )) {
    ret = AVERROR(ENOMEM);dump_msg("error allocating codec context.");
    return;

  }
  m_codec_ctx.open();
  if( (ret = m_codec_ctx.open(m_codec, nullptr) ) < 0 ) {
    dump_msg("error opening codec context.");
    return;
  }
  m_codec_tb = m_codec_ctx->time_base;
  auto pkt = packet_ptr();
  pkt.alloc();
  auto discarded = size_t{0};
  while(ret != AVERROR_EOF) {
    ret = m_format_ctx.read_frame(pkt);
    if ( ret < 0 ) {
      if ( ret != AVERROR_EOF && ret != AVERROR(EAGAIN) ){
          dump_msg("error reading packet, codec=" + std::string(m_codec->long_name));
      }
      pkt.unref();
      if ( ret == AVERROR(EAGAIN) )
        ret = 0;
      continue;
    }
    if ( pkt->stream_index != m_stream_index ) {
      discarded ++;
      pkt.unref();
    } else {
      m_pkt_array.push_back(pkt);
      pkt.unref();
    }
  }
  m_swr.alloc();
  m_frame.alloc();
  m_orig_frame.alloc();
  if ( discarded )
    std::cerr << "discarded " << discarded << " packets from non-audio streams for " << filename << std::endl;
  if ( m_pkt_array.size() )
    std::cerr << "accepted " << m_pkt_array.size() << " packets for audio stream " << m_stream_index << " for file " << filename << std::endl;
  if ( nch <= 0 ) {
    m_channels = m_codec_ctx->channels;
  } else {
    m_channels = nch;
  }
  if ( srate <= 0 ) {
    m_rate = m_codec_ctx->sample_rate;
  } else {
    m_rate = srate;
  }
  m_output_tb = AVRational{1,m_rate };
  m_pkt_index = 0;
  decode_one_frame ( );
  m_offset    = 0;
  std::cerr << "demuxed " << m_pkt_array.size() << " packets for " << filename;
  std::cerr << filename << " " << channels() << " channels, " << rate() << " rate, " << m_pkt_array.size() << " packets, " << length() << " samples\n";
}

RubbersFile::Impl::~Impl ( ) = default;
size_t RubbersFile::Impl::length ( ) const {
  return av_rescale_q ( m_pkt_array.back()->pts + m_pkt_array.back()->duration, m_stream_tb, m_output_tb );
}
off_t RubbersFile::Impl::seek ( off_t offset, int whence )
{
  if ( m_frame->pts == AV_NOPTS_VALUE && !decode_one_frame() )
      return -1;
  auto first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
  if ( whence == SEEK_CUR ) {
    offset += first_sample + m_offset;
  }else if ( whence == SEEK_END ) {
    offset += length();
  }else if ( whence != SEEK_SET ) {
    return first_sample + m_offset;
  }
  if ( first_sample <= offset && first_sample + m_frame->nb_samples > offset ) {
    m_offset = offset - first_sample;
    return offset;
  }else if ( offset >= av_rescale_q ( m_pkt_array.back()->pts, m_stream_tb, m_output_tb ) ) {
    m_pkt_index = m_pkt_array.size() - 2;
    m_codec_ctx.flush();
    decode_one_frame ();
    first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
    m_offset = offset - first_sample;
    if ( m_offset < 0 )
        m_offset = 0;
    return offset;
  } else if ( offset < av_rescale_q ( m_pkt_array.front()->pts + m_pkt_array.front()->duration, m_stream_tb, m_output_tb ) ) {
    m_pkt_index = 0;
    m_codec_ctx.flush();
    decode_one_frame ();
    first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
    if ( offset < first_sample )
        offset = first_sample;
    m_offset = offset - first_sample;
    return offset;
  }
  auto hindex = m_pkt_array.size() - 1;
  auto lindex = decltype(hindex){0};
  auto hpts   = m_pkt_array.at(hindex)->pts;
  auto lpts   = m_pkt_array.at(lindex)->pts;
  auto target_pts = av_rescale_q ( offset, m_output_tb, m_stream_tb );
  auto bail = false;
  while ( hindex > lindex + 1 ) {
    auto time_dist  = hpts       - lpts;
    auto index_dist = hindex     - lindex;
    auto time_frac  = target_pts - lpts;
    auto mindex     = decltype(hindex)(( ( time_frac * index_dist ) / time_dist )) + lindex;
    if ( mindex <= lindex )  {
      if ( bail ) {
        mindex = ( index_dist / 2 ) + lindex;
      } else {
        mindex = lindex + 1;
        bail   = true;
      }
    } else {
      bail = false;
    }
    auto mpts = m_pkt_array.at ( mindex )->pts;
    auto npts = m_pkt_array.at ( mindex + 1 )->pts;
    if ( mpts > target_pts ) {
      hindex = mindex;
      hpts   = mpts;
    } else if ( npts <= target_pts ) {
      lindex = mindex + 1;
      lpts   = npts;
    } else {
      lindex = mindex;
      break;
    }
  }
  m_pkt_index = lindex;
  if(m_pkt_index > 0 )
      m_pkt_index--;
  m_codec_ctx.flush();
  avcodec_flush_buffers(m_codec_ctx.get());
  decode_one_frame ();
  first_sample = av_rescale_q ( m_frame->pts, m_stream_tb, m_output_tb );
  m_offset = offset - first_sample;
  return offset;
}
bool
RubbersFile::Impl::decode_one_frame ( )
{
    while(auto ret = m_codec_ctx.receive_frame(m_orig_frame)) {
        if(ret == AVERROR(EAGAIN)) {
            m_pkt_index++;
            if ( size_t(m_pkt_index) >= m_pkt_array.size() ) {
                m_pkt_index    = m_pkt_array.size();
                if(m_codec_ctx.send_packet(nullptr))
                    return false;
            }else{
                if(m_codec_ctx.send_packet(m_pkt_array.at(m_pkt_index)))
                    return false;
            }
        }else{
            return false;
        }
    }
    m_orig_frame->pts = m_orig_frame.get_best_effort_timestamp();
    m_frame.unref();
    m_frame->format         = AV_SAMPLE_FMT_FLTP;
    m_frame->channel_layout = av_get_default_channel_layout ( channels () );
    m_frame->sample_rate    = rate();
    if ( !m_swr.initialized()) {
        if ( !m_swr.config(m_frame,m_orig_frame)
           ||!m_swr.init()) {
            return false;
        }
    }
    auto delay = m_swr.delay(rate());
    m_frame->pts = m_orig_frame.get_pts()- av_rescale_q ( delay, m_output_tb, m_stream_tb );
    if ( !m_swr.convert(m_frame,m_orig_frame) ) {
        if ( !m_swr.config(m_frame,m_orig_frame)
         ||  !m_swr.convert(m_frame,m_orig_frame)) {
            return false;
        }
    }
    return true;
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
  while ( number_done < req ) {
    if ( m_offset >= m_frame.get_samples()) {
      m_offset -= m_frame.get_samples();
      if ( !decode_one_frame () )
          break;
    } else {
      auto available_here = m_frame.get_samples() - m_offset;
      auto needed_here    = req - number_done;
      auto this_chunk     = std::min<off_t>(available_here, needed_here );
      if ( this_chunk <= 0 )
          break;
      if ( buf && buf[0]) {
        m_frame.copy_to(reinterpret_cast<uint8_t**>(buf), number_done, m_offset, this_chunk);
      }
      number_done += this_chunk;
      m_offset    += this_chunk;
    }
  }
  return number_done;
}
