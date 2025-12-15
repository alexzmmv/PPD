#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <cstdlib>
#include <ctime>

const int VECTOR_SIZE = 100;  // Size of vectors
const int QUEUE_SIZE = 5;     // Queue size

using namespace std;

class BoundedQueue {
private:
    queue<double> q;
    mutex mtx;
    condition_variable not_full, not_empty;
    size_t capacity;

public:
    BoundedQueue(size_t cap) : capacity(cap) {}

    void enqueue(double value) {
        unique_lock<mutex> lock(mtx);
        not_full.wait(lock, [this]() { return q.size() < capacity; });
        q.push(value);
        not_empty.notify_one();
    }

    double dequeue() {
        unique_lock<mutex> lock(mtx);
        not_empty.wait(lock, [this]() { return !q.empty(); });
        double value = q.front();
        q.pop();
        not_full.notify_one();
        return value;
    }
};

// Producer: computes element-wise products
void producer(const vector<double>& v1, const vector<double>& v2, BoundedQueue& queue) {
    for (size_t i = 0; i < v1.size(); ++i) {
        double product = v1[i] * v2[i];
        queue.enqueue(product);
    }
    queue.enqueue(-1.0); 
}

// Consumer: sums up products
void consumer(BoundedQueue& queue, double& result) {
    result = 0.0;
    while (true) {
        double value = queue.dequeue();
        if (value == -1.0) break; 
        result += value;
    }
}

int main() {
    vector<double> vec1(VECTOR_SIZE);
    vector<double> vec2(VECTOR_SIZE);

    srand(time(nullptr));

    for (int i = 0; i < VECTOR_SIZE; ++i) {
        double r1 = rand() % 10 + 1;
        double r2 = rand() % 10 + 1;
        vec1[i] = 1;
        vec2[i] = i+1;
    }
    
    cout << "Vector 1: ";
    for (const auto& val : vec1)cout << val << " ";
    cout << "\nVector 2: ";
    for (const auto& val : vec2)cout << val << " ";
    cout << "\n";

    BoundedQueue queue(QUEUE_SIZE);
    double result;

    auto start = chrono::high_resolution_clock::now();

    thread prod_thread(producer, ref(vec1), ref(vec2), ref(queue));
    thread cons_thread(consumer, ref(queue), ref(result));

    prod_thread.join();
    cons_thread.join();

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> elapsed = end - start;

    cout << "Scalar product: " << result << "\n";
    cout << "Time elapsed: " << elapsed.count() << " ms\n";

    return 0;
}