#include "network/natmapper.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

CNatMapper::status g_status;
std::mutex g_statusMutex;

std::string trim(const std::string& value)
{
    std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
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
    std::stringstream upnp;
    upnp << "upnpc -r " << m_port << " TCP 2>&1";
    std::string output = runCommand(upnp.str());
    if (outputLooksSuccessful(output)) {
        updateStatus(true, true, m_port, m_port, "UPnP", parseExternalIp(output), "mapped");
        return true;
    }

    std::string upnpMessage = lastLine(output);
    std::stringstream natpmp;
    natpmp << "natpmpc -a " << m_port << " " << m_port << " tcp " << m_leaseSeconds << " 2>&1";
    output = runCommand(natpmp.str());
    if (outputLooksSuccessful(output)) {
        updateStatus(true, true, m_port, m_port, "NAT-PMP", parseExternalIp(output), "mapped");
        return true;
    }

    std::string message = lastLine(output);
    if (message.empty()) {
        message = upnpMessage.empty() ? "upnpc/natpmpc unavailable or router refused mapping" : upnpMessage;
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

    std::stringstream upnp;
    upnp << "upnpc -d " << m_port << " TCP 2>&1";
    runCommand(upnp.str());
    updateStatus(false, false, m_port, m_port, "", "", "disabled");
}
