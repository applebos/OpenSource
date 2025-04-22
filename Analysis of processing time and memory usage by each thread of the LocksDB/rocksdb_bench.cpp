#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <rocksdb/db.h>
#include <rocksdb/options.h>

using namespace rocksdb;
using namespace std;

// 진행률 저장용 전역 변수
vector<atomic<int>> progresses;
atomic<bool> printing_done(false);

// 데이터 크기 포맷 함수
string format_data_size(int bytes) {
    if (bytes < 1024) 
        return to_string(bytes) + " B";
    else if (bytes < 1024 * 1024) 
        return to_string(bytes / 1024) + " KB";
    else 
        return to_string(bytes / (1024 * 1024)) + " MB";
}

// 작업자 스레드
void write_worker(DB* db, int start, int end, int thread_id, int data_size) {
    int total = end - start;
    for (int i = start; i < end; ++i) {
        string key = "key_t" + to_string(thread_id) + "_" + to_string(i);
        string value = "value_" + to_string(i);
        db->Put(WriteOptions(), key, value);

        // 진행률 갱신
        int percent = ((i - start + 1) * 100) / total;
        progresses[thread_id] = percent;
    }
    progresses[thread_id] = 100; // 완료
}

// 진행률 프린터 스레드
void progress_printer(int num_threads, int data_size) {
    const int barWidth = 30;
    while (!printing_done) {
        cout << "\rData: " << format_data_size(data_size) << " ";
        for (int t = 0; t < num_threads; ++t) {
            int percent = progresses[t].load();
            cout << "[T" << t << ": [";
            int pos = barWidth * percent / 100;
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos) cout << "=";
                else if (i == pos) cout << ">";
                else cout << " ";
            }
            cout << "] " << percent << "%] ";
        }
        cout << flush;
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    // 마지막 상태 출력
    cout << "\rData: " << format_data_size(data_size) << " ";
    for (int t = 0; t < num_threads; ++t) {
        int percent = progresses[t].load();
        cout << "[T" << t << ": [";
        int pos = barWidth * percent / 100;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) cout << "=";
            else if (i == pos) cout << ">";
            else cout << " ";
        }
        cout << "] " << percent << "%] ";
    }
    cout << endl;
}


// 메모리 사용량 측정 함수
long get_memory_usage() {
    ifstream status("/proc/self/status");
    string line;
    while (getline(status, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            return stol(line.substr(7)) * 1024; // KB → Bytes
        }
    }
    return 0;
}

int main() {
    system("mkdir -p data");
    
    vector<int> thread_counts = {1, 2, 4, 8};
    vector<int> data_sizes = {10000, 50000, 100000, 200000, 300000, 400000};
    const int num_runs = 10;
    
    ofstream csv("benchmark_results.csv");
    csv << "Threads,DataSize,AvgTime(ms),AvgMemory(Bytes)\n";
    
    // 각 실행 결과 파일
    ofstream csv_runs("benchmark_results_runs.csv");
    csv_runs << "Threads,DataSize,Run,Time(ms),Memory(Bytes)\n";

    for (int threads : thread_counts) {
        for (int data_size : data_sizes) {
            long total_time = 0;
            long total_memory = 0;
            
            for (int run = 0; run < num_runs; ++run) {
                string db_path = "data/rocksdb_t" + to_string(threads) + 
                               "_d" + to_string(data_size) + "_r" + to_string(run);
                
                system(("rm -rf " + db_path).c_str());

                Options options;
                options.create_if_missing = true;
                options.IncreaseParallelism(threads);
                options.OptimizeLevelStyleCompaction();
                
                DB* db;
                DB::Open(options, db_path, &db);

                auto start = chrono::high_resolution_clock::now();
                vector<thread> workers;
                int per_thread = data_size / threads;

                // 진행률 벡터 및 프린터 스레드 준비
                progresses = vector<atomic<int>>(threads);
                for (int t = 0; t < threads; ++t) progresses[t] = 0;
                printing_done = false;
                thread printer(progress_printer, threads, data_size);

                // 메모리 모니터링 스레드
                long max_memory = 0;
                atomic<bool> measuring(true);
                thread memory_monitor([&]() {
                    while (measuring) {
                        max_memory = max(max_memory, get_memory_usage());
                        this_thread::sleep_for(chrono::milliseconds(10));
                    }
                });

                // 작업자 스레드 실행
                for (int t = 0; t < threads; ++t) {
                    int start_idx = t * per_thread;
                    int end_idx = (t == threads-1) ? data_size : start_idx + per_thread;
                    workers.emplace_back(write_worker, db, start_idx, end_idx, t, data_size);
                }

                for (auto& th : workers) th.join();
                measuring = false;
                memory_monitor.join();

                printing_done = true;
                printer.join();

                auto end = chrono::high_resolution_clock::now();
                long duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

                total_time += duration;
                total_memory += max_memory;
                
                // 결과 저장
                total_time += duration;
                total_memory += max_memory;
                csv_runs << threads << "," << data_size << "," << run << "," 
                        << duration << "," << max_memory << "\n";
                
                delete db;
            }

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


