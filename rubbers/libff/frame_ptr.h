_Pragma("once")

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
struct frame_ptr {
    AVFrame     *m_d{nullptr};
    frame_ptr() = default;
    frame_ptr(AVFrame *f)
        : m_d(f) { }
    frame_ptr(frame_ptr &&o)
        : m_d(o.release()) { }
    frame_ptr(const frame_ptr &o)
        : m_d(av_frame_clone(o)) { }
    frame_ptr &operator = (AVFrame *f)
    {
        reset(f);
        return *this;
    }
    frame_ptr &operator = (const AVFrame *f)
    {
        if(f != m_d)
            reset(av_frame_clone(f));
        return *this;
    }
    frame_ptr &operator =(frame_ptr &&o)
    {
        if(m_d) {
            ref(o);
        }else{
            reset(o.release());
        }
        return *this;
    }
    frame_ptr &operator = (const frame_ptr &o)
    {
        reset(av_frame_clone(o.m_d));
        return *this;
    }
    void swap(frame_ptr &o) noexcept { std::swap(m_d,o.m_d);}
    void reset(AVFrame *f = nullptr) { av_frame_free(&m_d); m_d = f;}
    AVFrame *release() { auto ret = m_d; m_d = nullptr; return ret;}
    AVFrame *get() const { return m_d;}
   ~frame_ptr() { reset();}
    AVFrame *operator ->() { return m_d; }
    AVFrame *operator ->() const { return m_d;}
    AVFrame &operator *() { return *m_d;}
    const AVFrame &operator *() const { return *m_d;}
    operator bool() const { return !!m_d;}
    operator AVFrame *() const { return m_d;}
    void alloc() { if(!m_d) m_d = av_frame_alloc(); else unref();}
    void unref() { av_frame_unref(m_d); }
    void ref(frame_ptr &o) { if(o.m_d != m_d) { unref(); av_frame_ref(m_d,o.m_d); } }
    bool make_writable() { return m_d && !av_frame_make_writable(m_d);}
    bool writable() { return m_d && av_frame_is_writable(m_d);}
    bool get_buffer() { return m_d && !av_frame_get_buffer(m_d, 16);}
    int64_t get_best_effort_timestamp() const { return av_frame_get_best_effort_timestamp(m_d);}
    int64_t get_pkt_duration() const { return av_frame_get_pkt_duration(m_d);}
    int64_t get_pkt_pos() const { return av_frame_get_pkt_pos(m_d);}
    int64_t get_pkt_pts() const { return m_d->pkt_pts;}
    int64_t get_pkt_dts() const { return m_d->pkt_dts;}
    int64_t get_channel_layout() const { return av_frame_get_channel_layout(m_d);}
    int     get_channels() const { return av_frame_get_channels(m_d);}
    int     get_sample_rate() const { return av_frame_get_sample_rate(m_d);}
    int     get_pkt_size() const { return av_frame_get_pkt_size(m_d);}
    AVSampleFormat get_format() const { return static_cast<AVSampleFormat>(m_d->format);}
    int     get_samples() const { return m_d->nb_samples;}
    int64_t get_pts() const { return m_d->pts;}
    int     copy(AVFrame *o) { return av_frame_copy(m_d,o);}
    int     copy_props(AVFrame *o) { return av_frame_copy_props(m_d,o);}
    int     copy_to(AVFrame *dst, int dst_offset, int src_offset, int nb_samples)
    {
        if(!dst || dst->format != get_format())
            return -EINVAL;
        if(dst->nb_samples < dst_offset + nb_samples
        || m_d->nb_samples < src_offset + nb_samples)
            return -EINVAL;
        return av_samples_copy(dst->extended_data, m_d->extended_data, dst_offset, src_offset, nb_samples, get_channels(), get_format());
    }
    int     copy_from(AVFrame *src, int dst_offset, int src_offset, int nb_samples)
    {
        if(!src|| src->format != get_format())
            return -EINVAL;
        if(m_d->nb_samples < dst_offset + nb_samples
        || src->nb_samples < src_offset + nb_samples)
            return -EINVAL;
        return av_samples_copy(m_d->extended_data, src->extended_data, dst_offset, src_offset, nb_samples, get_channels(), get_format());
    }
    int     copy_to(uint8_t **dst, int dst_offset, int src_offset, int nb_samples)
    {
        if(!dst)
            return -EINVAL;
        if(m_d->nb_samples < src_offset + nb_samples)
            return -EINVAL;
        return av_samples_copy(dst, m_d->extended_data, dst_offset, src_offset, nb_samples, get_channels(), get_format());
    }
    int     shift_samples(int nb_samples)
    {
        if(!writable())
            make_writable();
        nb_samples = std::min(nb_samples, get_samples());
        if( av_samples_copy(m_d->extended_data, m_d->extended_data, 0, nb_samples, get_samples() - nb_samples,  get_channels(),get_format()))
            return -1;
        m_d->nb_samples -= nb_samples;
        return nb_samples;
    }

};


inline void swap(frame_ptr &lhs, frame_ptr &rhs){ lhs.swap(rhs);}
