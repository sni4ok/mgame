/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "imports.hpp"
#include "dlfcn.hpp"
#include "mmap.hpp"

#include "../evie/socket.hpp"
#include "../evie/fmap.hpp"
#include "../evie/utils.hpp"
#include "../evie/thread.hpp"
#include "../evie/profiler.hpp"
#include "../evie/optional.hpp"
#include "../evie/mlog.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <poll.h>

#include <cerrno>

pair<void*, str_holder> import_context_create(void* params);
void import_context_destroy(pair<void*, str_holder> ctx);
bool import_proceed_data(str_holder& buf, void* ctx);

template<typename reader_state, optional<u32> (*read)(reader_state socket, char_it buf, u32 buf_size)>
struct reader
{
    reader_state socket;
    pair<void*, str_holder> ctx;
    time_t recv_time;

    bool proceed(bool& r)
    {
        optional<u32> readed = read(socket, const_cast<char_it>(ctx.second.begin()),
            ctx.second.size());

        if(!readed) [[unlikely]]
        {
            r = false;
            return true;
        }

        if(!*readed) [[unlikely]]
            return false;

        ctx.second.resize(*readed);
        bool ret = import_proceed_data(ctx.second, ctx.first);
        recv_time = time(NULL);
        r = true;
        return ret;
    }
    reader(void* ctx_params, reader_state socket) : socket(socket),
        ctx(import_context_create(ctx_params)), recv_time(time(NULL))
    {
    }
    ~reader()
    {
        import_context_destroy(ctx);
    }
    reader(const reader&) = delete;
};

optional<u32> tcp_read(int socket, char_it buf, u32 buf_size)
{
    int ret = ::recv(socket, buf, buf_size, MSG_DONTWAIT);
    return socket_result(ret, "tcp_read");
}

optional<u32> udp_read(int socket, char_it buf, u32 buf_size)
{
    int ret = recvfrom(socket, buf, buf_size, MSG_DONTWAIT, nullptr, nullptr);
    return socket_result(ret, "udp_read");
}

optional<u32> pipe_read(int hfile, char_it buf, u32 buf_size)
{
    u32 read_bytes = ::read(hfile, buf, buf_size);
    if(!read_bytes) [[unlikely]]
        throw_system_failure("read() from pipe error");
    return {read_bytes};
}

optional<u32> mmap_cp_read(void *v, char_it buf, u32 buf_size)
{
    u8* f = (u8*)v, *e = f + message_size, *i = f;
    u8 w = mmap_load(f);
    u8 r = mmap_load(f + 1);

    if(w < 2 || w >= message_size || r < 2 || r >= message_size) [[unlikely]]
        throw mexception(es() % "mmap_cp_read, internal error, wp: "
            % u32(w) % ", rp: " % u32(r));

    i += r;
    const message* p = (((const message*)v) + 1) + (r - 2) * 255;
    u8 cur_count = mmap_load(i);

    if(cur_count)
    {
        if(buf_size < cur_count) [[unlikely]]
            throw mexception(es() % "mmap_cp_read, buf_size too small, wp: " % u32(w) %
                ", rp: " % u32(r) % ", buf_size: " % buf_size % ", cur_count: "
                % cur_count);

        memcpy(buf, p, u32(cur_count) * message_size);
        mmap_store(i, 0);
        ++i;
        if(i == e)
            i = f + 2;
        *(f + 1) = u8(i - f);
    }
    return {cur_count * message_size};
}

struct import_mmap_cp
{
    volatile bool& can_run;
    mvector<mstring> params;
    bool pooling_mode;

    import_mmap_cp(volatile bool& can_run, const mstring& p) : can_run(can_run)
    {
        params = split_s(p.str(), ' ');
        if(params.size() != 2)
            throw mexception(es()
                % "import_mmap_cp, required 2 params (fname,pooling_mode): " % p);

        pooling_mode = lexical_cast<bool>(params[1]);
    }
    unique_ptr<void, mmap_close> init() const
    {
        void* p = mmap_create(params[0].c_str(), true);
        unique_ptr<void, &mmap_close> ptr(p);
        shared_memory_sync* s = get_smc(p);
        mmap_store(&s->pooling_mode, pooling_mode ? 1 : 2);

        u8* w = (u8*)p, *r = w + 1;
        bool initialized = false;

        pthread_lock lock(s->mutex);
        while(!initialized && can_run)
        {
            u8 wc = mmap_load(w);
            u8 rc = mmap_load(r);

            if((rc != 1 && rc != 0) || wc == 1)
            {
                mmap_store(w, 0);
                throw mexception(es() % "mmap inconsistence, wp: " % wc % ", rp: " % rc);
            }

            initialized = (wc == 2 && rc == 1);
            if(!initialized)
                pthread_timedwait(s->condition, s->mutex, 1);
        }
        *r = 2;
        pthread_cond_signal(&(s->condition));

        if(initialized)
            mlog() << "receiving data from " << params[0] << " started";

        return ptr;
    }
};

void import_mmap_cp_start(void* imc, void* params)
{
    import_mmap_cp& ic = *((import_mmap_cp*)(imc));
    auto ptr = ic.init();
    void* p = ptr.get();
    reader<void*, mmap_cp_read> rr(params, p);
    shared_memory_sync* s = get_smc(p);
    u8* w = (u8*)p, *r = w + 1;
    bool readed;

    while(ic.can_run)
    {
        if(!rr.proceed(readed) && !ic.pooling_mode && !pthread_mutex_trylock(&(s->mutex)))
        {
            u8 rc = *r;
            if(rc < 2)
            {
                pthread_mutex_unlock(&(s->mutex));
                throw mexception(es() % "mmap inconsistence 2, rp: " % rc);
            }

            u8 count = mmap_load(w + rc);
            if(!count)
                pthread_timedwait(s->condition, s->mutex, 1);

            pthread_mutex_unlock(&(s->mutex));
        }
    }
}

struct import_pipe
{
    volatile bool& can_run;
    mstring params;

    import_pipe(volatile bool& can_run, const mstring& params)
        : can_run(can_run), params(params)
    {
    }
};

template<typename reader>
void work_thread_reader(reader& r, volatile bool& can_run, i32 timeout/*in seconds*/)
{
    pollfd pfd = pollfd();
    pfd.events = POLLIN;
    pfd.fd = r.socket;

    while(can_run)
    {
        int ret = poll(&pfd, 1, 50);
        if(ret < 0)
            throw_system_failure("poll() error");

        if(!ret)
        {
            if(time(NULL) > r.recv_time + timeout)
                throw str_exception("feed timeout");

            continue;
        }

        bool readed;
        do
        {
            if(!r.proceed(readed))
                break;
        }
        while(readed && can_run);
    }
}

static const u32 max_connections = 32;
static const i32 timeout = 30; //in seconds

void import_pipe_start(void* c, void* p)
{
    import_pipe& ip = *((import_pipe*)(c));
    mkfifo(ip.params.c_str(), 0666);

    i64 h = open(ip.params.c_str(), O_RDONLY | O_NONBLOCK);
    if(h <= 0)
        throw_system_failure(es() % "open " % ip.params % " error");

    mlog() << "import from " << ip.params << " started";
    socket_holder ss(h);
    reader<int, pipe_read> r(p, h);
    work_thread_reader(r, ip.can_run, timeout);
}

struct import_tcp_server
{
    volatile bool& can_run;
    mstring params;
    u16 port;
    u32 count;
    ::mutex mutex;
    condition cond;

    import_tcp_server(volatile bool& can_run, const mstring& params)
        : can_run(can_run), params(params),
        port(lexical_cast<u16>(params)), count()
    {
    }
    ~import_tcp_server()
    {
        scoped_lock lock(mutex);
        while(count)
            cond.timed_uwait(lock, 50 * 1000);
    }
};

void import_tcp_thread(import_tcp_server* it, int socket, mstring client,
    volatile bool& initialized, void* p)
{
    try
    {
        socket_holder ss(socket);
        scoped_lock lock(it->mutex);
        ++(it->count);
        initialized = true;

        if(it->count == max_connections)
            mlog(mlog::warning) << "import|tcp_server("
                << it->params << ") max_connections exceed on client " << client;

        if(it->count > max_connections)
            throw str_exception("limit connections exceed");

        it->cond.notify_all();
        lock.unlock();
        mlog() << "import|tcp_server thread for " << client << " started";
        reader<int, tcp_read> r(p, socket);
        work_thread_reader(r, it->can_run, timeout);
    }
    catch(exception& e)
    {
        mlog(mlog::error) << "import|tcp_server(" << it->params << ") client " << client << " " << e;
    }

    scoped_lock lock(it->mutex);
    it->cond.notify_all();
    mlog() << "import|tcp_server(" << it->params << ") thread for " << client << " ended";
    --(it->count);
}

void import_tcp_start(void* c, void* p)
{
    import_tcp_server& it = *((import_tcp_server*)(c));

    while(it.can_run)
    {
        mstring client;
        scoped_lock lock(it.mutex);
        while(it.can_run && it.count >= max_connections)
            it.cond.timed_uwait(lock, 50 * 1000);

        lock.unlock();
        int socket = socket_accept(it.port, false/*local*/,
            &client, &it.can_run, it.params.c_str());

        volatile bool initialized = false;
        thread thrd(&import_tcp_thread, &it, socket, client, ref(initialized), p);
        lock.lock();

        while(!initialized)
            it.cond.timed_uwait(lock, 50 * 1000);

        thrd.detach();
    }
}

struct import_tcp_client
{
    volatile bool& can_run;
    mstring params;

    import_tcp_client(volatile bool& can_run, const mstring& params)
        : can_run(can_run), params(params)
    {
    }
};

void import_tcp_client_start(void* c, void* p)
{
    import_tcp_client& tc = *((import_tcp_client*)c);
    int socket = socket_connect("import|tcp_client", tc.params.str(), 3, true);
    socket_holder ss(socket);
    reader<int, tcp_read> r(p, socket);
    work_thread_reader(r, tc.can_run, timeout);
}

struct import_udp
{
    volatile bool& can_run;
    mstring host, src_ip, ma;
    u16 port;

    import_udp(volatile bool& can_run, const mstring& params) : can_run(can_run)
    {
        auto p = split(params.str(), ' ');
        if(p.size() != 2 && p.size() != 4)
            throw mexception(es() % "import_udp, required 3 params (src_ip multiaddr port): "
                % params);

        host = p[0];
        port = lexical_cast<u16>(p[1]);

        if(p.size() == 4)
        {
            src_ip = p[2];
            ma = p[3];
        }
        mlog() << "import_udp, " << params << " started";
    }
};

void import_udp_start(void* c, void* p)
{
    import_udp& i = *((import_udp*)(c));
    udp_socket udp(i.host, i.port, i.src_ip, i.ma);
    reader<int, udp_read> r(p, udp.socket);
    work_thread_reader(r, i.can_run, timeout);
}

template<void* (*fcreate)(const char* params,
    volatile bool& can_run), void (*fdestroy)(void *v)>
struct import_ifile_impl
{
    void* ptr;
    volatile bool& can_run;

    import_ifile_impl(volatile bool& can_run, const mstring& params) : can_run(can_run)
    {
        ptr = fcreate(params.c_str(), can_run);
    }
    ~import_ifile_impl()
    {
        fdestroy(ptr);
    }
};

typedef import_ifile_impl<ifile_create, ifile_destroy> import_ifile;
typedef import_ifile_impl<files_replay_create, files_replay_destroy> import_files_replay;

template<typename fimport, optional<u32> (*fread)(void *v, char* buf, u32 buf_size)>
void import_ifile_start(void* c, void* p)
{
    fimport& f = *((fimport*)(c));
    reader<void*, fread> r(p, f.ptr);
    bool readed;

    while(f.can_run)
    {
        bool ret = r.proceed(readed);
        if(!ret) [[unlikely]]
            return;
    }
}

template<typename type>
void* importer_init(volatile bool& can_run, char_cit params)
{
    return (void*)(new type(can_run, _str_holder(params)));
}

template<typename type>
void importer_destroy(void* ptr)
{
    delete (type*)ptr;
}

struct importers
{
    fmap<mstring, hole_importer> i;
    ::mutex m;
    mvector<fcn_type> f;
};

importers _importers;

int register_importer(char_cit s, hole_importer hi)
{
    mstring name(_str_holder(s));
    mutex::scoped_lock lock(_importers.m);
    auto it = _importers.i.find(name);
    if(it != _importers.i.end())
        throw mexception(es() % "register_importer, double registration for " % name);

    _importers.i[name] = hi;
    return _importers.i.size();
}

static const int _import_mmap_cp = register_importer("mmap_cp",
    {importer_init<import_mmap_cp>, importer_destroy<import_mmap_cp>,
    import_mmap_cp_start, nullptr}
);
static const int _import_pipe = register_importer("pipe",
    {importer_init<import_pipe>, importer_destroy<import_pipe>, import_pipe_start, nullptr}
);
static const int _import_tcp_server = register_importer("tcp_server",
    {importer_init<import_tcp_server>, importer_destroy<import_tcp_server>, import_tcp_start, nullptr}
);
static const int _import_tcp_client = register_importer("tcp_client",
    {importer_init<import_tcp_client>, importer_destroy<import_tcp_client>, import_tcp_client_start, nullptr}
);
static const int _import_udp = register_importer("udp",
    {importer_init<import_udp>, importer_destroy<import_udp>, import_udp_start, nullptr}
);

optional<u32> __ifile_read(void *v, char* buf, u32 buf_size)
{
    return {ifile_read(v, buf, buf_size)};
}

static const int _import_file = register_importer("file",
    {importer_init<import_ifile>, importer_destroy<import_ifile>,
    import_ifile_start<import_ifile, __ifile_read>, nullptr}
);

optional<u32> __files_replay_read(void *v, char* buf, u32 buf_size)
{
    return {files_replay_read(v, buf, buf_size)};
}

static const int _import_files_replay = register_importer("files_replay",
    {importer_init<import_files_replay>, importer_destroy<import_files_replay>,
    import_ifile_start<import_files_replay, __files_replay_read>, nullptr}
);

hole_importer load_importer(const mstring& lib)
{
    typedef void (create_import)(hole_importer* i, simple_log* sl);
    auto f = lib_load<create_import>(lib, "create_import");
    hole_importer hi;
    f.second(&hi, log_get());
    _importers.i[lib] = hi;
    _importers.f.push_back(move(f.first));
    return hi;
}

hole_importer create_importer(char_cit s)
{
    mstring name(_str_holder(s));
    mutex::scoped_lock lock(_importers.m);
    auto it = _importers.i.find(name);
    if(it != _importers.i.end())
        return it->second;
    return load_importer(name);
}

