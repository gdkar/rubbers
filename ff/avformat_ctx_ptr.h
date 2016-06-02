_Pragma("once")

#include "ffcommon.h"

struct avformat_ctx_ptr {
    AVFormatContext     *m_d{nullptr};
    avformat_ctx_ptr() = default;
    avformat_ctx_ptr(AVFormatContext *f)
        : m_d(f) { }
    avformat_ctx_ptr(avformat_ctx_ptr &&o)
        : m_d(o.release()) { }
    avformat_ctx_ptr(const avformat_ctx_ptr &o) = delete;
    avformat_ctx_ptr &operator = (AVFormatContext *f)
    {
        reset(f);
        return *this;
    }
    avformat_ctx_ptr &operator = (const AVFormatContext *f) = delete;
        avformat_ctx_ptr &operator =(avformat_ctx_ptr &&o)
    {
        reset(o.release());
        return *this;
    }
    avformat_ctx_ptr &operator = (const avformat_ctx_ptr &o) = delete;
    void swap(avformat_ctx_ptr &o) { std::swap(m_d,o.m_d);}
    void reset(AVFormatContext *f = nullptr)
    {
        if(f != m_d)
            close();
        m_d = f;
    }
    AVFormatContext *release() { auto ret = m_d; m_d = nullptr; return ret;}
    AVFormatContext *get() const { return m_d;}
   ~avformat_ctx_ptr() { close();}
    AVFormatContext *operator ->() { return m_d; }
    AVFormatContext *operator ->() const { return m_d;}
    AVFormatContext &operator *() { return *m_d;}
    const AVFormatContext &operator *() const { return *m_d;}
    int open_input(const char *filename, AVInputFormat *fmt = nullptr, AVDictionary **opts = nullptr) { return avformat_open_input(&m_d, filename, fmt, opts);}
    void close() { avformat_close_input(&m_d);}
    operator bool() const { return !!m_d;}
    operator AVFormatContext *() const { return m_d;}
    void alloc() { if(!m_d) m_d = avformat_alloc_context();}
    void opt_set_int(const char *name, int val){ av_opt_set_int(m_d, name, val,0);}
    void opt_set_sample_fmt(const char *name, AVSampleFormat val){ av_opt_set_sample_fmt(m_d, name, val,0);}
    void opt_set_channel_layout(const char *name, int64_t val){ av_opt_set_channel_layout(m_d, name, val,0);}
    void opt_set(const char *name, int val) { opt_set_int(name,val);}
    void opt_set(const char *name, AVSampleFormat val) { opt_set_sample_fmt(name,val);}
    void opt_set(const char *name, int64_t val) { opt_set_channel_layout(name, val);}

    int read_frame(AVPacket *pkt) { return av_read_frame(m_d, pkt);}
    int seek_frame(int sid, int64_t ts, int flags = 0)
    {
        return av_seek_frame(m_d, sid, ts,flags);
    }
    int seek_file(int sid, int64_t min_ts, int64_t ts, int64_t max_ts, int flags = 0)
    {
        return avformat_seek_file(m_d,sid,min_ts,ts,max_ts,flags);
    }
    int flush() { return avformat_flush(m_d);}
    int find_stream_info(AVDictionary **opts) { return avformat_find_stream_info(m_d, opts);}
    int find_best_stream(AVMediaType type, int desired, int related, AVCodec **codec, int flags = 0)
    {
        return av_find_best_stream(m_d,type,desired,related,codec,flags);
    }
    int read_play() { return av_read_play(m_d);}
    int read_pause() { return av_read_pause(m_d);}
    void dump(int offset, const char *filename, bool is_out) const
    {
        av_dump_format(m_d, offset, filename, is_out);
    }
};
inline void swap(avformat_ctx_ptr &lhs, avformat_ctx_ptr &rhs){ lhs.swap(rhs);}
