#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <cstdlib>
#include <iomanip>
#include <chrono>

using namespace std;

enum class Strategy {
    RowBlock = 0,
    ColBlock = 1,
    Cyclic   = 2
};

struct TaskArgs {
    int tid;        // thread id
    int T;          // total threads
    int n, m, p;    // A is n x m, B is m x p, C is n x p
    const std::vector<double> *A;
    const std::vector<double> *B;
    std::vector<double> *C;
    Strategy strat;
};

static double compute_element(const std::vector<double> &A, const std::vector<double> &B,
                              int n, int m, int p,
                              int i, int j, int tid) {
    //cout << "thread " << tid << " -> computing C[" << i << "][" << j << "]\n";
    double sum = 0.0;
    for (int k = 0; k < m; ++k) {
        sum += A[i * m + k] * B[k * p + j];
    }
    return sum;
}

static Strategy parse_strategy(const char *s) {
    string v(s ? s : "");
    if (v == "row" || v == "rows") return Strategy::RowBlock;
    if (v == "col" || v == "cols" || v == "column") return Strategy::ColBlock;
    if (v == "cyclic" || v == "roundrobin") return Strategy::Cyclic;
    return Strategy::RowBlock;
}

static void fill_matrices(vector<double> &A, vector<double> &B, int n, int m, int p) {
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < m; ++k)
            A[i * m + k] = static_cast<double>(1);
    for (int k = 0; k < m; ++k)
        for (int j = 0; j < p; ++j)
            B[k * p + j] = static_cast<double>(k + j);
}

static void usage(const char *prog) {
    cerr << "Usage: " << (prog ? prog : "prog") << " n m p threads strategy\n";
    cerr << "  A is n x m, B is m x p, C is n x p\n";
    cerr << "  strategy: row | col | cyclic\n";
}

int main(int argc, char **argv) {
    cout.setf(ios::unitbuf);
    if (argc < 6) {
        usage(argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    int p = atoi(argv[3]);
    int T = atoi(argv[4]);
    Strategy strat = parse_strategy(argv[5]);

    if (n <= 0 || m <= 0 || p <= 0 || T <= 0) {
        usage(argv[0]);
        return 1;
    }

    vector<double> A(static_cast<size_t>(n) * m);
    vector<double> B(static_cast<size_t>(m) * p);
    vector<double> C(static_cast<size_t>(n) * p, 0.0);

    fill_matrices(A, B, n, m, p);

    vector<thread> threads;
    threads.reserve(static_cast<size_t>(T));

    const int total = n * p;
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int tid = 0; tid < T; ++tid) {
        threads.emplace_back([=, &A, &B, &C]() {
            switch (strat) {
                case Strategy::RowBlock: {
                    int base = total / T;
                    int start = tid * base;
                    int end = (tid == T - 1) ? total : start + base;
                    for (int idx = start; idx < end; ++idx) {
                        int i = idx / p;
                        int j = idx % p;
                        C[i * p + j] = compute_element(A, B, n, m, p, i, j, tid);
                    }
                    break;
                }
                case Strategy::ColBlock: {
                    int base = total / T;
                    int start = tid * base;
                    int end = (tid == T - 1) ? total : start + base;
                    for (int idx = start; idx < end; ++idx) {
                        int j = idx / n;
                        int i = idx % n;
                        C[i * p + j] = compute_element(A, B, n, m, p, i, j, tid);
                    }
                    break;
                }
                case Strategy::Cyclic: {
                    for (int idx = tid; idx < total; idx += T) {
                        int i = idx / p;
                        int j = idx % p;
                        C[i * p + j] = compute_element(A, B, n, m, p, i, j, tid);
                    }
                    break;
                }
            }
        });
    }

    for (auto &th : threads) th.join();
    auto end_time = chrono::high_resolution_clock::now();
    auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
    cout << "Time takeinn: " << elapsed_ms << " ms\n";
/*
    cout << "Result C (" << n << "x" << p << "):\n";
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < p; ++j)
            cout << ' ' << fixed << setprecision(2) << setw(8) << C[i * p + j];
        cout << '\n';
    }
*/
    return 0;
}
