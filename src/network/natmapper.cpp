#include "network/natmapper.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <fstream>
#include <mutex>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <net/route.h>
#include <sys/sysctl.h>
#endif

namespace {

CNatMapper::status g_status;
std::mutex g_statusMutex;

std::size_t curlWriteCallback(void *contents, std::size_t size, std::size_t nmemb, void *userp)
{
    std::string *buffer = reinterpret_cast<std::string *>(userp);
    std::size_t total = size * nmemb;
    buffer->append(reinterpret_cast<char *>(contents), total);
    return total;
}

std::string trim(const std::string& value)
{
    std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string lowerString(const std::string& value)
{
    std::string lower;
    lower.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    }
    return lower;
}

std::string headerValue(const std::string& headers, const std::string& name)
{
    std::stringstream stream(headers);
    std::string line;
    std::string prefix = lowerString(name) + ":";
    while (std::getline(stream, line)) {
        std::string lower = lowerString(line);
        if (lower.compare(0, prefix.size(), prefix) == 0) {
            return trim(line.substr(prefix.size()));
        }
    }
    return "";
}

std::string xmlTagValue(const std::string& xml, const std::string& tag)
{
    std::string lowerXml = lowerString(xml);
    std::string lowerTag = lowerString(tag);
    std::string open = "<" + lowerTag;
    std::string close = "</" + lowerTag + ">";
    std::size_t openPos = lowerXml.find(open);
    if (openPos == std::string::npos) {
        return "";
    }
    std::size_t valueStart = lowerXml.find(">", openPos);
    if (valueStart == std::string::npos) {
        return "";
    }
    ++valueStart;
    std::size_t valueEnd = lowerXml.find(close, valueStart);
    if (valueEnd == std::string::npos) {
        return "";
    }
    return trim(xml.substr(valueStart, valueEnd - valueStart));
}

bool httpGet(const std::string& url, std::string& response)
{
    static bool curlInitialized = []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)curlInitialized;

    response.clear();
    CURL *curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1500L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode result = curl_easy_perform(curl);
    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && statusCode >= 200 && statusCode < 300;
}

bool httpSoapPost(const std::string& url,
                  const std::string& serviceType,
                  const std::string& action,
                  const std::string& body,
                  std::string& response)
{
    static bool curlInitialized = []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)curlInitialized;

    response.clear();
    CURL *curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string soapAction = "SOAPAction: \"" + serviceType + "#" + action + "\"";
    struct curl_slist *headers = 0;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=\"utf-8\"");
    headers = curl_slist_append(headers, soapAction.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1500L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode result = curl_easy_perform(curl);
    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && statusCode >= 200 && statusCode < 300;
}

std::string runCommand(const std::string& command)
{
    std::string output;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return output;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        output += buffer;
    }
    pclose(pipe);
    return output;
}

uint16_t readUInt16(const unsigned char *data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

uint32_t readUInt32(const unsigned char *data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

void writeUInt16(std::vector<unsigned char>& data, std::size_t offset, uint16_t value)
{
    data[offset] = static_cast<unsigned char>((value >> 8) & 0xff);
    data[offset + 1] = static_cast<unsigned char>(value & 0xff);
}

void writeUInt32(std::vector<unsigned char>& data, std::size_t offset, uint32_t value)
{
    data[offset] = static_cast<unsigned char>((value >> 24) & 0xff);
    data[offset + 1] = static_cast<unsigned char>((value >> 16) & 0xff);
    data[offset + 2] = static_cast<unsigned char>((value >> 8) & 0xff);
    data[offset + 3] = static_cast<unsigned char>(value & 0xff);
}

std::string ipv4ToString(uint32_t networkOrderAddress)
{
    struct in_addr address;
    address.s_addr = networkOrderAddress;
    char buffer[INET_ADDRSTRLEN];
    const char *result = inet_ntop(AF_INET, &address, buffer, sizeof(buffer));
    return result ? std::string(result) : "";
}

std::string sockaddrToIpv4String(const struct sockaddr *address)
{
    if (!address || address->sa_family != AF_INET) {
        return "";
    }
    const struct sockaddr_in *ipv4 = reinterpret_cast<const struct sockaddr_in *>(address);
    return ipv4ToString(ipv4->sin_addr.s_addr);
}

#if defined(__APPLE__)
struct sockaddr *nextRouteSocketAddress(struct sockaddr *address)
{
    if (!address) {
        return address;
    }
    std::size_t length = address->sa_len > 0 ? address->sa_len : sizeof(long);
    length = (length + sizeof(long) - 1) & ~(sizeof(long) - 1);
    return reinterpret_cast<struct sockaddr *>(reinterpret_cast<char *>(address) + length);
}
#endif

std::string defaultGatewayAddress()
{
#if defined(__APPLE__)
    int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_GATEWAY };
    std::size_t needed = 0;
    if (sysctl(mib, 6, 0, &needed, 0, 0) < 0 || needed == 0) {
        return "";
    }

    std::vector<char> buffer(needed);
    if (sysctl(mib, 6, &buffer[0], &needed, 0, 0) < 0) {
        return "";
    }

    char *limit = &buffer[0] + needed;
    for (char *next = &buffer[0]; next < limit;) {
        struct rt_msghdr *route = reinterpret_cast<struct rt_msghdr *>(next);
        struct sockaddr *address = reinterpret_cast<struct sockaddr *>(route + 1);
        std::string destination;
        std::string gateway;
        for (int i = 0; i < RTAX_MAX; ++i) {
            if (route->rtm_addrs & (1 << i)) {
                if (i == RTAX_DST) {
                    destination = sockaddrToIpv4String(address);
                }
                if (i == RTAX_GATEWAY) {
                    gateway = sockaddrToIpv4String(address);
                }
                address = nextRouteSocketAddress(address);
            }
        }
        if (!gateway.empty() && (destination.empty() || destination == "0.0.0.0")) {
            return gateway;
        }
        next += route->rtm_msglen;
    }
#elif defined(__linux__)
    std::ifstream routes("/proc/net/route");
    std::string line;
    std::getline(routes, line);
    while (std::getline(routes, line)) {
        std::stringstream ss(line);
        std::string iface;
        std::string destination;
        std::string gateway;
        std::string flags;
        ss >> iface >> destination >> gateway >> flags;
        if (destination != "00000000" || gateway.empty()) {
            continue;
        }
        unsigned long gatewayValue = std::strtoul(gateway.c_str(), 0, 16);
        struct in_addr address;
        address.s_addr = static_cast<in_addr_t>(gatewayValue);
        return ipv4ToString(address.s_addr);
    }
#endif
    return "";
}

struct UrlParts {
    std::string scheme;
    std::string host;
    int port;
    std::string path;
};

bool parseUrl(const std::string& url, UrlParts& parts)
{
    std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        return false;
    }
    parts.scheme = url.substr(0, schemeEnd);
    std::size_t hostStart = schemeEnd + 3;
    std::size_t pathStart = url.find("/", hostStart);
    std::string hostPort = pathStart == std::string::npos ? url.substr(hostStart) : url.substr(hostStart, pathStart - hostStart);
    parts.path = pathStart == std::string::npos ? "/" : url.substr(pathStart);
    parts.port = parts.scheme == "https" ? 443 : 80;

    std::size_t colon = hostPort.rfind(":");
    if (colon != std::string::npos) {
        parts.host = hostPort.substr(0, colon);
        parts.port = std::atoi(hostPort.substr(colon + 1).c_str());
    } else {
        parts.host = hostPort;
    }
    return !parts.scheme.empty() && !parts.host.empty() && parts.port > 0;
}

std::string absoluteUrl(const std::string& baseUrl, const std::string& path)
{
    if (path.find("http://") == 0 || path.find("https://") == 0) {
        return path;
    }

    UrlParts base;
    if (!parseUrl(baseUrl, base)) {
        return "";
    }

    std::stringstream url;
    url << base.scheme << "://" << base.host;
    if (!((base.scheme == "http" && base.port == 80) ||
          (base.scheme == "https" && base.port == 443))) {
        url << ":" << base.port;
    }
    if (path.empty()) {
        url << "/";
    } else if (path[0] == '/') {
        url << path;
    } else {
        std::size_t slash = base.path.find_last_of('/');
        std::string parent = slash == std::string::npos ? "/" : base.path.substr(0, slash + 1);
        url << parent << path;
    }
    return url.str();
}

std::string localAddressForRemote(const std::string& remoteAddress, int remotePort)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return "";
    }

    struct sockaddr_in remote;
    std::memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(static_cast<uint16_t>(remotePort));
    if (inet_pton(AF_INET, remoteAddress.c_str(), &remote.sin_addr) != 1) {
        close(sock);
        return "";
    }

    if (connect(sock, reinterpret_cast<struct sockaddr *>(&remote), sizeof(remote)) != 0) {
        close(sock);
        return "";
    }

    struct sockaddr_in local;
    socklen_t localLength = sizeof(local);
    std::memset(&local, 0, sizeof(local));
    if (getsockname(sock, reinterpret_cast<struct sockaddr *>(&local), &localLength) != 0) {
        close(sock);
        return "";
    }
    close(sock);
    return ipv4ToString(local.sin_addr.s_addr);
}

std::vector<std::string> discoverUpnpLocations()
{
    std::vector<std::string> locations;
    std::set<std::string> seen;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return locations;
    }

    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in multicast;
    std::memset(&multicast, 0, sizeof(multicast));
    multicast.sin_family = AF_INET;
    multicast.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &multicast.sin_addr);

    const char *targets[] = {
        "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
        "urn:schemas-upnp-org:service:WANIPConnection:1",
        "urn:schemas-upnp-org:service:WANPPPConnection:1"
    };

    for (std::size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
        std::stringstream request;
        request << "M-SEARCH * HTTP/1.1\r\n"
                << "HOST: 239.255.255.250:1900\r\n"
                << "MAN: \"ssdp:discover\"\r\n"
                << "MX: 2\r\n"
                << "ST: " << targets[i] << "\r\n\r\n";
        std::string data = request.str();
        sendto(sock,
               data.c_str(),
               data.size(),
               0,
               reinterpret_cast<struct sockaddr *>(&multicast),
               sizeof(multicast));
    }

    while (true) {
        char buffer[4096];
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, 0, 0);
        if (received <= 0) {
            break;
        }
        buffer[received] = '\0';
        std::string location = headerValue(std::string(buffer), "location");
        if (!location.empty() && seen.insert(location).second) {
            locations.push_back(location);
        }
    }

    close(sock);
    return locations;
}

bool findUpnpService(const std::string& descriptionUrl,
                     const std::string& descriptionXml,
                     std::string& serviceType,
                     std::string& controlUrl)
{
    const char *serviceTypes[] = {
        "urn:schemas-upnp-org:service:WANIPConnection:2",
        "urn:schemas-upnp-org:service:WANIPConnection:1",
        "urn:schemas-upnp-org:service:WANPPPConnection:1"
    };

    std::string lowerXml = lowerString(descriptionXml);
    for (std::size_t i = 0; i < sizeof(serviceTypes) / sizeof(serviceTypes[0]); ++i) {
        std::string wanted = lowerString(serviceTypes[i]);
        std::size_t typePos = lowerXml.find(wanted);
        if (typePos == std::string::npos) {
            continue;
        }

        std::size_t serviceStart = lowerXml.rfind("<service", typePos);
        std::size_t serviceEnd = lowerXml.find("</service>", typePos);
        if (serviceStart == std::string::npos || serviceEnd == std::string::npos) {
            continue;
        }

        std::string serviceXml = descriptionXml.substr(serviceStart, serviceEnd - serviceStart);
        std::string controlPath = xmlTagValue(serviceXml, "controlURL");
        if (controlPath.empty()) {
            continue;
        }

        serviceType = serviceTypes[i];
        controlUrl = absoluteUrl(descriptionUrl, controlPath);
        return !controlUrl.empty();
    }

    return false;
}

std::string soapEnvelope(const std::string& serviceType,
                         const std::string& action,
                         const std::string& arguments)
{
    std::stringstream body;
    body << "<?xml version=\"1.0\"?>"
         << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
         << "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
         << "<s:Body>"
         << "<u:" << action << " xmlns:u=\"" << serviceType << "\">"
         << arguments
         << "</u:" << action << ">"
         << "</s:Body>"
         << "</s:Envelope>";
    return body.str();
}

bool getUpnpExternalAddress(const std::string& controlUrl,
                            const std::string& serviceType,
                            std::string& externalAddress)
{
    std::string response;
    std::string body = soapEnvelope(serviceType, "GetExternalIPAddress", "");
    if (!httpSoapPost(controlUrl, serviceType, "GetExternalIPAddress", body, response)) {
        return false;
    }
    externalAddress = xmlTagValue(response, "NewExternalIPAddress");
    return !externalAddress.empty();
}

bool addUpnpMapping(const std::string& controlUrl,
                    const std::string& serviceType,
                    const std::string& internalAddress,
                    int port,
                    int leaseSeconds)
{
    std::stringstream arguments;
    arguments << "<NewRemoteHost></NewRemoteHost>"
              << "<NewExternalPort>" << port << "</NewExternalPort>"
              << "<NewProtocol>TCP</NewProtocol>"
              << "<NewInternalPort>" << port << "</NewInternalPort>"
              << "<NewInternalClient>" << internalAddress << "</NewInternalClient>"
              << "<NewEnabled>1</NewEnabled>"
              << "<NewPortMappingDescription>Safire</NewPortMappingDescription>"
              << "<NewLeaseDuration>" << leaseSeconds << "</NewLeaseDuration>";

    std::string response;
    std::string body = soapEnvelope(serviceType, "AddPortMapping", arguments.str());
    return httpSoapPost(controlUrl, serviceType, "AddPortMapping", body, response);
}

void deleteUpnpMapping(const std::string& controlUrl, const std::string& serviceType, int port)
{
    std::stringstream arguments;
    arguments << "<NewRemoteHost></NewRemoteHost>"
              << "<NewExternalPort>" << port << "</NewExternalPort>"
              << "<NewProtocol>TCP</NewProtocol>";
    std::string response;
    std::string body = soapEnvelope(serviceType, "DeletePortMapping", arguments.str());
    httpSoapPost(controlUrl, serviceType, "DeletePortMapping", body, response);
}

bool mapWithNativeUpnp(int port, int leaseSeconds, std::string& externalAddress, std::string& message)
{
    std::vector<std::string> locations = discoverUpnpLocations();
    if (locations.empty()) {
        message = "router did not answer UPnP discovery";
        return false;
    }

    bool foundPortMappingService = false;
    for (std::size_t i = 0; i < locations.size(); ++i) {
        std::string description;
        if (!httpGet(locations[i], description)) {
            continue;
        }

        std::string serviceType;
        std::string controlUrl;
        if (!findUpnpService(locations[i], description, serviceType, controlUrl)) {
            continue;
        }
        foundPortMappingService = true;

        UrlParts controlParts;
        if (!parseUrl(controlUrl, controlParts)) {
            continue;
        }

        std::string localAddress = localAddressForRemote(controlParts.host, controlParts.port);
        if (localAddress.empty()) {
            std::string gateway = defaultGatewayAddress();
            if (!gateway.empty()) {
                localAddress = localAddressForRemote(gateway, 1900);
            }
        }
        if (localAddress.empty()) {
            message = "local network address not found";
            return false;
        }

        if (addUpnpMapping(controlUrl, serviceType, localAddress, port, leaseSeconds)) {
            getUpnpExternalAddress(controlUrl, serviceType, externalAddress);
            message = "mapped";
            return true;
        }
    }

    if (!foundPortMappingService) {
        message = "router does not expose UPnP port mapping";
        return false;
    }

    message = "router refused UPnP port mapping";
    return false;
}

void clearNativeUpnpMapping(int port)
{
    std::vector<std::string> locations = discoverUpnpLocations();
    for (std::size_t i = 0; i < locations.size(); ++i) {
        std::string description;
        if (!httpGet(locations[i], description)) {
            continue;
        }

        std::string serviceType;
        std::string controlUrl;
        if (findUpnpService(locations[i], description, serviceType, controlUrl)) {
            deleteUpnpMapping(controlUrl, serviceType, port);
        }
    }
}

std::string findExecutable(const std::string& name)
{
    const char *pathEnv = std::getenv("PATH");
    std::string searchPath = pathEnv ? pathEnv : "";
    if (searchPath.empty()) {
        searchPath = "/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin";
    } else {
        searchPath += ":/usr/local/bin:/opt/homebrew/bin";
    }

    std::stringstream ss(searchPath);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) {
            continue;
        }
        std::string candidate = dir + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }
    return "";
}

std::string natPmpResultMessage(uint16_t result)
{
    switch (result) {
        case 0: return "success";
        case 1: return "router does not support NAT-PMP";
        case 2: return "router refused automatic port mapping";
        case 3: return "network failure while mapping port";
        case 4: return "router ran out of mapping resources";
        case 5: return "unsupported NAT-PMP version";
        default: return "router returned NAT-PMP error";
    }
}

bool natPmpRequest(const std::string& gateway,
                   const std::vector<unsigned char>& request,
                   std::vector<unsigned char>& response,
                   std::string& message)
{
    response.clear();
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        message = "unable to open NAT-PMP socket";
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in router;
    std::memset(&router, 0, sizeof(router));
    router.sin_family = AF_INET;
    router.sin_port = htons(5351);
    if (inet_pton(AF_INET, gateway.c_str(), &router.sin_addr) != 1) {
        close(sock);
        message = "router gateway address is invalid";
        return false;
    }

    ssize_t sent = sendto(sock,
                          reinterpret_cast<const char *>(&request[0]),
                          request.size(),
                          0,
                          reinterpret_cast<struct sockaddr *>(&router),
                          sizeof(router));
    if (sent < 0 || static_cast<std::size_t>(sent) != request.size()) {
        close(sock);
        message = "unable to send NAT-PMP request";
        return false;
    }

    unsigned char buffer[64];
    ssize_t received = recvfrom(sock, reinterpret_cast<char *>(buffer), sizeof(buffer), 0, 0, 0);
    close(sock);
    if (received <= 0) {
        message = "router did not answer NAT-PMP";
        return false;
    }

    response.assign(buffer, buffer + received);
    return true;
}

bool mapWithNativeNatPmp(int port, int leaseSeconds, std::string& externalAddress, std::string& message)
{
    std::string gateway = defaultGatewayAddress();
    if (gateway.empty()) {
        message = "router gateway not found";
        return false;
    }

    std::vector<unsigned char> request(2);
    request[0] = 0;
    request[1] = 0;
    std::vector<unsigned char> response;
    if (natPmpRequest(gateway, request, response, message) && response.size() >= 12) {
        if (response[0] == 0 && response[1] == 128 && readUInt16(&response[2]) == 0) {
            externalAddress = ipv4ToString(htonl(readUInt32(&response[8])));
        }
    }

    request.assign(12, 0);
    request[0] = 0;
    request[1] = 2; // Map TCP.
    writeUInt16(request, 4, static_cast<uint16_t>(port));
    writeUInt16(request, 6, static_cast<uint16_t>(port));
    writeUInt32(request, 8, static_cast<uint32_t>(leaseSeconds));

    if (!natPmpRequest(gateway, request, response, message)) {
        return false;
    }
    if (response.size() < 16 || response[0] != 0 || response[1] != 130) {
        message = "router returned invalid NAT-PMP response";
        return false;
    }

    uint16_t result = readUInt16(&response[2]);
    if (result != 0) {
        message = natPmpResultMessage(result);
        return false;
    }

    uint16_t mappedPort = readUInt16(&response[10]);
    if (mappedPort != static_cast<uint16_t>(port)) {
        message = "router mapped a different public port";
        return false;
    }

    message = "mapped";
    return true;
}

std::string parseAfterToken(const std::string& output, const std::string& token)
{
    std::size_t start = output.find(token);
    if (start == std::string::npos) {
        return "";
    }
    start += token.length();
    std::size_t end = output.find_first_of("\r\n", start);
    if (end == std::string::npos) {
        end = output.length();
    }
    return trim(output.substr(start, end - start));
}

std::string parseExternalIp(const std::string& output)
{
    std::string value = parseAfterToken(output, "ExternalIPAddress = ");
    if (!value.empty()) {
        return value;
    }

    value = parseAfterToken(output, "Public IP address : ");
    if (!value.empty()) {
        return value;
    }

    return "";
}

bool outputLooksSuccessful(const std::string& output)
{
    std::string lower;
    lower.reserve(output.size());
    for (std::size_t i = 0; i < output.size(); ++i) {
        lower += static_cast<char>(std::tolower(output[i]));
    }
    if (lower.find("failed") != std::string::npos ||
        lower.find("not found") != std::string::npos ||
        lower.find("no igd") != std::string::npos ||
        lower.find("error") != std::string::npos ||
        lower.find("can't") != std::string::npos ||
        lower.find("cannot") != std::string::npos) {
        return false;
    }
    return output.find("is redirected to internal") != std::string::npos ||
           output.find("AddPortMapping() returned") != std::string::npos ||
           output.find("Mapped public port") != std::string::npos ||
           output.find("mapping successful") != std::string::npos;
}

std::string lastLine(const std::string& output)
{
    std::size_t end = output.find_last_not_of("\r\n");
    if (end == std::string::npos) {
        return "";
    }
    std::size_t start = output.find_last_of("\r\n", end);
    if (start == std::string::npos) {
        return trim(output.substr(0, end + 1));
    }
    return trim(output.substr(start + 1, end - start));
}

void updateStatus(bool enabled, bool mapped, int internalPort, int externalPort,
                  const std::string& method, const std::string& externalAddress,
                  const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_statusMutex);
    g_status.enabled = enabled;
    g_status.mapped = mapped;
    g_status.internalPort = internalPort;
    g_status.externalPort = externalPort;
    g_status.method = method;
    g_status.externalAddress = externalAddress;
    g_status.message = message;
}

}

CNatMapper::CNatMapper()
    : m_port(0),
      m_leaseSeconds(3600),
      m_running(false)
{
}

CNatMapper::~CNatMapper()
{
    stop();
}

bool CNatMapper::start(int port, int leaseSeconds)
{
    if (port <= 0 || port > 65535) {
        updateStatus(true, false, port, port, "", "", "invalid node port");
        return false;
    }
    if (m_running) {
        return true;
    }

    m_port = port;
    m_leaseSeconds = leaseSeconds > 300 ? leaseSeconds : 3600;
    m_running = true;
    updateStatus(true, false, m_port, m_port, "", "", "starting");
    mapOnce();
    m_thread = std::thread(&CNatMapper::run, this);
    return true;
}

void CNatMapper::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    clearMapping();
}

CNatMapper::status CNatMapper::currentStatus()
{
    std::lock_guard<std::mutex> lock(g_statusMutex);
    return g_status;
}

void CNatMapper::run()
{
    while (m_running) {
        int sleepSeconds = m_leaseSeconds / 2;
        if (sleepSeconds < 300) {
            sleepSeconds = 300;
        }
        for (int i = 0; i < sleepSeconds && m_running; ++i) {
            sleep(1);
        }
        if (m_running) {
            mapOnce();
        }
    }
}

bool CNatMapper::mapOnce()
{
    std::string nativeExternalAddress;
    std::string nativeMessage;
    if (mapWithNativeNatPmp(m_port, m_leaseSeconds, nativeExternalAddress, nativeMessage)) {
        updateStatus(true, true, m_port, m_port, "NAT-PMP", nativeExternalAddress, "mapped");
        return true;
    }

    std::string nativeUpnpExternalAddress;
    std::string nativeUpnpMessage;
    if (mapWithNativeUpnp(m_port, m_leaseSeconds, nativeUpnpExternalAddress, nativeUpnpMessage)) {
        updateStatus(true, true, m_port, m_port, "UPnP", nativeUpnpExternalAddress, "mapped");
        return true;
    }

    std::string upnpCommand = findExecutable("upnpc");
    std::string natpmpCommand = findExecutable("natpmpc");
    std::string output;
    std::string upnpMessage;

    if (!upnpCommand.empty()) {
        std::stringstream upnp;
        upnp << "\"" << upnpCommand << "\" -r " << m_port << " TCP 2>&1";
        output = runCommand(upnp.str());
        if (outputLooksSuccessful(output)) {
            updateStatus(true, true, m_port, m_port, "UPnP", parseExternalIp(output), "mapped");
            return true;
        }
        upnpMessage = lastLine(output);
    }

    if (!natpmpCommand.empty()) {
        std::stringstream natpmp;
        natpmp << "\"" << natpmpCommand << "\" -a " << m_port << " " << m_port << " tcp " << m_leaseSeconds << " 2>&1";
        output = runCommand(natpmp.str());
        if (outputLooksSuccessful(output)) {
            updateStatus(true, true, m_port, m_port, "NAT-PMP", parseExternalIp(output), "mapped");
            return true;
        }
    }

    std::string message = lastLine(output);
    if (upnpCommand.empty() && natpmpCommand.empty()) {
        message = !nativeUpnpMessage.empty() ? nativeUpnpMessage :
                  (nativeMessage.empty() ? "automatic router mapping unavailable" : nativeMessage);
    } else if (message.empty()) {
        message = upnpMessage.empty() ? (nativeUpnpMessage.empty() ? nativeMessage : nativeUpnpMessage) : upnpMessage;
        if (message.empty()) {
            message = "router refused automatic port mapping";
        }
    }
    updateStatus(true, false, m_port, m_port, "", "", message);
    return false;
}

void CNatMapper::clearMapping()
{
    if (m_port <= 0) {
        updateStatus(false, false, 0, 0, "", "", "disabled");
        return;
    }

    std::string externalAddress;
    std::string message;
    mapWithNativeNatPmp(m_port, 0, externalAddress, message);
    clearNativeUpnpMapping(m_port);

    std::string upnpCommand = findExecutable("upnpc");
    if (!upnpCommand.empty()) {
        std::stringstream upnp;
        upnp << "\"" << upnpCommand << "\" -d " << m_port << " TCP 2>&1";
        runCommand(upnp.str());
    }
    updateStatus(false, false, m_port, m_port, "", "", "disabled");
}
