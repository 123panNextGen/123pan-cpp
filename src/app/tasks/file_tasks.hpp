#pragma once

#include <QObject>
#include <QRunnable>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QList>

#include <memory>
#include <functional>

namespace app {

class Pan123;
class FileInterface;

/// 异步加载文件列表任务。
class LoadListTask : public QObject, public QRunnable {
    Q_OBJECT
public:
    LoadListTask(std::function<QList<QMap<QString, QVariant>>(int64_t)> fetch_method,
                 int64_t dir_id, QObject* parent = nullptr);

    void run() override;

signals:
    void finished(const QList<QMap<QString, QVariant>>& file_items, const QString& error);

private:
    std::function<QList<QMap<QString, QVariant>>(int64_t)> _fetch_method;
    int64_t _dir_id;
};

/// 异步创建文件夹任务。
class CreateFolderTask : public QObject, public QRunnable {
    Q_OBJECT
public:
    CreateFolderTask(std::shared_ptr<Pan123> pan,
                     const QString& folder_name,
                     int64_t current_dir_id,
                     QObject* parent = nullptr);

    void run() override;

signals:
    void finished(bool success, const QString& folder_name,
                  const QString&, const QString& error,
                  const QList<QMap<QString, QVariant>>& file_items,
                  const QList<QMap<QString, QVariant>>& folder_items);

private:
    std::shared_ptr<Pan123> _pan;
    QString _folder_name;
    int64_t _current_dir_id;
};

/// 异步删除文件任务。
class DeleteFileTask : public QObject, public QRunnable {
    Q_OBJECT
public:
    DeleteFileTask(std::shared_ptr<Pan123> pan,
                   int64_t file_id, const QString& file_name,
                   int64_t current_dir_id,
                   QObject* parent = nullptr);

    void run() override;

signals:
    void finished(bool success, const QString& file_name, const QString&,
                  const QString& error,
                  const QList<QMap<QString, QVariant>>& file_items,
                  const QList<QMap<QString, QVariant>>& folder_items);

private:
    std::shared_ptr<Pan123> _pan;
    int64_t _file_id;
    QString _file_name;
    int64_t _current_dir_id;
};

/// 异步重命名文件任务。
class RenameFileTask : public QObject, public QRunnable {
    Q_OBJECT
public:
    RenameFileTask(std::shared_ptr<Pan123> pan,
                   int64_t file_id, const QString& old_name,
                   const QString& new_name, int64_t current_dir_id,
                   QObject* parent = nullptr);

    void run() override;

signals:
    void finished(bool success, const QString& old_name, const QString& new_name,
                  const QString& error,
                  const QList<QMap<QString, QVariant>>& file_items,
                  const QList<QMap<QString, QVariant>>& folder_items);

private:
    std::shared_ptr<Pan123> _pan;
    int64_t _file_id;
    QString _old_name;
    QString _new_name;
    int64_t _current_dir_id;
};

/// 异步存储统计任务。
class StorageTask : public QObject, public QRunnable {
    Q_OBJECT
public:
    StorageTask(QObject* parent = nullptr);
    void run() override;

signals:
    void finished(const QString& used_text);

private:
    // Storage calculation requires access to Pan123 which is set externally
    std::function<QString()> _calculate_func;
    friend class FileInterface;
};

}  // namespace app
