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

struct packet_ptr {
    AVPacket     *m_d{nullptr};
    packet_ptr() = default;
    packet_ptr(AVPacket *f)
        : m_d(f) { }
    packet_ptr(packet_ptr &&o)
        : m_d(o.release()) { }
    packet_ptr(const packet_ptr &o)
        : m_d(av_packet_clone(o)) { }
    packet_ptr &operator = (AVPacket *f)
    {
        reset(f);
        return *this;
    }
    packet_ptr &operator =(packet_ptr &&o)
    {
        if(m_d) {
            ref(o);
        }else{
            reset(o.release());
        }
        return *this;
    }
    packet_ptr &operator = (const packet_ptr &o)
    {
        reset(av_packet_clone(o.m_d));
        return *this;
    }
    void swap(packet_ptr &o) { std::swap(m_d,o.m_d);}
    void reset(AVPacket *f = nullptr) { av_packet_free(&m_d); m_d = f;}
    AVPacket *release() { auto ret = m_d; m_d = nullptr; return ret;}
    AVPacket *get() const { return m_d;}
   ~packet_ptr() { reset();}
    AVPacket *operator ->() { return m_d; }
    AVPacket *operator ->() const { return m_d;}
    AVPacket &operator *() { return *m_d;}
    const AVPacket &operator *() const { return *m_d;}
    operator bool() const { return !!m_d;}
    operator AVPacket *() const { return m_d;}
    void alloc() { if(!m_d) m_d = av_packet_alloc(); else unref();}
    void unref() { av_packet_unref(m_d); }
    void ref(packet_ptr &o) { if(o.m_d != m_d) { unref(); av_packet_ref(m_d,o.m_d); } }
};

inline void swap(packet_ptr &lhs, packet_ptr &rhs){ lhs.swap(rhs);}
