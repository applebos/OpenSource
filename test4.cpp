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

std::mutex write_mutex;

void write_data(DB* db, int start, int end, int thread_id) {
    for (int i = start; i < end; ++i) {
        std::string key = "key_" + std::to_string(thread_id) + "_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        Status s = db->Put(WriteOptions(), key, value);
        if (!s.ok()) {
            std::lock_guard<std::mutex> lock(write_mutex);
            std::cerr << "Write failed at key " << key << ": " << s.ToString() << std::endl;
        }
    }
}

int main() {
    std::ofstream result_file("thread_data_benchmark.csv");
    result_file << "Threads,DataSize,TimeSeconds\n";

    std::vector<int> thread_counts = {1, 2, 4, 8};
    std::vector<int> data_sizes = {10000, 50000, 100000, 150000 ,200000, 250000, 300000, 350000, 400000};

    for (int total_data : data_sizes) {
        for (int threads : thread_counts) {
            std::string db_path = "data/rocksdb_t" + std::to_string(threads) + "_d" + std::to_string(total_data);
            system(("rm -rf " + db_path).c_str());

            Options options;
            options.create_if_missing = true;
            options.IncreaseParallelism(threads);
            options.OptimizeLevelStyleCompaction();
            options.allow_mmap_reads = false;
            options.allow_mmap_writes = false;
            options.use_fsync = true;
            options.bytes_per_sync = 0;
            options.enable_pipelined_write = false;
            options.compaction_style = kCompactionStyleLevel;
            options.compression = kNoCompression;

            DB* db;
            Status status = DB::Open(options, db_path, &db);
            if (!status.ok()) {
                std::cerr << "RocksDB open failed: " << status.ToString() << std::endl;
                continue;
            }

            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::thread> workers;
            int per_thread = total_data / threads;
            for (int t = 0; t < threads; ++t) {
                int start_idx = t * per_thread;
                int end_idx = (t == threads - 1) ? total_data : (t + 1) * per_thread;
                workers.emplace_back(write_data, db, start_idx, end_idx, t);
            }

            for (auto& th : workers) {
                th.join();
            }

            auto end = std::chrono::high_resolution_clock::now();
            double duration = std::chrono::duration<double>(end - start).count();

            std::cout << "Threads: " << threads << ", DataSize: " << total_data << " -> Time: " << duration << "s" << std::endl;
            result_file << threads << "," << total_data << "," << duration << "\n";

            delete db;
        }
    }

    result_file.close();
    std::cout << "Benchmark complete! Results saved to 'thread_data_benchmark.csv'" << std::endl;
    return 0;
}
