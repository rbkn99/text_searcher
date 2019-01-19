#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCommonStyle>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QLabel>
#include <QDebug>
#include <QtWidgets/QTreeWidgetItem>
#include <QtConcurrent/QtConcurrent>
#include <QMetaType>
#include <algorithm>

main_window::main_window(QWidget *parent)
        : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), qApp->desktop()->availableGeometry()));
    ui->treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    QCommonStyle style;
    ui->actionScan_Directory->setIcon(style.standardIcon(QCommonStyle::SP_DialogOpenButton));
    ui->actionExit->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));
    ui->actionAbout->setIcon(style.standardIcon(QCommonStyle::SP_DialogHelpButton));
    ui->progressBar->setRange(0, 100);

    clear_gui();

    connect(ui->actionScan_Directory, &QAction::triggered, this, &main_window::select_directory);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionAbout, &QAction::triggered, this, &main_window::show_about_dialog);

    qRegisterMetaType<vector<QString>>("vector<QString>");
    qRegisterMetaType<vector<int>>("vector<int>");

    connect(this, SIGNAL(exception_occurred(
                                 const QString&)),
            this, SLOT(log_error(
                               const QString&)));

    connect(&s, SIGNAL(exception_occurred(
                               const QString&)),
            this, SLOT(log_error(
                               const QString&)));
    connect(&s, SIGNAL(info_message(
                               const QString&)),
            this, SLOT(log_info(
                               const QString&)));
    connect(&s, SIGNAL(new_text_file(
                               const QString&)),
            this, SLOT(new_text_file(
                               const QString&)));
    connect(&s, SIGNAL(progress_updated(int)),
            this, SLOT(update_progress_bar(int)));
    connect(&s, SIGNAL(indexing_finished()),
            this, SLOT(indexing_finished()));
    connect(&s, SIGNAL(searching_finished()),
            this, SLOT(searching_finished()));
    connect(&s, SIGNAL(update_results(
                               const QString&, const vector<int>&)),
            this, SLOT(update_window(
                               const QString&, const vector<int>&)));
    connect(ui->searchButton, SIGNAL(clicked()), this, SLOT(search_clicked()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancel_clicked()));
    connect(this, SIGNAL(cancel_thread()), &s, SLOT(cancel()));
}

main_window::~main_window() {}

void main_window::select_directory() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory for Scanning",
                                                    QString(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    scan_directory(dir);
}

void main_window::new_text_file(const QString &name) {
    auto *f = new QTreeWidgetItem(ui->treeWidget);
    f->setText(0, name);
    ui->treeWidget->addTopLevelItem(f);
}

void main_window::clear_layout(QLayout *layout) {
    if (!layout)
        return;
    while (auto item = layout->takeAt(0)) {
        delete item->widget();
        clear_layout(item->layout());
    }
}

void main_window::clear_gui() {
    ui->treeWidget->clear();
    clear_layout(ui->verticalLayout);
    ui->progressBar->setValue(0);
    ui->searchButton->setEnabled(false);
}

void main_window::searching_finished() {
    ui->searchButton->setEnabled(true);
}

void main_window::search_clicked() {
    QString text = ui->textEdit->toPlainText();
    const int MAX_STR_LEN = 10000;
    if (text.isEmpty()) {
        emit exception_occurred("Text field mustn't be empty");
        return;
    }
    if (text.size() > MAX_STR_LEN) {
        emit exception_occurred("Text size is too large");
        return;
    }
    ui->searchButton->setEnabled(false);
    clear_gui();
    QtConcurrent::run(&s, &scanner::search, text);
}

void main_window::scan_directory(QString const &dir) {
    emit cancel_thread();
    clear_gui();
    setWindowTitle(QString("Directory - %1").arg(dir));
    QtConcurrent::run(&s, &scanner::scan, dir);
}

void main_window::update_progress_bar(int value) {
    ui->progressBar->setValue(value);
}

void main_window::update_window(const QString &filename, const vector<int> &occurrences) {
    auto *f = new QTreeWidgetItem(ui->treeWidget);
    f->setText(0, filename);
    ui->treeWidget->addTopLevelItem(f);
    auto cp_occur = occurrences;
    std::sort(cp_occur.begin(), cp_occur.end());
    for (const auto &occur: cp_occur) {
        auto *item = new QTreeWidgetItem(f);
        item->setText(1, QString::number(occur));
    }
}

QString main_window::convert_to_readable_size(qint64 file_size) {
    QString abbr[] = {"B", "KB", "MB", "GB", "TB"};
    auto db_size = (double) file_size;
    int i = 0;
    while (db_size / 1024 >= 1) {
        db_size /= 1024;
        i++;
    }
    db_size = int(db_size * 100) / 100.0;
    return QString::number(db_size) + " " + abbr[i];
}

void main_window::indexing_finished() {
    ui->progressBar->setValue(0);
    ui->searchButton->setEnabled(true);
}

void main_window::cancel_clicked() {
    emit cancel_thread();
}

void main_window::log_error(const QString &message) {
    auto label = new QLabel(message);
    label->setStyleSheet("QLabel { color : red; }");
    ui->verticalLayout->addWidget(label);
}

void main_window::log_info(const QString &message) {
    ui->verticalLayout->addWidget(new QLabel(message));
}

void main_window::show_about_dialog() {
    QMessageBox::aboutQt(this);
}
