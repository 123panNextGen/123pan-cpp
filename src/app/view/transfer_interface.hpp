#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QThread>
#include <memory>
#include <vector>

namespace app {

class Pan123;

/// 传输任务基类。
struct TransferTask {
    std::string file_name;
    int64_t file_size;
    int progress = 0;
    std::string status = "等待中";
    virtual ~TransferTask() = default;
};

/// 上传任务。
struct UploadTask : TransferTask {
    std::string local_path;
    int64_t target_dir_id;
};

/// 下载任务。
struct DownloadTask : TransferTask {
    int64_t file_id;
    std::string save_path;
    int64_t current_dir_id;
};

/// 上传线程。
class UploadThread : public QThread {
    Q_OBJECT
public:
    UploadThread(std::shared_ptr<UploadTask> task, std::shared_ptr<Pan123> pan,
                QObject* parent = nullptr);

    void run() override;

signals:
    void progress_updated(int progress);
    void status_updated(const QString& status);
    void upload_finished();
    void error_occurred(const QString& error);

private:
    std::shared_ptr<UploadTask> _task;
    std::shared_ptr<Pan123> _pan;
};

/// 下载线程。
class DownloadThread : public QThread {
    Q_OBJECT
public:
    DownloadThread(std::shared_ptr<DownloadTask> task, std::shared_ptr<Pan123> pan,
                  QObject* parent = nullptr);

    void run() override;

signals:
    void progress_updated(int progress);
    void status_updated(const QString& status);
    void download_finished();
    void error_occurred(const QString& error);

private:
    std::shared_ptr<DownloadTask> _task;
    std::shared_ptr<Pan123> _pan;
};

/// 传输管理页面。
class TransferInterface : public QWidget {
    Q_OBJECT
public:
    explicit TransferInterface(QWidget* parent = nullptr);

    /// 设置 Pan123 实例并应用配置。
    void set_pan(std::shared_ptr<Pan123> pan);

    /// 添加上传任务。
    void add_upload_task(const std::string& file_name, int64_t file_size,
                        const std::string& local_path, int64_t target_dir_id);

    /// 添加下载任务。
    void add_download_task(const std::string& file_name, int64_t file_size,
                          int64_t file_id, const std::string& save_path,
                          int64_t current_dir_id = 0);

private slots:
    void on_segment_changed(int index);

private:
    void update_table(QTableWidget* table,
                     const std::vector<std::shared_ptr<TransferTask>>& tasks,
                     const QString& task_type);
    void remove_task(std::shared_ptr<TransferTask> task, const QString& task_type);
    void apply_proxy_settings();
    void apply_speed_settings();

    QComboBox* _segment_widget;
    QTableWidget* _upload_table;
    QTableWidget* _download_table;
    QWidget* _upload_frame;
    QWidget* _download_frame;

    std::vector<std::shared_ptr<UploadTask>> _upload_tasks;
    std::vector<std::shared_ptr<DownloadTask>> _download_tasks;

    std::shared_ptr<Pan123> _pan;
};

}  // namespace app
