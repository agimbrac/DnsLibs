package com.adguard.dnslibs.proxy;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class DnsProxySettings {
    /**
     * Specifies how to respond to filtered requests
     */
    public enum BlockingMode {
        // MUST keep names and ordinals in sync with ag::blocking_mode

        /** AdBlock-style filters -> REFUSED, hosts-style filters -> rule-specified or unspecified address */
        DEFAULT(0),

        /** Always return REFUSED */
        REFUSED(4),

        /** Always return NXDOMAIN */
        NXDOMAIN(1),

        /** Always return unspecified address */
        UNSPECIFIED_ADDRESS(2),

        /** Always return custom configured IP address (See {@link DnsProxySettings}) */
        CUSTOM_ADDRESS(3)

        ;

        private final int code;
        BlockingMode(int code) { this.code = code; }

        public int getCode() { return code; }

        public static BlockingMode fromCode(int code) {
            for (final BlockingMode m : values()) {
                if (m.code == code) {
                    return m;
                }
            }
            return DEFAULT;
        }
    }

    private List<UpstreamSettings> upstreams = new ArrayList<>();
    private List<UpstreamSettings> fallbacks = new ArrayList<>();
    private List<String> fallbackDomains = new ArrayList<>();
    private boolean detectSearchDomains;
    private Dns64Settings dns64;
    private long blockedResponseTtlSecs;
    private List<FilterParams> filterParams = new ArrayList<>();
    private List<ListenerSettings> listeners = new ArrayList<>();
    private OutboundProxySettings outboundProxy;
    private boolean ipv6Available;
    private boolean blockIpv6;
    private BlockingMode blockingMode;
    private String customBlockingIpv4;
    private String customBlockingIpv6;
    private long dnsCacheSize;
    private boolean optimisticCache;
    private boolean enableDNSSECOK;
    private boolean enableRetransmissionHandling;

    /**
     * @return Maximum number of cached responses
     */
    public long getDnsCacheSize() {
        return dnsCacheSize;
    }

    /**
     * @param dnsCacheSize Maximum number of cached responses
     */
    public void setDnsCacheSize(long dnsCacheSize) {
        this.dnsCacheSize = dnsCacheSize;
    }

    /**
     * @return Custom IPv4 address to return for filtered requests
     */
    public String getCustomBlockingIpv4() {
        return customBlockingIpv4;
    }

    /**
     * @param customBlockingIpv4 Custom IPv4 address to return for filtered requests,
     *                           must be either empty/{@code null}, or a valid IPv4 address;
     *                           ignored if {@link #getBlockingMode()} != {@link BlockingMode#CUSTOM_ADDRESS}
     */
    public void setCustomBlockingIpv4(String customBlockingIpv4) {
        this.customBlockingIpv4 = customBlockingIpv4;
    }

    /**
     * @return Custom IPv6 address to return for filtered requests
     */
    public String getCustomBlockingIpv6() {
        return customBlockingIpv6;
    }

    /**
     * @param customBlockingIpv6 Custom IPv6 address to return for filtered requests,
     *                           must be either empty/{@code null}, or a valid IPv6 address;
     *                           ignored if {@link #getBlockingMode()} != {@link BlockingMode#CUSTOM_ADDRESS}
     */
    public void setCustomBlockingIpv6(String customBlockingIpv6) {
        this.customBlockingIpv6 = customBlockingIpv6;
    }

    /**
     * @return The blocking mode
     */
    public BlockingMode getBlockingMode() {
        return blockingMode;
    }

    /**
     * @param blockingMode The blocking mode
     */
    public void setBlockingMode(BlockingMode blockingMode) {
        this.blockingMode = blockingMode;
    }

    /**
     * @return DNS upstreams settings list.
     */
    public List<UpstreamSettings> getUpstreams() {
        return upstreams;
    }

    /**
     * @param upstreams DNS upstreams settings list.
     */
    public void setUpstreams(List<UpstreamSettings> upstreams) {
        this.upstreams = new ArrayList<>(upstreams);
    }

    /**
     * @return Fallback DNS upstreams settings list.
     */
    public List<UpstreamSettings> getFallbacks() {
        return fallbacks;
    }

    /**
     * @param fallbacks Fallback DNS upstreams settings list.
     */
    public void setFallbacks(List<UpstreamSettings> fallbacks) {
        this.fallbacks = new ArrayList<>(fallbacks);
    }

    /**
     * @return the fallback domains
     */
    public List<String> getFallbackDomains() {
        return fallbackDomains;
    }

    /**
     * @param fallbackDomains Requests for these domains will be forwarded directly to the
     *                        fallback upstreams, if there are any. A wildcard character, `*`,
     *                        which stands for any number of characters, is allowed to appear
     *                        multiple times anywhere except at the end of the domain.
     */
    public void setFallbackDomains(List<String> fallbackDomains) {
        this.fallbackDomains = new ArrayList<>(fallbackDomains);
    }

    /**
     * @return whether search domains detection is enabled
     */
    public boolean isDetectSearchDomains() {
        return detectSearchDomains;
    }

    /**
     * @param detectSearchDomains if true, DNS search domains will be detected
     *                            and appended to the fallback filter automatically
     */
    public void setDetectSearchDomains(boolean detectSearchDomains) {
        this.detectSearchDomains = detectSearchDomains;
    }

    /**
     * @return DNS64 settings. If {@code null}, DNS64 is disabled.
     */
    public Dns64Settings getDns64() {
        return dns64;
    }

    /**
     * @param dns64 DNS64 settings. If {@code null}, DNS64 is disabled.
     */
    public void setDns64(Dns64Settings dns64) {
        this.dns64 = dns64;
    }

    /**
     * @return TTL of the record for the blocked domains (in seconds).
     */
    public long getBlockedResponseTtlSecs() {
        return blockedResponseTtlSecs;
    }

    /**
     * @param blockedResponseTtlSecs TTL of the record for the blocked domains (in seconds).
     */
    public void setBlockedResponseTtlSecs(long blockedResponseTtlSecs) {
        this.blockedResponseTtlSecs = blockedResponseTtlSecs;
    }

    /**
     * @return Filter engine parameters.
     */
    public List<FilterParams> getFilterParams() {
        return filterParams;
    }

    /**
     * @param filterParams Filter engine parameters.
     */
    public void setFilterParams(List<FilterParams> filterParams) {
        this.filterParams = new ArrayList<>(filterParams);
    }

    /**
     * @param filterParams Filter id -> filter data.
     * @param inMemory     If true, data is rules, otherwise data is path to file with rules.
     */
    public void setFilterParams(Map<Integer, String> filterParams, boolean inMemory) {
        this.filterParams = new ArrayList<>(filterParams.size());
        for (final Map.Entry<Integer, String> e : filterParams.entrySet()) {
            getFilterParams().add(new FilterParams(e.getKey(), e.getValue(), inMemory));
        }
    }

    /**
     * @return List of addresses/ports/protocols/etc... to listen on.
     */
    public List<ListenerSettings> getListeners() {
        return listeners;
    }

    /**
     * @param listeners List of addresses/ports/protocols/etc... to listen on.
     */
    public void setListeners(List<ListenerSettings> listeners) {
        this.listeners = new ArrayList<>(listeners);
    }

    /**
     * @return Outbound proxy settings.
     */
    public OutboundProxySettings getOutboundProxy() {
        return outboundProxy;
    }

    /**
     * @param outboundProxy Outbound proxy settings.
     */
    public void setOutboundProxy(OutboundProxySettings outboundProxy) {
        this.outboundProxy = outboundProxy;
    }

    /**
     * @return whether bootstrappers will fetch AAAA records.
     */
    public boolean isIpv6Available() {
        return ipv6Available;
    }

    /**
     * @param ipv6Available if {code false}, bootstrappers will only fetch A records.
     */
    public void setIpv6Available(boolean ipv6Available) {
        this.ipv6Available = ipv6Available;
    }

    /**
     * @return whether the proxy will block AAAA requests.
     */
    public boolean isBlockIpv6() {
        return blockIpv6;
    }

    /**
     * @param blockIpv6 if {@code true}, the proxy will block AAAA requests.
     */
    public void setBlockIpv6(boolean blockIpv6) {
        this.blockIpv6 = blockIpv6;
    }

    /**
     * @return whether optimistic cache is enabled
     */
    public boolean isOptimisticCache() {
        return optimisticCache;
    }

    /**
     * @param optimisticCache enable optimistic cache
     */
    public void setOptimisticCache(boolean optimisticCache) {
        this.optimisticCache = optimisticCache;
    }

    /**
     * @return Enabled log extending for responses which processed with DNSSEC or not
     */
    public boolean isEnabledDNSSECOK() {
        return enableDNSSECOK;
    }

    /**
     * @param enableDNSSECOK Enable DNSSEC OK extension.
     *                       This options tells server that we want to receive DNSSEC records along with normal queries.
     *                       If they exist, request processed event will have DNSSEC flag on.
     *                       WARNING: may increase data usage and probability of TCP fallbacks.
     */
    public void enableDNSSECOK(boolean enableDNSSECOK) {
        this.enableDNSSECOK = enableDNSSECOK;
    }

    /**
     * @return whether retransmission handling is enabled.
     */
    public boolean isEnableRetransmissionHandling() {
        return enableRetransmissionHandling;
    }

    /**
     * @param enableRetransmissionHandling if true, retransmitted requests will be handled
     *                                     using the fallback upstreams only, the
     *                                     original request will not be answered.
     */
    public void setEnableRetransmissionHandling(boolean enableRetransmissionHandling) {
        this.enableRetransmissionHandling = enableRetransmissionHandling;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        DnsProxySettings that = (DnsProxySettings) o;
        return blockedResponseTtlSecs == that.blockedResponseTtlSecs &&
                ipv6Available == that.ipv6Available &&
                blockIpv6 == that.blockIpv6 &&
                Objects.equals(upstreams, that.upstreams) &&
                Objects.equals(fallbacks, that.fallbacks) &&
                Objects.equals(fallbackDomains, that.fallbackDomains) &&
                detectSearchDomains == that.detectSearchDomains &&
                Objects.equals(dns64, that.dns64) &&
                Objects.equals(filterParams, that.filterParams) &&
                Objects.equals(listeners, that.listeners) &&
                Objects.equals(outboundProxy, that.outboundProxy) &&
                blockingMode == that.blockingMode &&
                Objects.equals(customBlockingIpv4, that.customBlockingIpv4) &&
                Objects.equals(customBlockingIpv6, that.customBlockingIpv6) &&
                dnsCacheSize == that.dnsCacheSize &&
                optimisticCache == that.optimisticCache &&
                enableDNSSECOK == that.enableDNSSECOK &&
                enableRetransmissionHandling == that.enableRetransmissionHandling;
    }

    @Override
    public int hashCode() {
        return Objects.hash(upstreams, fallbacks, fallbackDomains, detectSearchDomains, dns64, blockedResponseTtlSecs,
                filterParams, listeners, outboundProxy, ipv6Available, blockIpv6, blockingMode, customBlockingIpv4, customBlockingIpv6,
                dnsCacheSize, optimisticCache, enableDNSSECOK, enableRetransmissionHandling);
    }

    /**
     * @return the default DNS proxy settings.
     */
    public static DnsProxySettings getDefault() {
        return DnsProxy.getDefaultSettings();
    }
}
