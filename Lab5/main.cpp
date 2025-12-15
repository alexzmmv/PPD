#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>
#include <mutex>

using namespace std;
using namespace chrono;

class Polynomial {
public:
    vector<int> coefficients;
    
    Polynomial(int degree = 0) : coefficients(degree + 1, 0) {}
    
    Polynomial(const vector<int>& coef) : coefficients(coef) {}
    
    int degree() const {
        return coefficients.size() - 1;
    }
    
    static Polynomial random(int degree, int minVal = -100, int maxVal = 100) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(minVal, maxVal);
        
        vector<int> coef(degree + 1);
        for (int i = 0; i <= degree; i++) {
            coef[i] = 1;//dis(gen);
        }
        return Polynomial(coef);
    }
    
    void print(int maxTerms = 5) const {
        cout << "Polynomial (degree " << degree() << "): ";
        int terms = min(maxTerms, (int)coefficients.size());
        for (int i = min(degree(), terms - 1); i >= 0; i--) {
            if (i != min(degree(), terms - 1) && coefficients[i] >= 0) cout << "+";
            cout << coefficients[i];
            if (i > 0) cout << "x^" << i << " ";
        }
        if (degree() >= terms) cout << "...";
        cout << endl;
    }
    
    bool equals(const Polynomial& other) const {
        if (degree() != other.degree()) return false;
        for (int i = 0; i <= degree(); i++) {
            if (coefficients[i] != other.coefficients[i]) return false;
        }
        return true;
    }
};

// ==================== REGULAR O(n2) ALGORITHM ====================

Polynomial multiplyRegularSequential(const Polynomial& p1, const Polynomial& p2) {
    int resultDegree = p1.degree() + p2.degree();
    Polynomial result(resultDegree);
    
    for (int i = 0; i <= p1.degree(); i++) {
        for (int j = 0; j <= p2.degree(); j++) {
            result.coefficients[i + j] += p1.coefficients[i] * p2.coefficients[j];
        }
    }
    return result;
}

Polynomial multiplyRegularParallel(const Polynomial& p1, const Polynomial& p2, int numThreads = 4) {
    int resultDegree = p1.degree() + p2.degree();
    Polynomial result(resultDegree);
    
    vector<thread> threads;
    mutex resultMutex;
    
    auto worker = [&](int start, int end) {
        vector<int> localResult(resultDegree + 1, 0);
        
        for (int i = start; i < end; i++) {
            for (int j = 0; j <= p2.degree(); j++) {
                localResult[i + j] += p1.coefficients[i] * p2.coefficients[j];
            }
        }
        
        lock_guard<mutex> lock(resultMutex);
        for (int k = 0; k <= resultDegree; k++) {
            result.coefficients[k] += localResult[k];
        }
    };
    
    int rowsPerThread = (p1.degree() + 1) / numThreads;
    for (int t = 0; t < numThreads; t++) {
        int start = t * rowsPerThread;
        int end = (t == numThreads - 1) ? p1.degree() + 1 : start + rowsPerThread;
        threads.emplace_back(worker, start, end);
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    return result;
}

// ==================== KARATSUBA ALGORITHM ====================

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

Polynomial shiftPolynomial(const Polynomial& p, int shift) {
    Polynomial result(p.degree() + shift);
    for (int i = 0; i <= p.degree(); i++) {
        result.coefficients[i + shift] = p.coefficients[i];
    }
    return result;
}

Polynomial karatsubaSequential(const Polynomial& p1, const Polynomial& p2) {
    // Base case: use regular multiplication for small polynomials
    if (p1.degree() < 10 || p2.degree() < 10) {
        return multiplyRegularSequential(p1, p2);
    }
    
    int n = max(p1.degree(), p2.degree()) + 1;
    int mid = n / 2;
    
    // Split polynomials: p = low + high*x^mid
    Polynomial low1(mid - 1), high1(max(0, p1.degree() - mid));
    for (int i = 0; i < mid && i <= p1.degree(); i++) {
        low1.coefficients[i] = p1.coefficients[i];
    }
    for (int i = mid; i <= p1.degree(); i++) {
        high1.coefficients[i - mid] = p1.coefficients[i];
    }
    
    Polynomial low2(mid - 1), high2(max(0, p2.degree() - mid));
    for (int i = 0; i < mid && i <= p2.degree(); i++) {
        low2.coefficients[i] = p2.coefficients[i];
    }
    for (int i = mid; i <= p2.degree(); i++) {
        high2.coefficients[i - mid] = p2.coefficients[i];
    }
    
    // Three recursive multiplications
    Polynomial z0 = karatsubaSequential(low1, low2);
    Polynomial z2 = karatsubaSequential(high1, high2);
    
    Polynomial sum1 = addPolynomials(low1, high1);
    Polynomial sum2 = addPolynomials(low2, high2);
    Polynomial z1 = karatsubaSequential(sum1, sum2);
    
    // z1 = z1 - z2 - z0
    z1 = subtractPolynomials(z1, z2);
    z1 = subtractPolynomials(z1, z0);
    
    // Combine: result = z0 + z1*x^mid + z2*x^(2*mid)
    Polynomial result(p1.degree() + p2.degree());
    for (int i = 0; i <= z0.degree(); i++) {
        result.coefficients[i] += z0.coefficients[i];
    }
    for (int i = 0; i <= z1.degree(); i++) {
        result.coefficients[i + mid] += z1.coefficients[i];
    }
    for (int i = 0; i <= z2.degree(); i++) {
        result.coefficients[i + 2 * mid] += z2.coefficients[i];
    }
    
    return result;
}

Polynomial karatsubaParallel(const Polynomial& p1, const Polynomial& p2, int depth = 0) {
    // Use sequential for small polynomials or deep recursion
    if (p1.degree() < 10 || p2.degree() < 10 || depth > 3) {
        return karatsubaSequential(p1, p2);
    }
    
    int n = max(p1.degree(), p2.degree()) + 1;
    int mid = n / 2;
    
    // Split polynomials
    Polynomial low1(mid - 1), high1(max(0, p1.degree() - mid));
    for (int i = 0; i < mid && i <= p1.degree(); i++) {
        low1.coefficients[i] = p1.coefficients[i];
    }
    for (int i = mid; i <= p1.degree(); i++) {
        high1.coefficients[i - mid] = p1.coefficients[i];
    }
    
    Polynomial low2(mid - 1), high2(max(0, p2.degree() - mid));
    for (int i = 0; i < mid && i <= p2.degree(); i++) {
        low2.coefficients[i] = p2.coefficients[i];
    }
    for (int i = mid; i <= p2.degree(); i++) {
        high2.coefficients[i - mid] = p2.coefficients[i];
    }
    
    // Parallel execution of three multiplications
    Polynomial z0, z2, z1_temp;
    
    thread t1([&]() { z0 = karatsubaParallel(low1, low2, depth + 1); });
    thread t2([&]() { z2 = karatsubaParallel(high1, high2, depth + 1); });
    
    Polynomial sum1 = addPolynomials(low1, high1);
    Polynomial sum2 = addPolynomials(low2, high2);
    
    thread t3([&]() { z1_temp = karatsubaParallel(sum1, sum2, depth + 1); });
    
    t1.join();
    t2.join();
    t3.join();
    
    // z1 = z1_temp - z2 - z0
    Polynomial z1 = subtractPolynomials(z1_temp, z2);
    z1 = subtractPolynomials(z1, z0);
    
    // Combine results
    Polynomial result(p1.degree() + p2.degree());
    for (int i = 0; i <= z0.degree(); i++) {
        result.coefficients[i] += z0.coefficients[i];
    }
    for (int i = 0; i <= z1.degree(); i++) {
        result.coefficients[i + mid] += z1.coefficients[i];
    }
    for (int i = 0; i <= z2.degree(); i++) {
        result.coefficients[i + 2 * mid] += z2.coefficients[i];
    }
    
    return result;
}

// ==================== MAIN ====================

void printUsage(const char* programName) {
    cout << "Usage: " << programName << " <algorithm> <degree> [threads]\n\n";
    cout << "Algorithms:\n";
    cout << "  1 or regular-seq      - Regular O(n^2) Sequential\n";
    cout << "  2 or regular-par      - Regular O(n^2) Parallel\n";
    cout << "  3 or karatsuba-seq    - Karatsuba O(n^1.58) Sequential\n";
    cout << "  4 or karatsuba-par    - Karatsuba O(n^1.58) Parallel\n\n";
    cout << "Arguments:\n";
    cout << "  degree   - Degree of polynomials (e.g., 1000)\n";
    cout << "  threads  - Number of threads (required for regular-par)\n\n";
    cout << "Examples:\n";
    cout << "  " << programName << " 1 1000\n";
    cout << "  " << programName << " regular-seq 1000\n";
    cout << "  " << programName << " 2 1000 8\n";
    cout << "  " << programName << " regular-par 1000 4\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    string algorithm = argv[1];
    int degree = atoi(argv[2]);
    int numThreads = 4; 
    
    
    // Determine algorithm type
    int algoType = 0;
    if (algorithm == "1" || algorithm == "regular-seq") {
        algoType = 1;
    } else if (algorithm == "2" || algorithm == "regular-par") {
        algoType = 2;
        if (argc < 4) {
            cout << "Error: Regular parallel requires number of threads\n";
            cout << "Usage: " << argv[0] << " regular-par <degree> <threads>\n";
            return 1;
        }
        numThreads = atoi(argv[3]);
    } else if (algorithm == "3" || algorithm == "karatsuba-seq") {
        algoType = 3;
    } else if (algorithm == "4" || algorithm == "karatsuba-par") {
        algoType = 4;
    } else {
        cout << "Error: Unknown algorithm '" << algorithm << "'\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    cout << "=== Polynomial Multiplication ===\n";
    cout << "Degree: " << degree << "\n";
    
    // Generate random polynomials
    cout << "Generating random polynomials...\n";
    Polynomial p1 = Polynomial::random(degree, -10, 10);
    Polynomial p2 = Polynomial::random(degree, -10, 10);
    
    p1.print();
    p2.print();
    cout << endl;
    
    Polynomial result;
    auto start = high_resolution_clock::now();
    
    switch (algoType) {
        case 1:
            cout << "Algorithm: Regular O(n^2) Sequential\n";
            result = multiplyRegularSequential(p1, p2);
            break;
            
        case 2:
            cout << "Algorithm: Regular O(n²) Parallel (" << numThreads << " threads)\n";
            result = multiplyRegularParallel(p1, p2, numThreads);
            break;
            
        case 3:
            cout << "Algorithm: Karatsuba O(n^1.58) Sequential\n";
            result = karatsubaSequential(p1, p2);
            break;
            
        case 4:
            cout << "Algorithm: Karatsuba O(n^1.58) Parallel\n";
            result = karatsubaParallel(p1, p2);
            break;
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    cout << "\nExecution time: " << duration << " ms";
    cout << "\n\n";
    
    cout << "Result polynomial:\n";
    result.print();
    
    return 0;
}
