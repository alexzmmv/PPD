#include "dsm.hpp"
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

DSM::DSM(int selfId, const std::string& configPath)
    : selfId_(selfId), configPath_(configPath) {}

DSM::~DSM() { stop(); }

bool DSM::start() {
    if (!parse_config(configPath_)) {
        std::cerr << "Failed to parse DSM config\n";
        return false;
    }
    running_ = true;
    serverThread_ = std::thread(&DSM::server_loop, this);
    return true;
}

void DSM::stop() {
    if (running_) {
        running_ = false;
        // connect to self to unblock accept
        DSMMessage ping; ping.type = "RESP"; ping.op = "PING"; ping.originId = selfId_; ping.groupKey = ""; ping.reqId = 0;
        send_message_to(selfId_, ping);
        if (serverThread_.joinable()) serverThread_.join();
    }
}

bool DSM::write(int varId, int value) {
    auto it = varGroupKey_.find(varId);
    if (it == varGroupKey_.end()) return false;
    std::string gk = it->second;
    if (std::find(groupMembers_[gk].begin(), groupMembers_[gk].end(), selfId_) == groupMembers_[gk].end()) {
        std::cerr << "Not a subscriber of var " << varId << "\n";
        return false;
    }
    long long reqId;
    {
        std::lock_guard<std::mutex> lk(reqMutex_);
        reqId = nextReqId_++;
    }
    PendingReq pr; 
    {
        std::lock_guard<std::mutex> lk(reqMutex_);
        pending_[reqId] = &pr;
    }
    DSMMessage req;
    req.type = "REQUEST";
    req.op = "WRITE";
    req.varId = varId;
    req.value = value;
    req.originId = selfId_;
    req.groupKey = gk;
    req.reqId = reqId;

    int leader = groupLeader_[gk];
    if (!send_message_to(leader, req)) {
        std::cerr << "Failed to send request to leader\n";
        std::lock_guard<std::mutex> lk(reqMutex_);
        pending_.erase(reqId);
        return false;
    }
    std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    pr.cv.wait(lk, [&]{ return pr.done; });

    std::lock_guard<std::mutex> lk2(reqMutex_);
    pending_.erase(reqId);
    return pr.success;
}

bool DSM::compare_exchange(int varId, int expected, int desired) {
    auto it = varGroupKey_.find(varId);
    if (it == varGroupKey_.end()) return false;
    std::string gk = it->second;
    if (std::find(groupMembers_[gk].begin(), groupMembers_[gk].end(), selfId_) == groupMembers_[gk].end()) {
        std::cerr << "Not a subscriber of var " << varId << "\n";
        return false;
    }
    long long reqId;
    {
        std::lock_guard<std::mutex> lk(reqMutex_);
        reqId = nextReqId_++;
    }
    PendingReq pr; 
    {
        std::lock_guard<std::mutex> lk(reqMutex_);
        pending_[reqId] = &pr;
    }
    DSMMessage req;
    req.type = "REQUEST";
    req.op = "CMPXCHG";
    req.varId = varId;
    req.value = desired;
    req.expected = expected;
    req.originId = selfId_;
    req.groupKey = gk;
    req.reqId = reqId;

    int leader = groupLeader_[gk];
    if (!send_message_to(leader, req)) {
        std::cerr << "Failed to send request to leader\n";
        std::lock_guard<std::mutex> lk(reqMutex_);
        pending_.erase(reqId);
        return false;
    }
    std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    pr.cv.wait(lk, [&]{ return pr.done; });

    std::lock_guard<std::mutex> lk2(reqMutex_);
    pending_.erase(reqId);
    return pr.success;
}

int DSM::get_local_value(int varId) {
    auto it = varValue_.find(varId);
    if (it == varValue_.end()) return 0;
    return it->second;
}

void DSM::on_change(ChangeCallback cb) {
    callback_ = std::move(cb);
}

std::string DSM::canonical_group_key(const std::vector<int>& group) {
    std::vector<int> g = group;
    std::sort(g.begin(), g.end());
    std::stringstream ss;
    for (size_t i = 0; i < g.size(); ++i) {
        if (i) ss << "-";
        ss << g[i];
    }
    return ss.str();
}

bool DSM::is_leader_for_group(const std::string& key) const {
    auto it = groupLeader_.find(key);
    return it != groupLeader_.end() && it->second == selfId_;
}

bool DSM::parse_config(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    enum Section { NONE, NODES, VARS } sec = NONE;
    std::unordered_map<int, DSMNode> nodes;
    std::vector<DSMVariableCfg> vars;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        if (line == "Nodes:") { sec = NODES; continue; }
        if (line == "Variables:") { sec = VARS; continue; }
        if (sec == NODES) {
            // id host port
            std::stringstream ss(line);
            DSMNode n; ss >> n.id >> n.host >> n.port;
            nodes[n.id] = n;
        } else if (sec == VARS) {
            // id initial groupCSV
            std::stringstream ss(line);
            DSMVariableCfg v; std::string groupCSV;
            ss >> v.id >> v.initial >> groupCSV;
            auto parts = split(groupCSV, ',');
            for (auto& p : parts) v.group.push_back(std::stoi(p));
            vars.push_back(v);
        }
    }
    if (nodes.find(selfId_) == nodes.end()) {
        std::cerr << "Self node id not in config\n";
        return false;
    }
    nodes_ = nodes;
    self_ = nodes_[selfId_];

    for (auto& v : vars) {
        varValue_[v.id] = v.initial;
        std::string gk = canonical_group_key(v.group);
        varGroupKey_[v.id] = gk;
        groupMembers_[gk] = v.group;
    }

    // Leaders per group: lowest node id
    for (auto& kv : groupMembers_) {
        auto& members = kv.second;
        int leader = *std::min_element(members.begin(), members.end());
        groupLeader_[kv.first] = leader;
        if (leader == selfId_) {
            groupNextSeq_[kv.first] = 1; // start from 1
        }
    }

    return true;
}

void DSM::server_loop() {
    int srv_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        std::perror("socket");
        return;
    }
    int yes = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(self_.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(srv_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::perror("bind");
        close(srv_fd);
        return;
    }
    if (listen(srv_fd, 64) < 0) {
        std::perror("listen");
        close(srv_fd);
        return;
    }

    while (running_) {
        sockaddr_in cli{}; socklen_t clilen = sizeof(cli);
        int cfd = accept(srv_fd, (sockaddr*)&cli, &clilen);
        if (cfd < 0) {
            if (!running_) break;
            continue;
        }
        std::string buf;
        char tmp[1024]; ssize_t n;
        while ((n = read(cfd, tmp, sizeof(tmp))) > 0) {
            buf.append(tmp, tmp + n);
        }
        close(cfd);
        if (!buf.empty()) {
            DSMMessage msg = deserialize(buf);
            handle_message(msg);
        }
    }

    close(srv_fd);
}

void DSM::handle_message(const DSMMessage& msg) {
    if (msg.type == "REQUEST") {
        if (is_leader_for_group(msg.groupKey)) {
            handle_request_as_leader(msg);
        } else {
            // ignore; only leaders accept requests
        }
    } else if (msg.type == "COMMIT") {
        apply_commit(msg);
    } else if (msg.type == "RESP") {
        // response to a request (failed cmpxchg)
        std::lock_guard<std::mutex> lk(reqMutex_);
        auto it = pending_.find(msg.reqId);
        if (it != pending_.end()) {
            PendingReq* pr = it->second;
            pr->success = (msg.success == 1);
            pr->done = true;
            pr->cv.notify_all();
        }
    }
}

void DSM::handle_request_as_leader(const DSMMessage& req) {
    auto gm = groupMembers_.find(req.groupKey);
    if (gm == groupMembers_.end()) return;

    if (req.op == "WRITE") {
        long long seq = groupNextSeq_[req.groupKey]++;
        DSMMessage commit;
        commit.type = "COMMIT";
        commit.op = req.op;
        commit.varId = req.varId;
        commit.value = req.value;
        commit.groupKey = req.groupKey;
        commit.seq = seq;
        commit.reqId = req.reqId;
        broadcast_to_group(req.groupKey, commit);
    } else if (req.op == "CMPXCHG") {
        int cur = varValue_[req.varId];
        if (cur == req.expected) {
            long long seq = groupNextSeq_[req.groupKey]++;
            DSMMessage commit;
            commit.type = "COMMIT";
            commit.op = req.op;
            commit.varId = req.varId;
            commit.value = req.value;
            commit.expected = req.expected;
            commit.groupKey = req.groupKey;
            commit.seq = seq;
            commit.reqId = req.reqId;
            broadcast_to_group(req.groupKey, commit);
        } else {
            DSMMessage resp;
            resp.type = "RESP";
            resp.op = "CMPXCHG";
            resp.varId = req.varId;
            resp.originId = req.originId;
            resp.groupKey = req.groupKey;
            resp.reqId = req.reqId;
            resp.success = 0;
            send_message_to(req.originId, resp);
        }
    }
}

void DSM::apply_commit(const DSMMessage& commit) {
    // apply value
    varValue_[commit.varId] = commit.value;

    // success notify if it belongs to a pending request
    {
        std::lock_guard<std::mutex> lk(reqMutex_);
        auto it = pending_.find(commit.reqId);
        if (it != pending_.end()) {
            PendingReq* pr = it->second;
            pr->success = true;
            pr->done = true;
            pr->cv.notify_all();
        }
    }

    if (callback_) {
        callback_(commit.varId, commit.value, commit.seq);
    }
}

bool DSM::send_message_to(int nodeId, const DSMMessage& msg) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) return false;
    auto& node = it->second;

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(node.port);
    if (inet_pton(AF_INET, node.host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        return false;
    }
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return false;
    }
    std::string line = serialize(msg);
    ssize_t n = ::send(fd, line.data(), line.size(), 0);
    (void)n;
    close(fd);
    return true;
}

bool DSM::broadcast_to_group(const std::string& groupKey, const DSMMessage& msg) {
    auto it = groupMembers_.find(groupKey);
    if (it == groupMembers_.end()) return false;
    bool ok = true;
    for (int nid : it->second) {
        ok = send_message_to(nid, msg) && ok;
    }
    return ok;
}

std::string DSM::serialize(const DSMMessage& msg) {
    std::stringstream ss;
    // simple key=value; format, one line
    ss << "type=" << msg.type << ";";
    ss << "op=" << msg.op << ";";
    ss << "var=" << msg.varId << ";";
    ss << "value=" << msg.value << ";";
    ss << "expected=" << msg.expected << ";";
    ss << "origin=" << msg.originId << ";";
    ss << "group=" << msg.groupKey << ";";
    ss << "seq=" << msg.seq << ";";
    ss << "req=" << msg.reqId << ";";
    ss << "success=" << msg.success << ";";
    return ss.str();
}

DSMMessage DSM::deserialize(const std::string& line) {
    DSMMessage m;
    auto parts = split(line, ';');
    for (auto& kv : parts) {
        auto p = split(kv, '=');
        if (p.size() != 2) continue;
        auto k = p[0];
        auto v = p[1];
        if (k == "type") m.type = v;
        else if (k == "op") m.op = v;
        else if (k == "var") m.varId = std::stoi(v);
        else if (k == "value") m.value = std::stoi(v);
        else if (k == "expected") m.expected = std::stoi(v);
        else if (k == "origin") m.originId = std::stoi(v);
        else if (k == "group") m.groupKey = v;
        else if (k == "seq") m.seq = std::stoll(v);
        else if (k == "req") m.reqId = std::stoll(v);
        else if (k == "success") m.success = std::stoi(v);
    }
    return m;
}
