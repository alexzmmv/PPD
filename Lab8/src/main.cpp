#include "dsm.hpp"
#include <iostream>
#include <chrono>
#include <thread>

static void usage() {
    std::cout << "Usage: ./dsm_lab8 --id <nodeId> --config <path>\n";
}

int main(int argc, char** argv) {
    int id = -1; std::string cfg; bool daemon = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--id" && i + 1 < argc) { id = std::stoi(argv[++i]); }
        else if (a == "--config" && i + 1 < argc) { cfg = argv[++i]; }
        else if (a == "--daemon") { daemon = true; }
    }
    if (id < 0 || cfg.empty()) { usage(); return 1; }

    DSM dsm(id, cfg);
    dsm.on_change([&](int varId, int newValue, long long seq){
        std::cout << "[Node " << id << "] COMMIT seq=" << seq << " var=" << varId << " value=" << newValue << "\n";
    });

    if (!dsm.start()) return 2;

    std::cout << "Node " << id << " started.\n";
    if (!daemon) {
        std::cout << "Commands: w <var> <val> | c <var> <expected> <desired> | g <var> | q\n";
        // Simple REPL
        while (true) {
            std::cout << "> ";
            std::string cmd; if (!(std::cin >> cmd)) break;
            if (cmd == "q") break;
            if (cmd == "w") {
                int v, val; std::cin >> v >> val;
                bool ok = dsm.write(v, val);
                std::cout << (ok ? "OK" : "ERR") << "\n";
            } else if (cmd == "c") {
                int v, e, d; std::cin >> v >> e >> d;
                bool ok = dsm.compare_exchange(v, e, d);
                std::cout << (ok ? "SWAPPED" : "NO-SWAP") << "\n";
            } else if (cmd == "g") {
                int v; std::cin >> v;
                std::cout << "val(" << v << ")=" << dsm.get_local_value(v) << "\n";
            } else {
                std::cout << "Unknown command\n";
            }
        }
    } else {
        // Run as daemon: block until killed
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    dsm.stop();
    return 0;
}
