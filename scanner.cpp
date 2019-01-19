#include "scanner.h"
#include <QtCore/QDirIterator>
#include <QtCore/QCryptographicHash>
#include <QtConcurrent/QtConcurrent>
#include <set>
#include <memory>
#include <iostream>
#include <chrono>
#include <thread>

void scanner::update_progress(size_t i, size_t overall_size) {
    int progress = int((i / (double) overall_size) * 100);
    if (progress > current_progress) {
        current_progress = progress;
        emit progress_updated(progress);
    }
}

void scanner::init() {
    trigrams.clear();
    overall_files_count = 0;
    overall_text_files_count = 0;
    current_progress = 0;
    cancel_state = false;
    max_socket_limit_reached = false;
    for (auto& x : text_file_names) {
        watcher.removePath(x);
    }
    connect(&watcher, SIGNAL(fileChanged(const QString&)), this, SLOT(text_file_changed(const QString&)));
}

void scanner::text_file_changed(const QString &filename) {
    QByteArray ba = filename.toUtf8();
    QFile f(filename);
    text_file_names.remove(filename);
    trigrams[ba].clear();
    if (f.exists()) {
        to_trigrams(ba);
    }
    else {
        max_socket_limit_reached = false;
        watcher.removePath(filename);
    }
}

void scanner::cancel() {
    cancel_state = true;
}

void scanner::to_trigrams(const QByteArray &absolute_path) {
    QFile f(absolute_path);
    if (f.open(QFile::ReadOnly)) {
        QByteArray chunk(CHUNK_LEN, ' ');
        QHash<QByteArray, size_t> &current_text_trigrams = trigrams[absolute_path];
        QByteArray trigram_buffer(R"(\\\)");
        while (true) {
            if (cancel_state) return;
            qint64 actual_size = f.read(chunk.data(), CHUNK_LEN);
            if (actual_size == 0 || current_text_trigrams.size() > TEXT_FILE_THRESHOLD) break;
            for (char c: chunk) {
                trigram_buffer[0] = trigram_buffer[1];
                trigram_buffer[1] = trigram_buffer[2];
                trigram_buffer[2] = c;
                current_text_trigrams[trigram_buffer]++;
            }
        }
        if (current_text_trigrams.size() > TEXT_FILE_THRESHOLD) {
            trigrams[absolute_path].clear();
        }
        else {
            text_file_names.insert(absolute_path);
            if (!max_socket_limit_reached && !watcher.addPath(absolute_path)) {
                emit exception_occurred("Cannot watch the file " + dir.relativeFilePath(absolute_path));
                max_socket_limit_reached = true;
            }
        }
    } else {
        throw std::runtime_error("Cannot open the file");
    }
}

void scanner::index() {
    QDirIterator it(dir.path(), QDir::Files, QDirIterator::Subdirectories);
    size_t i = 0;
    while (it.hasNext()) {
        if (cancel_state) return;
        it.next();
        QString rel_path = dir.relativeFilePath(it.fileInfo().path()) + "/";
        if (rel_path == "./") {
            rel_path = "";
        }
        rel_path += it.fileName();
        try {
            to_trigrams(it.fileInfo().absoluteFilePath().toUtf8());
        }
        catch (const std::runtime_error &e) {
            QString message = (QString) e.what() + " " + it.fileName();
            emit exception_occurred(message);
        }
        update_progress(i++, overall_files_count);
    }
    update_progress(overall_files_count, overall_files_count);
}

void scanner::scan(QDir const &dir) {
    this->dir = dir;
    emit info_message("Indexing is started...");
    init();
    overall_files_count = dir.count();
    emit info_message("Collecting information about files...");
    index();
    overall_text_files_count = (uint)text_file_names.size();
    emit all_new_text_files(text_file_names);
    emit info_message("Done! Total number of text files: " + QString::number(overall_text_files_count));
    emit indexing_finished();
}

QHash<QByteArray, size_t> scanner::split_into_trigrams(const QString &s) {
    QHash<QByteArray, size_t> trigrams;
    auto sb = s.toUtf8();
    QByteArray current_trigram = sb.left(3);
    trigrams[current_trigram]++;
    for (int i = 3; i < sb.size(); i++) {
        current_trigram[0] = current_trigram[1];
        current_trigram[1] = current_trigram[2];
        current_trigram[2] = sb[i];
        trigrams[current_trigram]++;
    }
    return trigrams;
}

void scanner::KMP(const QByteArray &S, const QString &pattern, qint64 S_size, vector<int>& result, int start_index) {
    vector<int> pf((unsigned int)pattern.size());

    pf[0] = 0;
    for (int k = 0, i = 1; i < pattern.length(); ++i) {
        while ((k > 0) && (pattern[i] != pattern[k]))
            k = pf[k - 1];
        if (pattern[i] == pattern[k])
            k++;
        pf[i] = k;
    }
    for (int k = 0, i = 0; i < S_size; ++i) {
        while ((k > 0) && (k < pattern.length()) && (pattern[k] != S[i]))
            k = pf[k - 1];
        if (k == pattern.length()) {
            result.push_back(start_index + i - 2 * (unsigned int) pattern.length() + 3);
            k = pf[k - 1];
        }
        //qDebug() << k << " " << i << " " << S_size << " " << pattern.length() << " " << S.length() << " " << pattern.size() << " " << S.size();
        if ((k < pattern.length()) && pattern[k] == S[i])
            k++;
    }
}

vector<int> scanner::find_substr(const scanner::Trigrams &tg, const QString &filename, const QString &needle) {
    //qDebug() << filename;
    vector<int> occurrences;
    auto &file_trigrams = trigrams[filename.toUtf8()];

    if (needle.size() < 3) {
        bool found = false;
        for (auto i = file_trigrams.begin(); i != file_trigrams.end(); i++) {
            if (i.key().contains(needle.toStdString().data())) {
                found = true;
                break;
            }
        }
        if (!found) {
            return occurrences;
        }
    }
    else {
        for (auto i = tg.begin(); i != tg.end(); i++) {
            //qDebug() << trigram.first << " " << trigram.second;
            //qDebug() << file_trigrams[trigram.first];
            if (i.value() > file_trigrams[i.key()]) {
                return occurrences;
            }
        }
    }

    QFile f(filename);
    if (f.open(QFile::ReadOnly)) {
        QByteArray buffer(CHUNK_LEN + needle.size() + 1, '\\');
        int start_index = 0;
        while (true) {
            if (cancel_state) return occurrences;
            for (int i = 0; i < needle.size() - 1; i++) {
                buffer[i] = buffer[buffer.size() - needle.size() + i + 1];
            }
            qint64 actual_size = f.read(buffer.data() + needle.size() - 1, CHUNK_LEN);
            if (actual_size == 0) break;
            //qDebug() << "byte arr: " << buffer.size() << " " << actual_size + needle.size() - 1 << " " << QString(buffer).size();
            //qDebug() << buffer;
            //qDebug() << QString(buffer);
            KMP(buffer, needle, actual_size + needle.size() - 1, occurrences, start_index);
            start_index += actual_size;
        }
    }
    return occurrences;
}

void scanner::search(QString const &needle) {
    current_progress = 0;
    emit info_message("Searching has been started...");

    auto needle_trigrams = split_into_trigrams(needle);
    vector<QFuture<vector<int>>> my_pool;
    vector<QString> thread_file_names;

    size_t counter = 0;
    for (auto& i : text_file_names) {
        if (cancel_state)
            break;
        QFileInfo qFileInfo(i);
        if (qFileInfo.size() > BIG_FILE_THRESHOLD) {
            thread_file_names.push_back(dir.relativeFilePath(i));
             my_pool.push_back(QtConcurrent::run(this, &scanner::find_substr, needle_trigrams,
                    i, needle));
        }
        else {
            auto result = find_substr(needle_trigrams, i, needle);
            if (!result.empty()) {
                emit update_results(dir.relativeFilePath(i), result);
            }
            update_progress(++counter, overall_text_files_count);
        }
    }
    vector<bool> already_finished(my_pool.size());
    size_t finished_threads_counter = 0;
    while (finished_threads_counter != my_pool.size()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (size_t i = 0; i < my_pool.size(); i++) {
            if (my_pool[i].isFinished() && !already_finished[i]) {
                finished_threads_counter++;
                already_finished[i] = true;
                auto result = my_pool[i].result();
                if (!result.empty()) {
                    emit update_results(thread_file_names[i], result);
                }
                update_progress(++counter, overall_text_files_count);
            }
        }
    }
    emit info_message("Searching has finished...");
    emit searching_finished();
    update_progress(overall_text_files_count, overall_text_files_count);
}