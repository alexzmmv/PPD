#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <future>
#include <memory>
#include <sstream>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

using namespace std;

typedef int SocketType;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define CLOSE_SOCKET close


struct HttpResponse {
    int statusCode = 0;
    map<string, string> headers;
    string body;
    size_t contentLength = 0;
    bool headersParsed = false;
};

class HttpParser {
public:
    static pair<string, string> parseUrl(const string& url) {
        string host, path = "/";
        size_t protocolEnd = url.find("://");
        size_t start = (protocolEnd != string::npos) ? protocolEnd + 3 : 0;
        size_t pathStart = url.find('/', start);
        
        if (pathStart != string::npos) {
            host = url.substr(start, pathStart - start);
            path = url.substr(pathStart);
        } else {
            host = url.substr(start);
        }
        
        return {host, path};
    }
    
    static string buildRequest(const string& host, const string& path) {
        stringstream ss;
        ss << "GET " << path << " HTTP/1.1\r\n";
        ss << "Host: " << host << "\r\n";
        ss << "Connection: close\r\n";
        ss << "User-Agent: SimpleDownloader/1.0\r\n";
        ss << "\r\n";
        return ss.str();
    }
    
    static bool parseResponse(const string& data, HttpResponse& response) {
        if (response.headersParsed) {
            response.body += data;
            return true;
        }
        
        size_t headerEnd = data.find("\r\n\r\n");
        if (headerEnd == string::npos) {
            return false;
        }
        
        string headerSection = data.substr(0, headerEnd);
        response.body = data.substr(headerEnd + 4);
        
        // Parse status line
        size_t lineEnd = headerSection.find("\r\n");
        string statusLine = headerSection.substr(0, lineEnd);
        
        size_t codeStart = statusLine.find(' ') + 1;
        size_t codeEnd = statusLine.find(' ', codeStart);
        response.statusCode = stoi(statusLine.substr(codeStart, codeEnd - codeStart));
        
        // Parse headers
        size_t pos = lineEnd + 2;
        while (pos < headerSection.size()) {
            lineEnd = headerSection.find("\r\n", pos);
            if (lineEnd == string::npos) break;
            
            string line = headerSection.substr(pos, lineEnd - pos);
            size_t colonPos = line.find(':');
            if (colonPos != string::npos) {
                string key = line.substr(0, colonPos);
                string value = line.substr(colonPos + 2);
                response.headers[key] = value;
                
                if (key == "Content-Length") {
                    response.contentLength = stoull(value);
                }
            }
            pos = lineEnd + 2;
        }
        
        response.headersParsed = true;
        return true;
    }
};

// ======================== UTILITY FUNCTIONS ========================

SocketType createNonBlockingSocket() {
    SocketType sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        throw runtime_error("Failed to create socket");
    }
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    return sock;
}

sockaddr_in resolveHost(const string& host, int port = 80) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    struct hostent* he = gethostbyname(host.c_str());
    if (he == nullptr) {
        throw runtime_error("Failed to resolve host: " + host);
    }
    
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    return addr;
}

bool isSocketReady(SocketType sock, bool forWrite) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    
    struct timeval tv = {0, 10000}; 
    
    int result = forWrite ? select(sock + 1, nullptr, &fds, nullptr, &tv)
                          : select(sock + 1, &fds, nullptr, nullptr, &tv);
    
    return result > 0 && FD_ISSET(sock, &fds);
}


class TaskBasedDownloader {
private:
    struct DownloadState {
        string url;
        string host;
        string path;
        SocketType sock;
        HttpResponse response;
        string buffer;
        
        DownloadState(const string& u) : url(u), sock(INVALID_SOCKET) {
            auto [h, p] = HttpParser::parseUrl(url);
            host = h;
            path = p;
        }
        
        ~DownloadState() {
            if (sock != INVALID_SOCKET) {
                CLOSE_SOCKET(sock);
            }
        }
    };
    
public:
    static future<HttpResponse> downloadAsync(const string& url) {
        return async(launch::async, [url]() {
            auto state = make_shared<DownloadState>(url);
            
            // Task 1: Connect
            SocketType sock = createNonBlockingSocket();
            sockaddr_in addr = resolveHost(state->host);
            
            int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
            if (result == SOCKET_ERROR && errno != EINPROGRESS) {
                CLOSE_SOCKET(sock);
                throw runtime_error("Connect failed");
            }
            
            // Wait for connection
            while (!isSocketReady(sock, true)) {
                this_thread::sleep_for(chrono::milliseconds(1));
            }
            state->sock = sock;
            
            // Task 2: Send request
            string request = HttpParser::buildRequest(state->host, state->path);
            int totalSent = 0;
            while (totalSent < request.length()) {
                if (isSocketReady(state->sock, true)) {
                    int sent = send(state->sock, request.c_str() + totalSent, 
                                   request.length() - totalSent, 0);
                    if (sent > 0) {
                        totalSent += sent;
                    } else if (sent == SOCKET_ERROR) {
                        throw runtime_error("Send failed");
                    }
                }
                this_thread::sleep_for(chrono::milliseconds(1));
            }
            
            // Task 3: Receive response
            string data;
            while (true) {
                if (isSocketReady(state->sock, false)) {
                    char buffer[4096];
                    int received = recv(state->sock, buffer, sizeof(buffer), 0);
                    
                    if (received > 0) {
                        data.append(buffer, received);
                    } else if (received == 0) {
                        break;
                    } else {
                        throw runtime_error("Receive failed");
                    }
                }
                this_thread::sleep_for(chrono::milliseconds(1));
            }
            
            // Parse response
            HttpParser::parseResponse(data, state->response);
            return state->response;
        });
    }
    
    static vector<future<HttpResponse>> startDownloads(const vector<string>& urls) {
        vector<future<HttpResponse>> futures;
        for (const auto& url : urls) {
            futures.push_back(downloadAsync(url));
        }
        return futures;
    }
};


int main() {
    vector<string> urls = {
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/index.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-1-noncooperative-mt.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-2-producer-consumer.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-3-parallel-simple.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-4-futures-continuations.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-5-parallel-algo.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-6-parallel-algo-2.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-7-mpi.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-8-distributed.html",
        "http://www.cs.ubbcluj.ro/~rlupsa/edu/pdp/lab-o1-opencl.html",
        "http://en.wikipedia.org/wiki/Hough_transform",
        "http://example.com/"
    };
    
    cout << "Downloading " << urls.size() << " URLs simultaneously\n\n";
    
    auto start = chrono::high_resolution_clock::now();
    
    auto futures = TaskBasedDownloader::startDownloads(urls);
    
    cout << "Downloads in progress...\n\n";
    for (size_t i = 0; i < urls.size(); i++) {
        try {
            auto response = futures[i].get();
            cout << "URL " << (i+1) << ": " << urls[i] << "\n";
            cout << "  Status: " << response.statusCode << "\n";
            cout << "  Content-Length: " << response.contentLength << "\n";
            cout << "  Body size: " << response.body.size() << " bytes\n\n";
        } catch (const exception& e) {
            cout << "URL " << (i+1) << ": " << urls[i] << "\n";
            cout << "  Error: " << e.what() << "\n\n";
        }
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    cout << "Total time: " << duration.count() << " ms\n";
    cout << "All downloads completed!\n";
    
    return 0;
}
