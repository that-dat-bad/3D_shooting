#pragma once
#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <Windows.h>


// 1 : PrewarmPool を無効
// 0 : PrewarmPool を有効
#define BENCHMARK_DISABLE_PREWARM 0

// 1 : 非同期プリロードを無効
// 0 : 非同期プリロードを有効
#define BENCHMARK_DISABLE_ASYNC_PRELOAD 1

namespace Benchmark {
    inline void Log(const std::string& message) {
        // 1. 標準出力
        std::cout << message << std::endl;

        // 2. Visual Studio 出力ウィンドウ
        OutputDebugStringA((message + "\n").c_str());

        // 3. テキストファイルへの自動追記 (benchmark_log.txt)
        static std::mutex fileMutex;
        std::lock_guard<std::mutex> lock(fileMutex);
        static bool isFirstWrite = true;
        
        std::ios_base::openmode mode = isFirstWrite ? std::ios::trunc : std::ios::app;
        std::ofstream ofs("benchmark_log.txt", mode);
        if (ofs.is_open()) {
            if (isFirstWrite) {
                ofs << "========================================================\n";
                ofs << "          BENCHMARK LOG REPORT             \n";
                ofs << "========================================================\n";
                isFirstWrite = false;
            }
            ofs << message << "\n";
            ofs.flush();
        }
    }
}
