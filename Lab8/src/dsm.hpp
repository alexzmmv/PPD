#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

struct DSMNode {
    int id;
    std::string host;
    int port;
};

struct DSMVariableCfg {
    int id;
    int initial;
    std::vector<int> group; // subscriber node IDs
};

struct DSMMessage {
    std::string type;      // REQUEST | COMMIT | RESP
    std::string op;        // WRITE | CMPXCHG
    int varId = -1;
    int value = 0;
    int expected = 0;
    int originId = -1;
    std::string groupKey;  // canonical group key
    long long seq = 0;     // total order per group
    long long reqId = 0;   // request correlation id
    int success = -1;      // for RESP of CMPXCHG
};

class DSM {
public:
    using ChangeCallback = std::function<void(int /*varId*/, int /*newValue*/, long long /*seq*/)>;

    DSM(int selfId, const std::string& configPath);
    ~DSM();

    bool start();
    void stop();

    // Operations
    bool write(int varId, int value);
    bool compare_exchange(int varId, int expected, int desired);

    // Introspection
    int get_local_value(int varId);

    // Register change callback
    void on_change(ChangeCallback cb);

private:
    int selfId_;
    std::string configPath_;

    // Networking
    DSMNode self_;
    std::unordered_map<int, DSMNode> nodes_; // id -> node

    // Variables and groups
    std::unordered_map<int, int> varValue_; // varId -> value
    std::unordered_map<int, std::string> varGroupKey_; // varId -> groupKey
    std::unordered_map<std::string, std::vector<int>> groupMembers_; // groupKey -> node IDs
    std::unordered_map<std::string, int> groupLeader_; // groupKey -> leader node id
    std::unordered_map<std::string, long long> groupNextSeq_; // only valid on leader

    ChangeCallback callback_ = nullptr;

    // Server
    std::thread serverThread_;
    std::atomic<bool> running_{false};

    // Pending requests
    std::mutex reqMutex_;
    long long nextReqId_ = 1; // local
    struct PendingReq {
        std::condition_variable cv;
        bool done = false;
        bool success = false;
    };
    std::unordered_map<long long, PendingReq*> pending_; // reqId -> pending

    // Helpers
    bool parse_config(const std::string& path);
    std::string canonical_group_key(const std::vector<int>& group);
    bool is_leader_for_group(const std::string& key) const;

    // Networking helpers
    void server_loop();
    void handle_message(const DSMMessage& msg);
    void handle_request_as_leader(const DSMMessage& req);
    void apply_commit(const DSMMessage& commit);

    bool send_message_to(int nodeId, const DSMMessage& msg);
    bool broadcast_to_group(const std::string& groupKey, const DSMMessage& msg);

    // Serialization
    static std::string serialize(const DSMMessage& msg);
    static DSMMessage deserialize(const std::string& line);

    // Utility
    int port_for_self() const;
};
