#include "app/tasks/file_tasks.hpp"
#include "app/common/api.hpp"
#include "app/common/log.hpp"

#include <QThreadPool>

namespace app {

// ============================================================
// LoadListTask
// ============================================================
LoadListTask::LoadListTask(
    std::function<QList<QMap<QString, QVariant>>(int64_t)> fetch_method,
    int64_t dir_id, QObject* parent)
    : QObject(parent), _fetch_method(std::move(fetch_method)), _dir_id(dir_id) {}

void LoadListTask::run() {
    try {
        auto file_items = _fetch_method(_dir_id);
        emit finished(file_items, QString());
    } catch (const std::exception& e) {
        emit finished({}, QString::fromStdString(e.what()));
    }
}

// ============================================================
// CreateFolderTask
// ============================================================
CreateFolderTask::CreateFolderTask(std::shared_ptr<Pan123> pan,
                                     const QString& folder_name,
                                     int64_t current_dir_id,
                                     QObject* parent)
    : QObject(parent), _pan(std::move(pan)), _folder_name(folder_name),
      _current_dir_id(current_dir_id) {}

void CreateFolderTask::run() {
    auto logger = get_logger("tasks");
    try {
        int64_t current_parent = _pan->parent_file_id;
        _pan->parent_file_id = _current_dir_id;

        auto result = _pan->mkdir(_folder_name.toStdString());
        _pan->parent_file_id = current_parent;

        if (result.has_value()) {
            // Reload directory data
            // Note: _reload_dir_data is a private method on FileInterface
            // We need to signal back to the UI thread
            emit finished(true, _folder_name, QString(), QString(), {}, {});
        } else {
            emit finished(false, _folder_name, QString(), QString(), {}, {});
        }
    } catch (const std::exception& e) {
        logger->error("CreateFolderTask error: {}", e.what());
        emit finished(false, _folder_name, QString(),
                     QString::fromStdString(e.what()), {}, {});
    }
}

// ============================================================
// DeleteFileTask
// ============================================================
DeleteFileTask::DeleteFileTask(std::shared_ptr<Pan123> pan,
                                 int64_t file_id, const QString& file_name,
                                 int64_t current_dir_id,
                                 QObject* parent)
    : QObject(parent), _pan(std::move(pan)), _file_id(file_id),
      _file_name(file_name), _current_dir_id(current_dir_id) {}

void DeleteFileTask::run() {
    auto logger = get_logger("tasks");
    try {
        logger->info("删除文件: name={}, id={}",
                    _file_name.toStdString(), _file_id);

        bool success = false;

        // Search in current list
        for (size_t i = 0; i < _pan->list.size(); ++i) {
            auto& file = _pan->list[i];
            int64_t fid = file.contains("FileId") ? file["FileId"].get<int64_t>()
                                                    : file.value("fileId", 0);
            if (fid == _file_id) {
                _pan->delete_file(static_cast<int>(i), true, true);
                success = true;
                break;
            }
        }

        if (!success) {
            logger->debug("文件未在当前列表中找到，尝试刷新目录");
            auto [code, files] = _pan->get_dir_by_id(_current_dir_id, true, true, 1000);
            if (code == 0) {
                for (size_t i = 0; i < _pan->list.size(); ++i) {
                    auto& file = _pan->list[i];
                    int64_t fid = file.contains("FileId")
                        ? file["FileId"].get<int64_t>()
                        : file.value("fileId", 0);
                    if (fid == _file_id) {
                        _pan->delete_file(static_cast<int>(i), true, true);
                        success = true;
                        break;
                    }
                }
            }
        }

        if (success) {
            logger->debug("删除成功: {}", _file_name.toStdString());
            emit finished(true, _file_name, QString(), QString(), {}, {});
        } else {
            logger->warn("删除失败: 文件未找到 {}", _file_name.toStdString());
            emit finished(false, _file_name, QString(), QString(), {}, {});
        }
    } catch (const std::exception& e) {
        logger->error("删除异常: {}: {}", _file_name.toStdString(), e.what());
        emit finished(false, _file_name, QString(),
                     QString::fromStdString(e.what()), {}, {});
    }
}

// ============================================================
// RenameFileTask
// ============================================================
RenameFileTask::RenameFileTask(std::shared_ptr<Pan123> pan,
                                 int64_t file_id, const QString& old_name,
                                 const QString& new_name, int64_t current_dir_id,
                                 QObject* parent)
    : QObject(parent), _pan(std::move(pan)), _file_id(file_id),
      _old_name(old_name), _new_name(new_name), _current_dir_id(current_dir_id) {}

void RenameFileTask::run() {
    auto logger = get_logger("tasks");
    try {
        logger->info("重命名文件: {} -> {} (id={})",
                    _old_name.toStdString(), _new_name.toStdString(), _file_id);

        bool success = _pan->rename_file(_file_id, _new_name.toStdString());

        if (success) {
            logger->debug("重命名成功: {} -> {}",
                         _old_name.toStdString(), _new_name.toStdString());
            emit finished(true, _old_name, _new_name, QString(), {}, {});
        } else {
            logger->warn("重命名失败: {} -> {}",
                        _old_name.toStdString(), _new_name.toStdString());
            emit finished(false, _old_name, _new_name, "重命名失败", {}, {});
        }
    } catch (const std::exception& e) {
        logger->error("重命名异常: {}: {}", _old_name.toStdString(), e.what());
        emit finished(false, _old_name, _new_name,
                     QString::fromStdString(e.what()), {}, {});
    }
}

// ============================================================
// StorageTask
// ============================================================
StorageTask::StorageTask(QObject* parent)
    : QObject(parent) {}

void StorageTask::run() {
    try {
        if (_calculate_func) {
            QString result = _calculate_func();
            emit finished(result);
        } else {
            emit finished("0 B");
        }
    } catch (const std::exception& e) {
        emit finished("0 B");
    }
}

}  // namespace app
