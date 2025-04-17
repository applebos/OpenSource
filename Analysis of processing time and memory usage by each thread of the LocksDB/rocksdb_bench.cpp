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

mutex write_mutex;                 // (미사용) 쓰기 동기화를 위한 mutex (이 코드에서는 atomic으로 대체됨)
atomic<long> total_writes(0);     // 총 write 횟수를 추적하는 atomic 변수

// 쓰레드별 RocksDB write 작업 함수
void write_worker(DB* db, int start, int end, int thread_id) {
    for (int i = start; i < end; ++i) {
        string key = "key_t" + to_string(thread_id) + "_" + to_string(i);  // key 이름 생성
        string value = "value_" + to_string(i);                            // value 생성
        db->Put(WriteOptions(), key, value);                               // RocksDB에 key-value 저장
        total_writes++;                                                   // 쓰기 카운트 증가
    }
}

// 현재 프로세스의 메모리 사용량 조회 (VmRSS 기준)
long get_memory_usage() {
    ifstream status("/proc/self/status");
    string line;
    while (getline(status, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            return stol(line.substr(7)) * 1024; // KB -> Bytes 변환
        }
    }
    return 0;
}

int main() {
    system("mkdir -p data");  // 실험 데이터 저장 디렉터리 생성

    // 실험에 사용할 쓰레드 수, 데이터 사이즈 조합
    vector<int> thread_counts = {1, 2, 4, 8};
    vector<int> data_sizes = {10000, 50000, 100000, 200000, 300000, 400000};
    const int num_runs = 10;  // 각 조합마다 10회 반복

    ofstream csv("benchmark_results.csv");  // 결과를 저장할 CSV 파일 생성
    csv << "Threads,DataSize,AvgTime(ms),AvgMemory(Bytes)\n";  // CSV 헤더

    for (int threads : thread_counts) {
        for (int data_size : data_sizes) {
            long total_time = 0;
            long total_memory = 0;

            for (int run = 0; run < num_runs; ++run) {
                // RocksDB 저장 경로 구성 및 초기화
                string db_path = "data/rocksdb_t" + to_string(threads) + 
                               "_d" + to_string(data_size) + "_r" + to_string(run);
                
                // 이전 DB 디렉토리 제거
                int ret = system(("rm -rf " + db_path).c_str());
                if (ret != 0) {
                    cerr << "Failed to remove directory: " << db_path << endl;
                }

                // RocksDB 옵션 설정
                Options options;
                options.create_if_missing = true;
                options.IncreaseParallelism(threads);            // 병렬 처리 설정
                options.OptimizeLevelStyleCompaction();          // compaction 최적화

                // RocksDB 인스턴스 열기
                DB* db;
                DB::Open(options, db_path, &db);

                total_writes = 0;
                auto start = chrono::high_resolution_clock::now();  // 시간 측정 시작
                vector<thread> workers;
                int per_thread = data_size / threads;               // 쓰레드당 할당량

                // 메모리 측정용 스레드 실행 (최대 메모리 기록)
                long max_memory = 0;
                atomic<bool> measuring(true);
                thread memory_monitor([&]() {
                    while (measuring) {
                        max_memory = max(max_memory, get_memory_usage());
                        this_thread::sleep_for(10ms);
                    }
                });

                // 쓰기 작업자 스레드 실행
                for (int t = 0; t < threads; ++t) {
                    int start_idx = t * per_thread;
                    int end_idx = (t == threads-1) ? data_size : start_idx + per_thread;
                    workers.emplace_back(write_worker, db, start_idx, end_idx, t);
                }

                // 쓰기 스레드 종료 대기
                for (auto& th : workers) th.join();
                measuring = false;        // 메모리 측정 종료
                memory_monitor.join();    // 메모리 측정 스레드 종료

                auto end = chrono::high_resolution_clock::now();  // 시간 측정 종료
                long duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

                total_time += duration;
                total_memory += max_memory;

                delete db;  // RocksDB 닫기
            }

            // 평균값 계산 후 CSV에 기록
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
