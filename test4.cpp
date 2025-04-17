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

using namespace rocksdb;
using namespace std;

// 쓰기 작업에 대한 동기화 보호를 위한 뮤텍스 객체
std::mutex write_mutex;

// 데이터베이스에 데이터를 쓰는 함수
void write_data(DB* db, int start, int end, int thread_id) {
    // 각 쓰레드는 자신의 범위에 해당하는 데이터를 쓴다
    for (int i = start; i < end; ++i) {
        // 키와 값 생성 (키는 쓰레드 ID와 인덱스를 포함)
        std::string key = "key_" + std::to_string(thread_id) + "_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        
        // 데이터베이스에 쓰기 수행
        Status s = db->Put(WriteOptions(), key, value);
        
        // 쓰기 실패 시 오류 메시지를 출력
        if (!s.ok()) {
            std::lock_guard<std::mutex> lock(write_mutex); // 동기화 보호
            std::cerr << "Write failed at key " << key << ": " << s.ToString() << std::endl;
        }
    }
}

int main() {
    // 결과를 CSV 파일에 저장
    std::ofstream result_file("thread_data_benchmark.csv");
    result_file << "Threads,DataSize,TimeSeconds\n"; // 헤더 작성

    // 테스트할 쓰레드 수와 데이터 크기
    std::vector<int> thread_counts = {1, 2, 4, 8};
    std::vector<int> data_sizes = {10000, 50000, 100000, 150000 ,200000, 250000, 300000, 350000, 400000};

    // 각 데이터 크기에 대해 벤치마크 실행
    for (int total_data : data_sizes) {
        // 각 쓰레드 수에 대해 벤치마크 실행
        for (int threads : thread_counts) {
            // 데이터베이스 파일 경로 설정 (각 쓰레드와 데이터 크기마다 별도의 DB 생성)
            std::string db_path = "data/rocksdb_t" + std::to_string(threads) + "_d" + std::to_string(total_data);
            
            // 이전 DB 파일이 있으면 삭제
            system(("rm -rf " + db_path).c_str());

            // RocksDB 옵션 설정
            Options options;
            options.create_if_missing = true; // DB가 없으면 새로 생성
            options.IncreaseParallelism(threads); // 쓰레드 수에 맞게 병렬 처리 최적화
            options.OptimizeLevelStyleCompaction(); // 레벨 스타일 컴팩션 최적화
            options.allow_mmap_reads = false; // mmap 읽기 비활성화
            options.allow_mmap_writes = false; // mmap 쓰기 비활성화
            options.use_fsync = true; // fsync 사용
            options.bytes_per_sync = 0; // fsync 후 동기화할 바이트 수
            options.enable_pipelined_write = false; // 파이프라인 쓰기 비활성화
            options.compaction_style = kCompactionStyleLevel; // 컴팩션 스타일 설정
            options.compression = kNoCompression; // 압축 비활성화

            DB* db;
            // RocksDB 열기
            Status status = DB::Open(options, db_path, &db);
            if (!status.ok()) {
                std::cerr << "RocksDB open failed: " << status.ToString() << std::endl;
                continue;
            }

            // 벤치마크 시작 시간 기록
            auto start = std::chrono::high_resolution_clock::now();

            // 쓰레드 벡터 생성
            std::vector<std::thread> workers;
            int per_thread = total_data / threads; // 각 쓰레드가 처리할 데이터 크기
            for (int t = 0; t < threads; ++t) {
                // 각 쓰레드는 특정 범위의 데이터를 처리
                int start_idx = t * per_thread;
                int end_idx = (t == threads - 1) ? total_data : (t + 1) * per_thread;
                workers.emplace_back(write_data, db, start_idx, end_idx, t); // 쓰레드 생성
            }

            // 모든 쓰레드가 종료될 때까지 대기
            for (auto& th : workers) {
                th.join();
            }

            // 벤치마크 종료 시간 기록
            auto end = std::chrono::high_resolution_clock::now();
            double duration = std::chrono::duration<double>(end - start).count(); // 소요 시간 계산

            // 결과 출력 및 CSV 파일에 기록
            std::cout << "Threads: " << threads << ", DataSize: " << total_data << " -> Time: " << duration << "s" << std::endl;
            result_file << threads << "," << total_data << "," << duration << "\n";

            // DB 종료
            delete db;
        }
    }

    // 결과 파일 닫기
    result_file.close();
    std::cout << "Benchmark complete! Results saved to 'thread_data_benchmark.csv'" << std::endl;
    return 0;
}
 to 'thread_data_benchmark.csv'" << std::endl;
    return 0;
}
