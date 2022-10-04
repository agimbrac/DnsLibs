#include "proxied_socket.h"

#define log_sock(s_, lvl_, fmt_, ...) lvl_##log((s_)->m_log, "[id={}] " fmt_, (s_)->m_id, ##__VA_ARGS__)

namespace ag::dns {

ProxiedSocket::ProxiedSocket(Parameters p)
        : Socket(__func__, std::move(p.socket_parameters), p.prepare_fd)
        , m_proxy(&p.outbound_proxy)
        , m_proxied_callbacks(p.callbacks) {
}

ProxiedSocket::~ProxiedSocket() {
    m_fallback_info.reset();
    if (m_proxy_id.has_value()) {
        m_proxy->close_connection(m_proxy_id.value());
    }
}

std::optional<evutil_socket_t> ProxiedSocket::get_fd() const {
    return !m_proxy_id.has_value() ? std::nullopt : m_proxy->get_fd(m_proxy_id.value());
}

Error<SocketError> ProxiedSocket::connect(ConnectParameters params) {
    log_sock(this, trace, "{}", params.peer.str());

    if (auto err = this->set_callbacks(params.callbacks)) {
        log_sock(this, dbg, "Failed to set callbacks: {}", err->str());
        assert(0);
        return err;
    }

    auto r = m_proxy->connect({
            .loop = params.loop,
            .proto = this->get_protocol(),
            .peer = params.peer,
            .callbacks = {on_successful_proxy_connection, on_proxy_connection_failed, on_connected, on_read, on_close, this},
            .timeout = params.timeout,
            .outbound_interface = m_parameters.outbound_interface,
    });
    if (r.has_error()) {
        return r.error();
    }

    m_proxy_id = r.value();

    m_fallback_info = std::make_unique<struct FallbackInfo>();
    m_fallback_info->loop = params.loop;
    m_fallback_info->peer = params.peer;
    m_fallback_info->connect_timestamp = SteadyClock::now();
    m_fallback_info->timeout = params.timeout;

    return {};
}

Error<SocketError> ProxiedSocket::send(Uint8View data) {
    log_sock(this, trace, "{}", data.size());
    return m_proxy->send(m_proxy_id.value(), data);
}

bool ProxiedSocket::set_timeout(Micros timeout) {
    log_sock(this, trace, "{}", timeout);
    if (m_fallback_info != nullptr) {
        m_fallback_info->timeout = timeout;
    }
    return m_proxy->set_timeout(m_proxy_id.value(), timeout);
}

Error<SocketError> ProxiedSocket::set_callbacks(Socket::Callbacks cbx) {
    log_sock(this, trace, "...");

    Error<SocketError> err;

    m_socket_callbacks.mtx.lock();
    m_socket_callbacks.val = cbx;
    m_socket_callbacks.mtx.unlock();

    if (m_proxy_id.has_value()) {
        err = m_proxy->set_callbacks(m_proxy_id.value(),
                {
                        on_successful_proxy_connection,
                        on_proxy_connection_failed,
                        (cbx.on_connected != nullptr) ? on_connected : nullptr,
                        (cbx.on_read != nullptr) ? on_read : nullptr,
                        (cbx.on_close != nullptr) ? on_close : nullptr,
                        this,
                });
    }

    return err;
}

struct Socket::Callbacks ProxiedSocket::get_callbacks() {
    std::scoped_lock l(m_socket_callbacks.mtx);
    return m_socket_callbacks.val;
}

void ProxiedSocket::on_successful_proxy_connection(void *arg) {
    auto *self = (ProxiedSocket *) arg;
    self->m_proxied_callbacks.on_successful_proxy_connection(self->m_proxied_callbacks.arg);
}

void ProxiedSocket::on_proxy_connection_failed(void *arg, Error<SocketError> err) {
    auto *self = (ProxiedSocket *) arg;

    ProxyConnectionFailedResult r
            = self->m_proxied_callbacks.on_proxy_connection_failed(self->m_proxied_callbacks.arg, std::move(err));
    if (std::holds_alternative<CloseConnection>(r)) {
        return;
    }

    self->m_fallback_info->proxy = std::get<Fallback>(r).proxy;
}

void ProxiedSocket::on_connected(void *arg, uint32_t) {
    auto *self = (ProxiedSocket *) arg;
    log_sock(self, trace, "...");
    self->m_fallback_info.reset();
    if (Socket::Callbacks cbx = self->get_callbacks(); cbx.on_connected != nullptr) {
        cbx.on_connected(cbx.arg);
    }
}

void ProxiedSocket::on_read(void *arg, Uint8View data) {
    auto *self = (ProxiedSocket *) arg;
    log_sock(self, trace, "{}", data.size());
    if (Socket::Callbacks cbx = self->get_callbacks(); cbx.on_read != nullptr) {
        cbx.on_read(cbx.arg, data);
    }
}

void ProxiedSocket::on_close(void *arg, Error<SocketError> error) {
    auto *self = (ProxiedSocket *) arg;
    if (error) {
        log_sock(self, dbg, "{}", error->str());
    }

    if (std::unique_ptr info = std::move(self->m_fallback_info); info != nullptr && info->proxy != nullptr) {
        log_sock(self, dbg, "Falling back to direct connection");
        self->m_proxy->close_connection(std::exchange(self->m_proxy_id, std::nullopt).value());
        self->m_proxy = info->proxy;
        Micros elapsed = std::chrono::duration_cast<Micros>(SteadyClock::now() - info->connect_timestamp);
        self->m_socket_callbacks.mtx.lock();
        Socket::Callbacks socket_callbacks = self->m_socket_callbacks.val;
        self->m_socket_callbacks.mtx.unlock();
        error = self->connect({
                info->loop,
                info->peer,
                socket_callbacks,
                info->timeout.has_value()
                        ? std::make_optional<Micros>(std::max(Micros(0), info->timeout.value() - elapsed))
                        : std::nullopt,
        });
        if (!error) {
            return;
        }
        log_sock(self, dbg, "Failed to fall back");
    }

    if (Socket::Callbacks cbx = self->get_callbacks(); cbx.on_close != nullptr) {
        cbx.on_close(cbx.arg, std::move(error));
    }
}

} // namespace ag::dns
