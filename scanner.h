#include <utility>

#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include <vector>
#include <algorithm>
#include <QDir>
#include <QDebug>
#include <QFuture>
#include <QThread>
#include <map>
#include <atomic>

using std::string;
using std::vector;
using std::map;
using std::pair;

class scanner: public QObject {
    Q_OBJECT

    using Trigrams = map<QString, size_t>;

    QDir dir;
    int current_progress;
    std::atomic_bool cancel_state;
    uint overall_files_count;
    uint overall_text_files_count;
    map<QString, Trigrams> trigrams;
    const size_t TEXT_FILE_THRESHOLD = 20000;
    const qint64 BIG_FILE_THRESHOLD = 512 * 1024;
    const int CHUNK_LEN = 1024 * 8;

    void init();
    void index();
    void to_trigrams(const QString &);
    Trigrams split_into_trigrams(const QString&);
    void update_progress(size_t i, size_t overall_size);
    void KMP(const QString &S, const QString &pattern, qint64 S_size, vector<int>& result, int start_index);
    vector<int> find_substr(const scanner::Trigrams &tg, const QString& filename, const QString& needle);


public:
    void scan(QDir const& dir);
    void search(QString const& needle);

public slots:
    void cancel();

signals:
    void exception_occurred(const QString &message);
    void info_message(const QString& message);
    void progress_updated(int value);
    void indexing_finished();
    void new_text_file(const QString&);
    void searching_finished();
    void update_results(const QString&, const vector<int>&);
};

#endif // SCANNER_H