#include "app/view/transfer_interface.hpp"
#include "app/common/api.hpp"
#include "app/common/config.hpp"
#include "app/common/log.hpp"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QFrame>
#include <QMessageBox>

namespace app {

// ============================================================
// UploadThread
// ============================================================
UploadThread::UploadThread(std::shared_ptr<UploadTask> task,
                           std::shared_ptr<Pan123> pan,
                           QObject* parent)
    : QThread(parent), _task(std::move(task)), _pan(std::move(pan)) {}

void UploadThread::run() {
    auto logger = get_logger("transfer");
    try {
        emit status_updated("上传中");
        logger->info("上传线程启动: {} ({:.2f} MB)",
                    _task->file_name, _task->file_size / 1024.0 / 1024.0);

        int ul_limit = ConfigManager::get_setting<int>("uploadSpeedLimit", 0);
        _pan->set_upload_speed_limit(ul_limit);

        auto progress_cb = [this](int pct) {
            emit progress_updated(pct);
        };

        auto result = _pan->up_load(_task->local_path, progress_cb);
        emit progress_updated(100);
        emit status_updated("已完成");
        emit upload_finished();
    } catch (const std::exception& e) {
        logger->error("上传失败: {}: {}", _task->file_name, e.what());
        emit error_occurred(QString::fromStdString(e.what()));
        emit status_updated("失败");
    }
}

// ============================================================
// DownloadThread
// ============================================================
DownloadThread::DownloadThread(std::shared_ptr<DownloadTask> task,
                               std::shared_ptr<Pan123> pan,
                               QObject* parent)
    : QThread(parent), _task(std::move(task)), _pan(std::move(pan)) {}

void DownloadThread::run() {
    auto logger = get_logger("transfer");
    try {
        emit status_updated("下载中");
        logger->info("下载线程启动: {}, file_id={}, size={:.2f} MB",
                    _task->file_name, _task->file_id,
                    _task->file_size / 1024.0 / 1024.0);

        bool multi_thread = ConfigManager::get_setting<bool>("multiThreadDownload", true);
        int dl_limit = ConfigManager::get_setting<int>("downloadSpeedLimit", 0);

        _pan->set_download_multi_thread(multi_thread);
        _pan->set_download_speed_limit(dl_limit);

        // Find file info
        json target_file;
        auto [code, files] = _pan->get_dir_by_id(_task->current_dir_id, false, true, 1000);
        if (code == 0) {
            for (auto& f : files) {
                int64_t fid = f.contains("FileId") ? f["FileId"].get<int64_t>()
                                                     : f.value("fileId", 0);
                if (fid == _task->file_id) {
                    target_file = f;
                    int64_t real_size = target_file.contains("Size")
                        ? target_file["Size"].get<int64_t>()
                        : target_file.value("size", 0);
                    if (real_size > 0) _task->file_size = real_size;
                    break;
                }
            }
        }

        if (target_file.is_null()) {
            target_file = {
                {"FileId", _task->file_id},
                {"FileName", _task->file_name},
                {"Type", 0},
                {"Size", _task->file_size},
                {"Etag", ""},
                {"S3KeyFlag", false},
            };
        }

        std::string download_url = _pan->link_by_fileDetail(target_file, false);
        if (download_url.starts_with('-') || download_url.find_first_not_of("0123456789") == std::string::npos) {
            // This is an error code, not a URL
            throw std::runtime_error("获取下载链接失败，返回码: " + download_url);
        }

        std::filesystem::path file_path(_task->save_path);
        auto save_dir = file_path.parent_path();
        if (!std::filesystem::exists(save_dir)) {
            std::filesystem::create_directories(save_dir);
        }

        auto progress_cb = [this](int64_t downloaded, int64_t total) {
            if (total > 0) {
                emit progress_updated(static_cast<int>(downloaded * 100 / total));
            }
        };

        bool success = _pan->download_file(download_url, file_path,
                                            _task->file_size, progress_cb);
        if (!success) {
            throw std::runtime_error("下载失败");
        }

        emit progress_updated(100);
        emit status_updated("已完成");
        emit download_finished();
    } catch (const std::exception& e) {
        logger->error("下载失败: {}: {}", _task->file_name, e.what());
        emit error_occurred(QString::fromStdString(e.what()));
        emit status_updated("失败");
    }
}

// ============================================================
// TransferInterface
// ============================================================
TransferInterface::TransferInterface(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("TransferInterface");

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(24, 20, 24, 24);
    main_layout->setSpacing(12);

    // Top bar
    auto* top_frame = new QFrame(this);
    top_frame->setObjectName("breadcrumbFrame");
    auto* top_layout = new QHBoxLayout(top_frame);

    auto* title_label = new QLabel("传输管理", top_frame);
    title_label->setObjectName("titleLabel");
    top_layout->addWidget(title_label);

    _segment_widget = new QComboBox(top_frame);
    _segment_widget->addItem("上传");
    _segment_widget->addItem("下载");
    top_layout->addWidget(_segment_widget);

    main_layout->addWidget(top_frame);

    // Upload table
    _upload_frame = new QFrame(this);
    auto* upload_layout = new QVBoxLayout(_upload_frame);
    upload_layout->setContentsMargins(0, 8, 0, 0);

    _upload_table = new QTableWidget(0, 6, _upload_frame);
    _upload_table->setHorizontalHeaderLabels({"文件名", "大小", "进度", "百分比", "状态", "操作"});
    _upload_table->setAlternatingRowColors(true);
    _upload_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _upload_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* ul_header = _upload_table->horizontalHeader();
    ul_header->setSectionResizeMode(0, QHeaderView::Stretch);
    ul_header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ul_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ul_header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ul_header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ul_header->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    upload_layout->addWidget(_upload_table);
    main_layout->addWidget(_upload_frame);

    // Download table
    _download_frame = new QFrame(this);
    auto* download_layout = new QVBoxLayout(_download_frame);
    download_layout->setContentsMargins(0, 8, 0, 0);

    _download_table = new QTableWidget(0, 6, _download_frame);
    _download_table->setHorizontalHeaderLabels({"文件名", "大小", "进度", "百分比", "状态", "操作"});
    _download_table->setAlternatingRowColors(true);
    _download_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _download_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* dl_header = _download_table->horizontalHeader();
    dl_header->setSectionResizeMode(0, QHeaderView::Stretch);
    dl_header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    dl_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    dl_header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    dl_header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    dl_header->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    download_layout->addWidget(_download_table);
    _download_frame->hide();
    main_layout->addWidget(_download_frame);

    connect(_segment_widget, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TransferInterface::on_segment_changed);
}

void TransferInterface::set_pan(std::shared_ptr<Pan123> pan) {
    _pan = std::move(pan);
    apply_proxy_settings();
    apply_speed_settings();
}

void TransferInterface::apply_proxy_settings() {
    if (!_pan) return;
    bool enabled = ConfigManager::get_setting<bool>("proxyEnabled", false);
    if (enabled) {
        std::string pt = ConfigManager::get_setting<std::string>("proxyType", "http");
        std::string host = ConfigManager::get_setting<std::string>("proxyHost", "");
        int port = ConfigManager::get_setting<int>("proxyPort", 0);
        if (!host.empty() && port > 0) {
            std::string uname = ConfigManager::get_setting<std::string>("proxyUsername", "");
            std::string pwd = ConfigManager::get_setting<std::string>("proxyPassword", "");
            _pan->set_download_proxy(pt, host, port, uname, pwd);
        }
    } else {
        _pan->clear_download_proxy();
    }
}

void TransferInterface::apply_speed_settings() {
    if (!_pan) return;
    int dl = ConfigManager::get_setting<int>("downloadSpeedLimit", 0);
    int ul = ConfigManager::get_setting<int>("uploadSpeedLimit", 0);
    bool mt = ConfigManager::get_setting<bool>("multiThreadDownload", true);
    _pan->set_download_multi_thread(mt);
    _pan->set_download_speed_limit(dl);
    _pan->set_upload_speed_limit(ul);
}

void TransferInterface::on_segment_changed(int index) {
    if (index == 0) {
        _upload_frame->show();
        _download_frame->hide();
    } else {
        _upload_frame->hide();
        _download_frame->show();
    }
}

void TransferInterface::add_upload_task(const std::string& file_name,
                                         int64_t file_size,
                                         const std::string& local_path,
                                         int64_t target_dir_id) {
    auto task = std::make_shared<UploadTask>();
    task->file_name = file_name;
    task->file_size = file_size;
    task->local_path = local_path;
    task->target_dir_id = target_dir_id;
    _upload_tasks.push_back(task);

    auto logger = get_logger("transfer");
    logger->info("添加上传任务: {} ({:.2f} MB)", file_name,
                file_size / 1024.0 / 1024.0);
    update_table(_upload_table,
                 {_upload_tasks.begin(), _upload_tasks.end()},
                 "upload");

    if (_pan) {
        auto* thread = new UploadThread(task, _pan, this);
        connect(thread, &UploadThread::progress_updated, this,
                [this, task](int p) {
                    task->progress = p;
                    update_table(_upload_table,
                                 {_upload_tasks.begin(), _upload_tasks.end()},
                                 "upload");
                });
        connect(thread, &UploadThread::status_updated, this,
                [this, task](const QString& s) {
                    task->status = s.toStdString();
                    update_table(_upload_table,
                                 {_upload_tasks.begin(), _upload_tasks.end()},
                                 "upload");
                });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    }
}

void TransferInterface::add_download_task(const std::string& file_name,
                                           int64_t file_size,
                                           int64_t file_id,
                                           const std::string& save_path,
                                           int64_t current_dir_id) {
    auto task = std::make_shared<DownloadTask>();
    task->file_name = file_name;
    task->file_size = file_size;
    task->file_id = file_id;
    task->save_path = save_path;
    task->current_dir_id = current_dir_id;
    _download_tasks.push_back(task);

    auto logger = get_logger("transfer");
    logger->info("添加下载任务: {} ({:.2f} MB, id={})", file_name,
                file_size / 1024.0 / 1024.0, file_id);
    update_table(_download_table,
                 {_download_tasks.begin(), _download_tasks.end()},
                 "download");

    if (_pan) {
        auto* thread = new DownloadThread(task, _pan, this);
        connect(thread, &DownloadThread::progress_updated, this,
                [this, task](int p) {
                    task->progress = p;
                    update_table(_download_table,
                                 {_download_tasks.begin(), _download_tasks.end()},
                                 "download");
                });
        connect(thread, &DownloadThread::status_updated, this,
                [this, task](const QString& s) {
                    task->status = s.toStdString();
                    update_table(_download_table,
                                 {_download_tasks.begin(), _download_tasks.end()},
                                 "download");
                });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    }
}

void TransferInterface::update_table(
    QTableWidget* table,
    const std::vector<std::shared_ptr<TransferTask>>& tasks,
    const QString& task_type
) {
    table->setRowCount(static_cast<int>(tasks.size()));

    for (int row = 0; row < static_cast<int>(tasks.size()); ++row) {
        auto& task = tasks[row];

        // File name
        auto* name_item = new QTableWidgetItem(QString::fromStdString(task->file_name));
        table->setItem(row, 0, name_item);

        // File size
        auto* size_item = new QTableWidgetItem(
            QString::fromStdString(format_file_size(task->file_size)));
        table->setItem(row, 1, size_item);

        // Progress bar
        auto* progress_bar = new QProgressBar();
        progress_bar->setRange(0, 100);
        progress_bar->setValue(task->progress);
        progress_bar->setTextVisible(false);
        table->setCellWidget(row, 2, progress_bar);

        // Percentage
        auto* pct_item = new QTableWidgetItem(
            QString("%1%").arg(task->progress));
        pct_item->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 3, pct_item);

        // Status
        auto* status_item = new QTableWidgetItem(
            QString::fromStdString(task->status));
        table->setItem(row, 4, status_item);

        // Delete button
        auto* del_btn = new QPushButton("删除任务");
        del_btn->setFixedSize(100, 24);
        connect(del_btn, &QPushButton::clicked, this,
                [this, task, task_type]() {
                    remove_task(task, task_type);
                });
        table->setCellWidget(row, 5, del_btn);
    }
}

void TransferInterface::remove_task(std::shared_ptr<TransferTask> task,
                                     const QString& task_type) {
    if (task_type == "upload") {
        auto it = std::find_if(_upload_tasks.begin(), _upload_tasks.end(),
                               [&](auto& t) { return t.get() == task.get(); });
        if (it != _upload_tasks.end()) {
            _upload_tasks.erase(it);
        }
        update_table(_upload_table,
                     {_upload_tasks.begin(), _upload_tasks.end()},
                     "upload");
    } else {
        auto it = std::find_if(_download_tasks.begin(), _download_tasks.end(),
                               [&](auto& t) { return t.get() == task.get(); });
        if (it != _download_tasks.end()) {
            _download_tasks.erase(it);
        }
        update_table(_download_table,
                     {_download_tasks.begin(), _download_tasks.end()},
                     "download");
    }
}

}  // namespace app
