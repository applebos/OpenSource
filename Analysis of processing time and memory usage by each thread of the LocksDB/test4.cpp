#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/env.h>
#include <rocksdb/utilities/options_util.h>
#include <rocksdb/cache.h>
#include <sstream>
#include <fstream>

using namespace rocksdb;
using namespace std;

// 쓰기 작업 중에 동시성 문제를 방지하기 위한 mutex 선언
std::mutex write_mutex;

// 데이터베이스에 데이터를 쓰는 함수
void write_data(DB* db, int start, int end, int thread_id) {
    for (int i = start; i < end; ++i) {
        // 키와 값을 문자열로 생성
        std::string key = "key_" + std::to_string(thread_id) + "_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        
        // 데이터베이스에 데이터를 쓴다
        Status s = db->Put(WriteOptions(), key, value);
        if (!s.ok()) {
            // 쓰기 실패 시 오류 메시지를 출력
            std::lock_guard<std::mutex> lock(write_mutex);
            std::cerr << "Write failed at key " << key << ": " << s.ToString() << std::endl;
        }
    }
}

// 시스템의 메모리 사용량을 읽어오는 함수 (Linux에서 /proc/self/status 파일을 사용)
long get_memory_usage() {
    ifstream status_file("/proc/self/status");  // 시스템의 메모리 정보 파일 열기
    string line;
    long memory_usage = 0;

    // 파일에서 각 라인을 읽고 "VmRSS:" 라인이 나오면 메모리 사용량을 추출
    while (getline(status_file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            stringstream ss(line);
            string tmp;
            ss >> tmp >> memory_usage; // 'VmRSS'와 실제 메모리 사용량을 읽음
            break;
        }
    }

    return memory_usage; // 메모리 사용량 반환 (KB 단위)
}

int main() {
    // 결과를 저장할 CSV 파일 열기
    std::ofstream result_file("thread_data_benchmark.csv");
    result_file << "Threads,DataSize,TimeSeconds,MemoryUsage(KB)\n";  // CSV 헤더 작성

    std::vector<int> thread_counts = {1, 2, 4, 8};  // 테스트할 스레드 수
    std::vector<int> data_sizes = {10000, 50000, 100000, 150000 ,200000, 250000, 300000, 350000, 400000};  // 테스트할 데이터 크기

    // 각 스레드 수와 데이터 크기에 대해 벤치마크 수행
    for (int threads : thread_counts) {
        for (int total_data : data_sizes) {
            // 각 테스트마다 RocksDB 경로를 다르게 설정
            std::string db_path = "data/rocksdb_t" + std::to_string(threads) + "_d" + std::to_string(total_data);
            system(("rm -rf " + db_path).c_str());  // 이전에 생성된 데이터베이스를 삭제

            // RocksDB 설정 옵션
            Options options;
            options.create_if_missing = true;  // 데이터베이스가 없으면 생성
            options.IncreaseParallelism(threads);  // 병렬 처리 스레드 수 설정
            options.OptimizeLevelStyleCompaction();  // 컴팩션 최적화
            options.allow_mmap_reads = false;  // MMAP 읽기 비활성화
            options.allow_mmap_writes = false;  // MMAP 쓰기 비활성화
            options.use_fsync = true;  // fsync 사용
            options.bytes_per_sync = 0;  // 동기화 시 바이트 크기 설정
            options.enable_pipelined_write = false;  // 파이프라인 쓰기 비활성화
            options.compaction_style = kCompactionStyleLevel;  // 레벨 스타일 컴팩션 사용
            options.compression = kNoCompression;  // 압축 비활성화

            // 데이터베이스 열기
            DB* db;
            Status status = DB::Open(options, db_path, &db);
            if (!status.ok()) {
                std::cerr << "RocksDB open failed: " << status.ToString() << std::endl;
                continue;
            }

            // 벤치마크 시작 시간 기록
            auto start = std::chrono::high_resolution_clock::now();

            // 작업을 여러 스레드로 분할하여 실행
            std::vector<std::thread> workers;
            int per_thread = total_data / threads;  // 각 스레드가 처리할 데이터 수
            for (int t = 0; t < threads; ++t) {
                int start_idx = t * per_thread;
                int end_idx = (t == threads - 1) ? total_data : (t + 1) * per_thread;
                workers.emplace_back(write_data, db, start_idx, end_idx, t);  // 각 스레드에 작업 분배
            }

            // 모든 스레드가 완료될 때까지 대기
            for (auto& th : workers) {
                th.join();
            }

            // 벤치마크 종료 시간 기록
            auto end = std::chrono::high_resolution_clock::now();
            double duration = std::chrono::duration<double>(end - start).count();  // 걸린 시간 계산

            // 메모리 사용량 측정
            long memory_usage = get_memory_usage();  // 메모리 사용량 얻기

            // 결과 출력
            std::cout << "Threads: " << threads << ", DataSize: " << total_data
                      << " -> Time: " << duration << "s, Memory: " << memory_usage << " KB" << std::endl;

            // CSV 파일에 결과 기록
            result_file << threads << "," << total_data << "," << duration << "," << memory_usage << "\n";

            // 데이터베이스 닫기
            delete db;
        }
    }

    // 결과 파일 닫기
    result_file.close();
    std::cout << "Benchmark complete! Results saved to 'thread_data_benchmark.csv'" << std::endl;
    return 0;
}
