#include <mpi.h>
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>

using namespace std;
using namespace chrono;

class Polynomial {
public:
    vector<int> coefficients;
    Polynomial(int degree = 0) : coefficients(degree + 1, 0) {}
    Polynomial(const vector<int>& coef) : coefficients(coef) {}
    int degree() const { return coefficients.size() - 1; }
    static Polynomial random(int degree, int minVal = -10, int maxVal = 10) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(minVal, maxVal);
        vector<int> coef(degree + 1);
        for (int i = 0; i <= degree; ++i) coef[i] = 1; // deterministic for easier checks
        return Polynomial(coef);
    }
};

// CPU implementations (adapted from Lab5)
Polynomial multiplyRegularSequential(const Polynomial& p1, const Polynomial& p2) {
    int resultDegree = p1.degree() + p2.degree();
    Polynomial result(resultDegree);
    for (int i = 0; i <= p1.degree(); i++)
        for (int j = 0; j <= p2.degree(); j++)
            result.coefficients[i + j] += p1.coefficients[i] * p2.coefficients[j];
    return result;
}

Polynomial addPolynomials(const Polynomial& p1, const Polynomial& p2) {
    int maxDegree = max(p1.degree(), p2.degree());
    Polynomial result(maxDegree);
    for (int i = 0; i <= maxDegree; i++) {
        if (i <= p1.degree()) result.coefficients[i] += p1.coefficients[i];
        if (i <= p2.degree()) result.coefficients[i] += p2.coefficients[i];
    }
    return result;
}

Polynomial subtractPolynomials(const Polynomial& p1, const Polynomial& p2) {
    int maxDegree = max(p1.degree(), p2.degree());
    Polynomial result(maxDegree);
    for (int i = 0; i <= maxDegree; i++) {
        if (i <= p1.degree()) result.coefficients[i] += p1.coefficients[i];
        if (i <= p2.degree()) result.coefficients[i] -= p2.coefficients[i];
    }
    return result;
}

Polynomial karatsubaSequential(const Polynomial& p1, const Polynomial& p2) {
    if (p1.degree() < 10 || p2.degree() < 10) return multiplyRegularSequential(p1, p2);
    int n = max(p1.degree(), p2.degree()) + 1;
    int mid = n / 2;
    Polynomial low1(max(0, mid - 1));
    Polynomial high1(max(0, p1.degree() - mid));
    for (int i = 0; i < mid && i <= p1.degree(); i++) low1.coefficients[i] = p1.coefficients[i];
    for (int i = mid; i <= p1.degree(); i++) high1.coefficients[i - mid] = p1.coefficients[i];
    Polynomial low2(max(0, mid - 1));
    Polynomial high2(max(0, p2.degree() - mid));
    for (int i = 0; i < mid && i <= p2.degree(); i++) low2.coefficients[i] = p2.coefficients[i];
    for (int i = mid; i <= p2.degree(); i++) high2.coefficients[i - mid] = p2.coefficients[i];
    Polynomial z0 = karatsubaSequential(low1, low2);
    Polynomial z2 = karatsubaSequential(high1, high2);
    Polynomial sum1 = addPolynomials(low1, high1);
    Polynomial sum2 = addPolynomials(low2, high2);
    Polynomial z1 = karatsubaSequential(sum1, sum2);
    z1 = subtractPolynomials(z1, z2);
    z1 = subtractPolynomials(z1, z0);
    Polynomial result(p1.degree() + p2.degree());
    for (int i = 0; i <= z0.degree(); i++) result.coefficients[i] += z0.coefficients[i];
    for (int i = 0; i <= z1.degree(); i++) result.coefficients[i + mid] += z1.coefficients[i];
    for (int i = 0; i <= z2.degree(); i++) result.coefficients[i + 2 * mid] += z2.coefficients[i];
    return result;
}

// MPI helpers for sending/receiving polynomials
void sendPolynomial(const Polynomial& p, int dest, int tag) {
    int deg = p.degree();
    MPI_Send(&deg, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
    if (deg >= 0) MPI_Send((void*)p.coefficients.data(), deg + 1, MPI_INT, dest, tag + 100, MPI_COMM_WORLD);
}

Polynomial recvPolynomial(int src, int tag) {
    MPI_Status status;
    int deg;
    MPI_Recv(&deg, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
    Polynomial p(max(0, deg));
    if (deg >= 0) MPI_Recv(p.coefficients.data(), deg + 1, MPI_INT, src, tag + 100, MPI_COMM_WORLD, &status);
    return p;
}

// Regular MPI multiplication: broadcast polynomials, each rank computes partial rows and reduce
Polynomial multiplyRegularMPI(Polynomial p1, Polynomial p2, int worldRank, int worldSize) {
    int deg1 = p1.degree();
    int deg2 = p2.degree();
    // Broadcast degrees
    MPI_Bcast(&deg1, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&deg2, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (worldRank != 0) {
        p1 = Polynomial(deg1);
        p2 = Polynomial(deg2);
    }
    MPI_Bcast(p1.coefficients.data(), deg1 + 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(p2.coefficients.data(), deg2 + 1, MPI_INT, 0, MPI_COMM_WORLD);

    int resultDegree = deg1 + deg2;
    vector<int> local(resultDegree + 1, 0);

    int rows = deg1 + 1;
    int base = rows / worldSize;
    int rem = rows % worldSize;
    int start = worldRank * base + min(worldRank, rem);
    int cnt = base + (worldRank < rem ? 1 : 0);
    int end = start + cnt;

    for (int i = start; i < end; ++i) {
        for (int j = 0; j <= deg2; ++j) {
            local[i + j] += p1.coefficients[i] * p2.coefficients[j];
        }
    }

    vector<int> result(resultDegree + 1, 0);
    MPI_Reduce(local.data(), result.data(), resultDegree + 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (worldRank == 0) return Polynomial(result);
    return Polynomial(0);
}

// Distributed (one-level) Karatsuba: requires at least 4 processes (root + 3 workers)
Polynomial karatsubaMPI(const Polynomial& p1_in, const Polynomial& p2_in, int worldRank, int worldSize) {
    if (worldSize < 4) {
        if (worldRank == 0) return karatsubaSequential(p1_in, p2_in);
        return Polynomial(0);
    }

    int n = max(p1_in.degree(), p2_in.degree()) + 1;
    int mid = n / 2;

    if (worldRank == 0) {
        Polynomial low1(max(0, mid - 1));
        Polynomial high1(max(0, p1_in.degree() - mid));
        for (int i = 0; i < mid && i <= p1_in.degree(); i++) low1.coefficients[i] = p1_in.coefficients[i];
        for (int i = mid; i <= p1_in.degree(); i++) high1.coefficients[i - mid] = p1_in.coefficients[i];
        Polynomial low2(max(0, mid - 1));
        Polynomial high2(max(0, p2_in.degree() - mid));
        for (int i = 0; i < mid && i <= p2_in.degree(); i++) low2.coefficients[i] = p2_in.coefficients[i];
        for (int i = mid; i <= p2_in.degree(); i++) high2.coefficients[i - mid] = p2_in.coefficients[i];

        Polynomial sum1 = addPolynomials(low1, high1);
        Polynomial sum2 = addPolynomials(low2, high2);

        // Send tasks to ranks 1,2,3
        sendPolynomial(low1, 1, 201);
        sendPolynomial(low2, 1, 202);

        sendPolynomial(high1, 2, 211);
        sendPolynomial(high2, 2, 212);

        sendPolynomial(sum1, 3, 221);
        sendPolynomial(sum2, 3, 222);

        // Receive results
        Polynomial z0 = recvPolynomial(1, 301);
        Polynomial z2 = recvPolynomial(2, 311);
        Polynomial z1 = recvPolynomial(3, 321);

        z1 = subtractPolynomials(z1, z2);
        z1 = subtractPolynomials(z1, z0);

        Polynomial result(p1_in.degree() + p2_in.degree());
        for (int i = 0; i <= z0.degree(); i++) result.coefficients[i] += z0.coefficients[i];
        for (int i = 0; i <= z1.degree(); i++) result.coefficients[i + mid] += z1.coefficients[i];
        for (int i = 0; i <= z2.degree(); i++) result.coefficients[i + 2 * mid] += z2.coefficients[i];
        return result;
    } else if (worldRank == 1) {
        Polynomial a = recvPolynomial(0, 201);
        Polynomial b = recvPolynomial(0, 202);
        Polynomial z = karatsubaSequential(a, b);
        sendPolynomial(z, 0, 301);
        return Polynomial(0);
    } else if (worldRank == 2) {
        Polynomial a = recvPolynomial(0, 211);
        Polynomial b = recvPolynomial(0, 212);
        Polynomial z = karatsubaSequential(a, b);
        sendPolynomial(z, 0, 311);
        return Polynomial(0);
    } else if (worldRank == 3) {
        Polynomial a = recvPolynomial(0, 221);
        Polynomial b = recvPolynomial(0, 222);
        Polynomial z = karatsubaSequential(a, b);
        sendPolynomial(z, 0, 321);
        return Polynomial(0);
    }

    return Polynomial(0);
}

void printUsage(const char* prog) {
    cout << "Usage: " << prog << " <algorithm> <degree> [threads]\n";
    cout << "Algorithms:\n  regular-mpi   - O(n^2) distributed with MPI\n  karatsuba-mpi - Karatsuba distributed (one-level)\n  regular-cpu   - Regular CPU sequential\n  karatsuba-cpu - Karatsuba CPU sequential\n";
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int worldRank, worldSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

    if (argc < 3) {
        if (worldRank == 0) printUsage(argv[0]);
        MPI_Finalize();
        return 1;
    }

    string alg = argv[1];
    int degree = atoi(argv[2]);

    Polynomial p1 = Polynomial::random(degree);
    Polynomial p2 = Polynomial::random(degree);

    Polynomial result;
    auto start = high_resolution_clock::now();

    if (alg == "regular-mpi") {
        if (worldRank == 0) {
            // root will broadcast degs and data inside function
        }
        result = multiplyRegularMPI(p1, p2, worldRank, worldSize);
    } else if (alg == "karatsuba-mpi") {
        result = karatsubaMPI(p1, p2, worldRank, worldSize);
    } else if (alg == "regular-cpu") {
        if (worldRank == 0) result = multiplyRegularSequential(p1, p2);
    } else if (alg == "karatsuba-cpu") {
        if (worldRank == 0) result = karatsubaSequential(p1, p2);
    } else {
        if (worldRank == 0) printUsage(argv[0]);
        MPI_Finalize();
        return 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    auto end = high_resolution_clock::now();

    if (worldRank == 0) {
        auto duration = duration_cast<milliseconds>(end - start).count();
        cout << "Algorithm: " << alg << " | Degree: " << degree << " | Procs: " << worldSize << "\n";
        cout << "Execution time (ms): " << duration << "\n";
    }

    MPI_Finalize();
    return 0;
}
