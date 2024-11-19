#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <chrono>
/*
*Калинкин Дмитрий Витальевич 21пм 
*Я люблю программировать на C++
*/
// Буфер для хранения данных, которые читаем частями
std::queue<std::string> dataQueue;
std::mutex mtx;
std::condition_variable cv;
bool done = false; // Флаг окончания чтения файла

// Функция для удаления знаков препинания
std::string cleanWord(const std::string& word) {
    std::string cleanedWord;
    cleanedWord.reserve(word.size()); // Предварительное выделение памяти
    for (char c : word) {
        if (!std::ispunct(static_cast<unsigned char>(c))) {
            cleanedWord += std::tolower(c); // Сразу приводим к нижнему регистру
        }
    }
    return cleanedWord;
}

// Поток для обработки данных
void processData(std::unordered_map<std::string, unsigned long long>& wordCount, std::string& longestWord, int& totalWords) {
    std::string localLongestWord;
    std::unordered_map<std::string, unsigned long long> localWordCount;
    int localTotalWords = 0;

    while (true) {
        std::string data;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !dataQueue.empty() || done; }); // Ждем, пока данные появятся в очереди

            if (dataQueue.empty() && done) {
                break; // Завершаем, если все данные прочитаны и очередь пуста
            }

            data = std::move(dataQueue.front());
            dataQueue.pop();
        }

        // Обработка данных (разделение на слова)
        std::istringstream stream(data);
        std::string word;
        while (stream >> word) {
            //word = cleanWord(word);
            localTotalWords++;
            localWordCount[word]++;
            if (word.length() > localLongestWord.length()) {
                localLongestWord = word;
            }
        }
    }

    // Синхронизация и объединение результатов
    {
        std::lock_guard<std::mutex> guard(mtx);
        totalWords += localTotalWords;
        if (localLongestWord.length() > longestWord.length()) {
            longestWord = localLongestWord;
        }
        for (const auto& pair : localWordCount) {
            wordCount[pair.first] += pair.second;
        }
    }
}

// Функция для чтения файла частями с перекрытием
void readFile(const std::string& fileName, std::size_t chunkSize) {
    std::ifstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл!" << std::endl;
        return;
    }

    char* buffer = new char[chunkSize + 100]; // читаем чуть больше (100 байт) для перекрытия
    std::size_t overlapSize = 100; // Размер перекрытия

    while (file.read(buffer, chunkSize) || file.gcount() > 0) {
        std::size_t bytesRead = file.gcount();
        std::string chunk(buffer, bytesRead);

        {
            std::lock_guard<std::mutex> lock(mtx);
            dataQueue.push(std::move(chunk)); // Добавляем прочитанную часть в очередь
        }
        cv.notify_one(); // Уведомляем поток обработки данных
    }

    delete[] buffer;
    file.close();

    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true; // Устанавливаем флаг завершения чтения
    }
    cv.notify_all(); // Уведомляем все потоки, что чтение завершено
}

int main() {
    std::ios::sync_with_stdio(false); // Отключаем синхронизацию потоков ввода-вывода для ускорения
    setlocale(LC_ALL, "RUS");

    std::string fileName = "warandpeace.txt";
    std::size_t chunkSize = 1024 * 1024 * 456; // Размер блока для чтения (например, 64 МБ)
    int numThreads = 5; // Количество потоков обработки

    std::unordered_map<std::string, unsigned long long> wordCount;
    std::string longestWord;
    int totalWords = 0;

    auto start = std::chrono::high_resolution_clock::now();

    // Запускаем N потоков для обработки данных
    std::vector<std::thread> processingThreads;
    for (int i = 0; i < numThreads; ++i) {
        processingThreads.emplace_back(processData, std::ref(wordCount), std::ref(longestWord), std::ref(totalWords));
    }

    // Поток для чтения файла
    std::thread fileThread(readFile, fileName, chunkSize);

    // Ожидаем завершения потока чтения файла
    fileThread.join();

    // Ожидаем завершения всех потоков обработки
    for (auto& th : processingThreads) {
        th.join();
    }

    // Конец замера времени
    auto end = std::chrono::high_resolution_clock::now();

    // Разница во времени
    std::chrono::duration<double> duration = end - start;

    
    for (const auto& pair : wordCount) {
        std::cout << pair.first << ": " << pair.second << std::endl;
    }
    std::cout << "Время выполнения: " << duration.count() << " секунд" << std::endl;
    std::cout << "Общее количество слов: " << totalWords << std::endl;
    std::cout << "Самое длинное слово: " << longestWord << std::endl;
    return 0;
}
