#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <random>
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <chrono>
#include <future>

std::mutex queue_mutex;
std::queue<double> shared_queue;
std::atomic<double> total_sum{0.0};
std::atomic<int> total_generated{0};
std::atomic<int> total_processed{0};
std::atomic<bool> all_producers_done{false};

// Función para generar números aleatorios entre [A, B]
double generate_random_number(double A, double B) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(A, B);
    return dis(gen);
}

// Función f(x) que devuelve x^2
double f(double x) {
    return x * x;
}

// Productor que llena la cola con números aleatorios
void producer(double A, double B, int n) {
    while (total_generated.load() < n) {
        double num = generate_random_number(A, B);

        queue_mutex.lock();
        if (total_generated.load() < n) { // Asegurarse de no exceder el límite
            shared_queue.push(num);
            total_generated.fetch_add(1, std::memory_order_relaxed);
        }
        queue_mutex.unlock();
    }
    all_producers_done = true;
}

// Consumidor que procesa los números en la cola
void consumer(int id, std::barrier<>& sync_barrier, int n) {
    double sum = 0;
    while (total_processed.load() < n && (all_producers_done.load() == false || !shared_queue.empty())) {
        queue_mutex.lock();
        if (shared_queue.size() >= 2) {
            double x1 = shared_queue.front();
            shared_queue.pop();
            double x2 = shared_queue.front();
            shared_queue.pop();
            queue_mutex.unlock();

            double fx1 = f(x1);
            if (fx1 <= x2) {
                sum += x1 + x2;
            }
            total_processed.fetch_add(2, std::memory_order_relaxed); // Procesamos dos elementos


        } else {
            queue_mutex.unlock();
        }
    }

    total_sum.fetch_add(sum, std::memory_order_relaxed);
    sync_barrier.arrive_and_wait();
}

int main() {
    // Medimos el tiempo de ejecución
    auto start_time = std::chrono::high_resolution_clock::now();

    double A = 0, B = 1;
    int num_producers = 4;
    int num_consumers = 2;
    int iteraciones = 10000;

    std::barrier sync_barrier(num_consumers + 1);

    // Usamos future para lanzar las tareas asincrónicas
    std::vector<std::future<void>> producer_futures;
    for (int i = 0; i < num_producers; ++i) {
        producer_futures.push_back(std::async(std::launch::async, producer, A, B, iteraciones * 2));
    }

    std::vector<std::future<void>> consumer_futures;
    for (int i = 0; i < num_consumers; ++i) {
        consumer_futures.push_back(std::async(std::launch::async, consumer, i + 1, std::ref(sync_barrier), iteraciones));
    }

    // Esperamos que los productores terminen
    for (auto& future : producer_futures) {
        future.get();
    }

    sync_barrier.arrive_and_wait();  // Sincronización entre los consumidores

    // Esperamos que los consumidores terminen
    for (auto& future : consumer_futures) {
        future.get();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;


    if (total_processed.load() > 0) {
        double average = total_sum.load() / iteraciones;
        std::cout << "Promedio final: " << average << std::endl;
    } else {
        std::cout << "No se procesaron elementos suficientes, no se puede calcular el promedio." << std::endl;
    }

    std::cout << "Tiempo de ejecución: " << duration.count() << " segundos" << std::endl;

    return 0;
}
