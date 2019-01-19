#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include <QFuture>
#include "scanner.h"

namespace Ui {
class MainWindow;
}

class main_window : public QMainWindow
{
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = 0);
    ~main_window() override;

signals:
    void exception_occurred(const QString &message);
    void cancel_thread();

private slots:
    void select_directory();
    void scan_directory(QString const& dir);
    void show_about_dialog();
    void log_error(const QString &message);
    void log_info(const QString &message);
    void update_progress_bar(int value);
    void cancel_clicked();
    void indexing_finished();
    void new_text_file(const QString&);
    void search_clicked();
    void searching_finished();
    void update_window(const QString& filename, const vector<int>& occurrences);

private:
    std::unique_ptr<Ui::MainWindow> ui;
    scanner s;
    void clear_layout(QLayout * layout);
    QString convert_to_readable_size(qint64 file_size);
    void clear_gui();
    //QList<QColor> get_row_colors(size_t colors_count);
};

#endif // MAINWINDOW_H
