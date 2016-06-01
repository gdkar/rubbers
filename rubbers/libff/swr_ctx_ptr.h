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

struct swr_ctx_ptr {
    SwrContext     *m_d{nullptr};
    swr_ctx_ptr() = default;
    swr_ctx_ptr(SwrContext *f)
        : m_d(f) { }
    swr_ctx_ptr(swr_ctx_ptr &&o)
        : m_d(o.release()) { }
    swr_ctx_ptr(const swr_ctx_ptr &o) = delete;
    swr_ctx_ptr &operator = (SwrContext *f)
    {
        reset(f);
        return *this;
    }
    swr_ctx_ptr &operator = (const SwrContext *f) = delete;
        swr_ctx_ptr &operator =(swr_ctx_ptr &&o)
    {
        reset(o.release());
        return *this;
    }
    swr_ctx_ptr &operator = (const swr_ctx_ptr &o) = delete;
    void swap(swr_ctx_ptr &o) { std::swap(m_d,o.m_d);}
    void reset(SwrContext *f = nullptr) { swr_free(&m_d); m_d = f;}
    SwrContext *release() { auto ret = m_d; m_d = nullptr; return ret;}
    SwrContext *get() const { return m_d;}
   ~swr_ctx_ptr() { reset();}
    SwrContext *operator ->() { return m_d; }
    SwrContext *operator ->() const { return m_d;}
    SwrContext &operator *() { return *m_d;}
    const SwrContext &operator *() const { return *m_d;}
    bool initialized() const { return swr_is_initialized(m_d);}
    bool init() { return m_d && !swr_init(m_d);}
    void close() { swr_close(m_d);}
    bool config(AVFrame *dst, AVFrame *src) { return !swr_config_frame(m_d,dst,src);}
    bool convert(AVFrame *dst, AVFrame *src) { return !swr_convert_frame(m_d,dst,src);}
    int64_t delay(int64_t base) const { return swr_get_delay(m_d,base);}
    operator bool() const { return !!m_d;}
    operator SwrContext *() const { return m_d;}
    void alloc() { if(!m_d) m_d = swr_alloc();}
    void opt_set_int(const char *name, int val){ av_opt_set_int(m_d, name, val,0);}
    void opt_set_sample_fmt(const char *name, AVSampleFormat val){ av_opt_set_sample_fmt(m_d, name, val,0);}
    void opt_set_channel_layout(const char *name, int64_t val){ av_opt_set_channel_layout(m_d, name, val,0);}
    void opt_set(const char *name, int val) { opt_set_int(name,val);}
    void opt_set(const char *name, AVSampleFormat val) { opt_set_sample_fmt(name,val);}
    void opt_set(const char *name, int64_t val) { opt_set_channel_layout(name, val);}
    int  drop_output(int samples) { return swr_drop_output(m_d, samples);}
    int inject_silence(int samples) { return swr_inject_silence(m_d,samples);};
    int64_t next_pts(int64_t pts) { return swr_next_pts(m_d,pts);}
};

inline void swap(swr_ctx_ptr &lhs, swr_ctx_ptr &rhs){ lhs.swap(rhs);}
