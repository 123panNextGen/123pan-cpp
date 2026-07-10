#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMenu>
#include <memory>
#include <vector>
#include <utility>

namespace app {

class Pan123;
class TransferInterface;

/// 文件浏览页面。
class FileInterface : public QWidget {
    Q_OBJECT
public:
    explicit FileInterface(QWidget* parent = nullptr);

    /// 设置 Pan123 实例。
    void set_pan(std::shared_ptr<Pan123> pan);

    /// 传递传输界面引用。
    void set_transfer_interface(TransferInterface* transfer);

    /// 加载 Pan123 数据和初始化。
    void load_pan_and_data();

    /// 重新加载指定目录数据（供后台任务调用）。
    std::pair<QList<QMap<QString, QVariant>>, QList<QMap<QString, QVariant>>>
    reload_dir_data(int64_t dir_id);

    /// 计算存储使用量。
    QString calculate_total_storage(int64_t dir_id = 0);

    /// 加载并更新存储信息。
    void load_and_update_storage_info();

public slots:
    void update_storage_info(const QString& used_text = "0 B");

private slots:
    void on_tree_item_clicked(QTreeWidgetItem* item, int column);
    void on_tree_item_expanded(QTreeWidgetItem* item);
    void on_table_item_double_clicked(QTableWidgetItem* item);
    void on_breadcrumb_changed(const QString& route_key);
    void on_header_sort_changed(int logical_index, Qt::SortOrder order);
    void on_context_menu(const QPoint& pos);

    void go_parent_dir();
    void create_new_folder();
    void upload_file();
    void download_file();
    void delete_file();
    void refresh_file_list();
    void rename_file();
    void copy_download_link();
    void share_file();

    void on_load_list_finished(const QList<QMap<QString, QVariant>>& items,
                              const QString& error);
    void on_operation_finished(bool success, const QString& name1,
                               const QString& name2, const QString& error,
                               const QList<QMap<QString, QVariant>>& file_items,
                               const QList<QMap<QString, QVariant>>& folder_items);

private:
    void create_top_bar();
    void create_content();
    void init_tree();
    void load_current_list();
    void update_breadcrumb();
    void update_back_button_state();
    void update_file_list_ui(const QList<QMap<QString, QVariant>>& file_items);
    void update_tree_ui(const QList<QMap<QString, QVariant>>& folder_items);
    QTreeWidgetItem* find_tree_item_by_id(int64_t dir_id);
    void add_placeholder(QTreeWidgetItem* parent);
    void ensure_tree_children_loaded(QTreeWidgetItem* item);
    std::vector<std::pair<int64_t, QString>> build_path_stack_from_tree(QTreeWidgetItem* item);
    QList<QMap<QString, QVariant>> fetch_dir_list(int64_t dir_id);
    QList<QMap<QString, QVariant>> sort_file_list(const QList<QMap<QString, QVariant>>& items);

    // UI elements
    QPushButton* _back_button;
    QPushButton* _new_folder_button;
    QPushButton* _upload_button;
    QPushButton* _download_button;
    QPushButton* _delete_button;
    QPushButton* _refresh_button;

    QTreeWidget* _folder_tree;
    QTableWidget* _file_table;

    QLabel* _storage_value_label;
    QProgressBar* _storage_progress_bar;

    // Breadcrumb uses a QList<QPushButton*> approach for simplicity
    QHBoxLayout* _breadcrumb_layout;
    QFrame* _breadcrumb_frame;

    // State
    std::shared_ptr<Pan123> _pan;
    TransferInterface* _transfer_interface = nullptr;
    int64_t _current_dir_id = 0;
    std::vector<std::pair<int64_t, QString>> _path_stack;
    bool _is_loading_tree = false;
    bool _is_updating_breadcrumb = false;

    // Sort state
    int _sort_mode = 0;   // 0=name, 2=size
    bool _sort_ascending = true;
};

}  // namespace app
