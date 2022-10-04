#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <ldns/packet.h>

#include "common/coro.h"
#include "common/defs.h"
#include "common/net_utils.h"
#include "dns/common/dns_defs.h"
#include "dns/common/net_consts.h"
#include "dns/common/dns_utils.h"
#include "dns/net/certificate_verifier.h"
#include "dns/net/socket.h"
#include "dns/net/tls_session_cache.h"

namespace ag {
namespace dns {

class Upstream;

using UpstreamPtr = std::shared_ptr<Upstream>;
using ldns_pkt_ptr = UniquePtr<ldns_pkt, &ldns_pkt_free>; // NOLINT(readability-identifier-naming)
using ldns_buffer_ptr = UniquePtr<ldns_buffer, &ldns_buffer_free>; // NOLINT(readability-identifier-naming)

/**
 * Upstream factory configuration
 */
struct UpstreamFactoryConfig {
    EventLoop &loop;
    SocketFactory *socket_factory = nullptr;
    bool ipv6_available = true;
};

/**
 * Options for upstream
 */
struct UpstreamOptions {
    /**
     * Server address, one of the following kinds:
     *     8.8.8.8:53 -- plain DNS
     *     tcp://8.8.8.8:53 -- plain DNS over TCP
     *     tls://1.1.1.1 -- DNS-over-TLS
     *     https://dns.adguard.com/dns-query -- DNS-over-HTTPS
     *     sdns://... -- DNS stamp (see https://dnscrypt.info/stamps-specifications)
     *     quic://dns.adguard.com:853 -- DNS-over-QUIC
     */
    std::string address;

    /**
     * List of the DNS server URLs to be used to resolve a hostname in the upstream address.
     * The URLs MUST contain the resolved server addresses, not hostnames.
     * E.g. `https://94.140.14.14` is correct, while `dns.adguard.com:53` is not.
     */
    std::vector<std::string> bootstrap;

    /** Upstream timeout. 0 means "default". */
    Millis timeout;

    /** Upstream's IP address. If specified, the bootstrapper is NOT used */
    IpAddress resolved_server_ip;

    /** User-provided ID for this upstream */
    int32_t id;

    /** (Optional) name or index of the network interface to route traffic through */
    IfIdVariant outbound_interface;

    /** If set to true, an outbound proxy won't be used for the upstream's network connections */
    bool ignore_proxy_settings; // @todo: expose this flag in the public API if it's needed
};

/**
 * Upstream is interface for handling DNS requests to upstream servers
 */
class Upstream : public std::enable_shared_from_this<Upstream> {
public:
    static constexpr Millis DEFAULT_TIMEOUT{5000};

    enum class InitError {
        AE_EMPTY_SERVER_NAME,
        AE_EMPTY_BOOTSTRAP,
        AE_BOOTSTRAPPER_INIT_FAILED,
        AE_INVALID_ADDRESS,
        AE_SSL_CONTEXT_INIT_FAILED,
        AE_CURL_HEADERS_INIT_FAILED,
        AE_CURL_POOL_INIT_FAILED,
    };

    Upstream(UpstreamOptions opts, const UpstreamFactoryConfig &config)
            : m_options(std::move(opts)), m_config(config) {
        m_rtt = Millis::zero();
        if (!m_options.timeout.count()) {
            m_options.timeout = DEFAULT_TIMEOUT;
        }
    }

    virtual ~Upstream() = default;

    /**
     * Initialize upstream
     * @return Error in case of error, nullptr otherwise
     */
    virtual Error<InitError> init() = 0;

    using ExchangeResult = Result<ldns_pkt_ptr, DnsError>;

    /**
     * Do DNS exchange, considering that `request` may be a forwarded request.
     * @param request DNS request message
     * @param info (optional) out of band info about the forwarded DNS request message
     * @return DNS response message or an error
     */
    virtual coro::Task<ExchangeResult> exchange(ldns_pkt *request, const DnsMessageInfo *info = nullptr) = 0;

    [[nodiscard]] const UpstreamOptions &options() const { return m_options; }

    [[nodiscard]] const UpstreamFactoryConfig &config() const { return m_config; }

    /**
     * Helper function for easier socket creation
     */
    [[nodiscard]] SocketFactory::SocketPtr make_socket(utils::TransportProtocol proto) const {
        return m_config.socket_factory->make_socket(
                {proto, m_options.outbound_interface, m_options.ignore_proxy_settings});
    }

    [[nodiscard]] SocketFactory::SocketPtr make_secured_socket(utils::TransportProtocol proto,
                                                               SocketFactory::SecureSocketParameters secure_socket_parameters) const {
        return m_config.socket_factory->make_secured_socket(
                {proto, m_options.outbound_interface, m_options.ignore_proxy_settings},
                std::move(secure_socket_parameters));
    }

    Millis rtt() {
        std::lock_guard<std::mutex> lk(m_rtt_guard);
        return m_rtt;
    }

    /**
     * Update RTT
     * @param elapsed spent time in exchange()
     */
    void adjust_rtt(Millis elapsed) {
        std::lock_guard<std::mutex> lk(m_rtt_guard);
        m_rtt = (m_rtt + elapsed) / 2;
    }

protected:
    /** Upstream options */
    UpstreamOptions m_options;
    /** Upstream factory configuration */
    UpstreamFactoryConfig m_config;
    /** RTT + mutex */
    Millis m_rtt;
    std::mutex m_rtt_guard;
};

/**
 * Upstream factory entity which produces upstreams
 */
class UpstreamFactory {
public:
    enum class UpstreamCreateError {
        AE_INVALID_URL,
        AE_INVALID_STAMP,
        AE_INIT_FAILED,
    };
    using CreateResult = Result<UpstreamPtr, UpstreamCreateError>;

    explicit UpstreamFactory(UpstreamFactoryConfig cfg);

    ~UpstreamFactory();

    /**
     * Create an upstream
     * @param opts upstream settings
     * @return Creation result
     */
    CreateResult create_upstream(const UpstreamOptions &opts) const;

    struct Impl;
private:
    std::unique_ptr<Impl> m_factory;
};

} // namespace dns

// clang format off
template<>
struct ErrorCodeToString<dns::UpstreamFactory::UpstreamCreateError> {
    std::string operator()(dns::UpstreamFactory::UpstreamCreateError e) {
        switch (e) {
        case decltype(e)::AE_INVALID_URL: return "Invalid URL";
        case decltype(e)::AE_INVALID_STAMP: return "Invalid DNS stamp";
        case decltype(e)::AE_INIT_FAILED: return "Error initializing upstream";
        }
    }
};

template<>
struct ErrorCodeToString<dns::Upstream::InitError> {
    std::string operator()(dns::Upstream::InitError e) {
        switch (e) {
        case decltype(e)::AE_EMPTY_SERVER_NAME: return "Server name is empty";
        case decltype(e)::AE_EMPTY_BOOTSTRAP: return "Bootstrap should not be empty when server IP address is not known";
        case decltype(e)::AE_BOOTSTRAPPER_INIT_FAILED: return "Failed to create bootstrapper";
        case decltype(e)::AE_INVALID_ADDRESS: return "Passed server address is not valid";
        case decltype(e)::AE_SSL_CONTEXT_INIT_FAILED: return "Failed to initialize SSL context";
        case decltype(e)::AE_CURL_HEADERS_INIT_FAILED: return "Failed to initialize CURL headers";
        case decltype(e)::AE_CURL_POOL_INIT_FAILED: return "Failed to initialize CURL connection pool";
        }
    }
};
// clang format on

} // namespace ag
