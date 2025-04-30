#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <unordered_map>
#include <list>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

struct WriterArgs {
    int nj;
    const char* filename;
};

struct ReaderArgs {
    const char* filename;
    int k;
    int N;
};

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_writers_done = PTHREAD_COND_INITIALIZER;
int finished_writers = 0;
bool is_first_write = true;

uint64_t generate_positive_uint64() {
    // return ((static_cast<uint64_t>(rand()) << 32) | rand()) % 5 + 1;
    return (static_cast<uint64_t>(rand()) << 32) | rand();
}

void* writer_thread(void* arg) {
    WriterArgs* args = static_cast<WriterArgs*>(arg);
    std::vector<uint64_t> numbers;
    for (int i = 0; i < args->nj; ++i) {
        numbers.push_back(generate_positive_uint64());
    }

    std::string data;
    for (size_t i = 0; i < numbers.size(); ++i) {
        data += std::to_string(numbers[i]) + ",";
    }

    pthread_mutex_lock(&file_mutex);
    FILE* file = fopen(args->filename, "a");
    if (file) {
        fprintf(file, "%s", data.c_str());
        fclose(file);
    }
    pthread_mutex_unlock(&file_mutex);

    pthread_mutex_lock(&counter_mutex);
    finished_writers++;
    if (finished_writers == args->nj) {
        pthread_cond_signal(&all_writers_done);
    }
    pthread_mutex_unlock(&counter_mutex);

    delete args;
    return nullptr;
}

void* reader_thread(void* arg) {
    ReaderArgs* args = static_cast<ReaderArgs*>(arg);
    std::unordered_map<uint64_t, int> freq_map;
    size_t n = 0;

    FILE* file = fopen(args->filename, "r");
    if (!file) {
        perror("Failed to open file");
        return nullptr;
    }

    char buffer[4096];
    std::string remaining;
    bool all_done = false;

    while (!all_done) {
        pthread_mutex_lock(&counter_mutex);
        all_done = (finished_writers >= args->N);
        pthread_mutex_unlock(&counter_mutex);

        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        if (bytes_read == 0) {
            if (all_done) break;
            usleep(10000);
            continue;
        }
        
        // for(int a = 0; a < bytes_read; a++) {
        //     std::cout << buffer[a];
        // }

        remaining.append(buffer, bytes_read);
        size_t pos;
        while ((pos = remaining.find(',')) != std::string::npos) {
            std::string num_str = remaining.substr(0, pos);
            remaining.erase(0, pos + 1);
            if (num_str.empty()) continue;

            uint64_t num = std::stoull(num_str);
            n++;
            auto it = freq_map.find(num);
            if (it != freq_map.end()) {
                it->second++;
            } else {
                if (freq_map.size() < static_cast<size_t>(args->k - 1)) {
                    freq_map[num] = 1;
                } else {
                    for (auto it = freq_map.begin(); it != freq_map.end(); ) {
                        it->second--;
                        if (it->second == 0) {
                            it = freq_map.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }
    }

    if (!remaining.empty()) {
        uint64_t num = std::stoull(remaining);
        n++;
        auto it = freq_map.find(num);
        if (it != freq_map.end()) {
            it->second++;
        } else {
            if (freq_map.size() < static_cast<size_t>(args->k - 1)) {
                freq_map[num] = 1;
            } else {
                for (auto it = freq_map.begin(); it != freq_map.end(); ) {
                    it->second--;
                    if (it->second == 0) {
                        it = freq_map.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }

    fclose(file);

    const double threshold = static_cast<double>(n) / args->k;
    std::unordered_map<uint64_t, int> precise_counts;
    file = fopen(args->filename, "r");
    if (file) {
        std::string content;
        char buffer[4096];
        while (size_t bytes = fread(buffer, 1, sizeof(buffer), file)) {
            content.append(buffer, bytes);
        }
        fclose(file);

        size_t start = 0;
        while (true) {
            size_t end = content.find(',', start);
            if (end == std::string::npos) {
                if (start < content.size()) {
                    uint64_t num = std::stoull(content.substr(start));
                    if (freq_map.count(num)) {
                        precise_counts[num]++;
                    }
                }
                break;
            }
            uint64_t num = std::stoull(content.substr(start, end - start));
            if (freq_map.count(num)) {
                precise_counts[num]++;
            }
            start = end + 1;
        }
    }

    std::set<uint64_t> T;
    for (const auto& pair : precise_counts) {
        if (pair.second > threshold) {
            T.insert(pair.first);
        }
    }

    std::cout << "Answer: T = {";
    for (auto it = T.begin(); it != T.end(); ++it) {
        if (it != T.begin()) std::cout << ", ";
        std::cout << *it;
    }
    std::cout << "}" << std::endl;

    delete args;
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <N> <k> <filename>" << std::endl;
        return 1;
    }

    int N = std::stoi(argv[1]);
    int k = std::stoi(argv[2]);
    const char* filename = argv[3];

    pthread_t writer_threads[N];
    for (int i = 0; i < N; ++i) {
        WriterArgs* args = new WriterArgs;
        args->nj = 1 + (rand() % 1000000);
        args->filename = filename;
        pthread_create(&writer_threads[i], nullptr, writer_thread, args);
    }

    ReaderArgs* rargs = new ReaderArgs;
    rargs->filename = filename;
    rargs->k = k;
    rargs->N = N;
    pthread_t reader;
    pthread_create(&reader, nullptr, reader_thread, rargs);

    for (int i = 0; i < N; ++i) {
        pthread_join(writer_threads[i], nullptr);
    }
    pthread_join(reader, nullptr);

    return 0;
}
