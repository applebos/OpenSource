#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <rocksdb/db.h>
#include <rocksdb/options.h>

using namespace rocksdb;
using namespace std;

mutex write_mutex;
atomic<long> total_writes(0);

void write_worker(DB* db, int start, int end, int thread_id) {
    for (int i = start; i < end; ++i) {
        string key = "key_t" + to_string(thread_id) + "_" + to_string(i);
        string value = "value_" + to_string(i);
        db->Put(WriteOptions(), key, value);
        total_writes++;
    }
}

long get_memory_usage() {
    ifstream status("/proc/self/status");
    string line;
    while (getline(status, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            return stol(line.substr(7)) * 1024; // KB -> Bytes
        }
    }
    return 0;
}

int main() {
    system("mkdir -p data");
    
    vector<int> thread_counts = {1, 2, 4, 8};
    vector<int> data_sizes = {10000, 50000, 100000, 200000, 300000, 400000};
    const int num_runs = 10;  // 각 테스트 10회 반복
    
    ofstream csv("benchmark_results.csv");
    csv << "Threads,DataSize,AvgTime(ms),AvgMemory(Bytes)\n";

    for (int threads : thread_counts) {
        for (int data_size : data_sizes) {
            long total_time = 0;
            long total_memory = 0;
            
            for (int run = 0; run < num_runs; ++run) {
                string db_path = "data/rocksdb_t" + to_string(threads) + 
                               "_d" + to_string(data_size) + "_r" + to_string(run);
                
                int ret = system(("rm -rf " + db_path).c_str());
                if (ret != 0) {
                    cerr << "Failed to remove directory: " << db_path << endl;
                }

                Options options;
                options.create_if_missing = true;
                options.IncreaseParallelism(threads);
                options.OptimizeLevelStyleCompaction();
                
                DB* db;
                DB::Open(options, db_path, &db);

                total_writes = 0;
                auto start = chrono::high_resolution_clock::now();
                vector<thread> workers;
                int per_thread = data_size / threads;

                // 메모리 모니터링 스레드
                long max_memory = 0;
                atomic<bool> measuring(true);
                thread memory_monitor([&]() {
                    while (measuring) {
                        max_memory = max(max_memory, get_memory_usage());
                        this_thread::sleep_for(10ms);
                    }
                });

                // 작업자 스레드 시작
                for (int t = 0; t < threads; ++t) {
                    int start_idx = t * per_thread;
                    int end_idx = (t == threads-1) ? data_size : start_idx + per_thread;
                    workers.emplace_back(write_worker, db, start_idx, end_idx, t);
                }

                // 작업자 스레드 완료 대기
                for (auto& th : workers) th.join();
                measuring = false;
                memory_monitor.join();

                auto end = chrono::high_resolution_clock::now();
                long duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
                
                total_time += duration;
                total_memory += max_memory;
                
                delete db;
            }

            // 평균 계산
            double avg_time = static_cast<double>(total_time) / num_runs;
            double avg_memory = static_cast<double>(total_memory) / num_runs;
            
            csv << threads << "," << data_size << "," 
                << avg_time << "," << avg_memory << "\n";
        }
    }

    csv.close();
    cout << "Benchmark complete! Results saved to benchmark_results.csv" << endl;
    return 0;
}
