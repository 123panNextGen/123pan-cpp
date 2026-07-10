#include "app/view/file_interface.hpp"
#include "app/view/transfer_interface.hpp"
#include "app/view/dialogs.hpp"
#include "app/common/api.hpp"
#include "app/common/config.hpp"
#include "app/common/const.hpp"
#include "app/common/log.hpp"
#include "app/tasks/file_tasks.hpp"

#include <QThreadPool>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QScrollBar>

namespace app {

FileInterface::FileInterface(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("FileInterface");

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(24, 20, 24, 24);
    main_layout->setSpacing(12);

    create_top_bar();
    main_layout->addWidget(_breadcrumb_frame);

    create_content();
    main_layout->addLayout(findChild<QHBoxLayout*>("contentLayout"));

    // Context menu for file table
    _file_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_file_table, &QTableWidget::customContextMenuRequested,
            this, &FileInterface::on_context_menu);
}

void FileInterface::set_pan(std::shared_ptr<Pan123> pan) {
    _pan = std::move(pan);
}

void FileInterface::set_transfer_interface(TransferInterface* transfer) {
    _transfer_interface = transfer;
}

void FileInterface::create_top_bar() {
    _breadcrumb_frame = new QFrame(this);
    _breadcrumb_frame->setObjectName("frame");
    auto* top_layout = new QHBoxLayout(_breadcrumb_frame);
    top_layout->setContentsMargins(12, 10, 12, 10);
    top_layout->setSpacing(8);

    _back_button = new QPushButton("← 返回上一级", _breadcrumb_frame);
    _back_button->setEnabled(false);
    connect(_back_button, &QPushButton::clicked,
            this, &FileInterface::go_parent_dir);
    top_layout->addWidget(_back_button);

    // Breadcrumb area
    _breadcrumb_layout = new QHBoxLayout();
    _breadcrumb_layout->setSpacing(2);
    top_layout->addLayout(_breadcrumb_layout);
    top_layout->addStretch();

    _new_folder_button = new QPushButton("新建文件夹", _breadcrumb_frame);
    _upload_button = new QPushButton("上传", _breadcrumb_frame);
    _download_button = new QPushButton("下载", _breadcrumb_frame);
    _delete_button = new QPushButton("删除", _breadcrumb_frame);
    _refresh_button = new QPushButton("刷新", _breadcrumb_frame);

    connect(_new_folder_button, &QPushButton::clicked,
            this, &FileInterface::create_new_folder);
    connect(_upload_button, &QPushButton::clicked,
            this, &FileInterface::upload_file);
    connect(_download_button, &QPushButton::clicked,
            this, &FileInterface::download_file);
    connect(_delete_button, &QPushButton::clicked,
            this, &FileInterface::delete_file);
    connect(_refresh_button, &QPushButton::clicked,
            this, &FileInterface::refresh_file_list);

    top_layout->addWidget(_new_folder_button);
    top_layout->addWidget(_upload_button);
    top_layout->addWidget(_download_button);
    top_layout->addWidget(_delete_button);
    top_layout->addWidget(_refresh_button);
}

void FileInterface::create_content() {
    auto* content_layout = new QHBoxLayout();
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(12);
    content_layout->setObjectName("contentLayout");

    // Left: folder tree
    auto* tree_frame = new QFrame(this);
    tree_frame->setObjectName("frame");
    auto* tree_layout = new QVBoxLayout(tree_frame);
    tree_layout->setContentsMargins(0, 8, 0, 0);
    tree_layout->setSpacing(8);

    _folder_tree = new QTreeWidget(tree_frame);
    _folder_tree->setHeaderHidden(true);
    _folder_tree->setUniformRowHeights(true);
    connect(_folder_tree, &QTreeWidget::itemClicked,
            this, &FileInterface::on_tree_item_clicked);
    connect(_folder_tree, &QTreeWidget::itemExpanded,
            this, &FileInterface::on_tree_item_expanded);
    tree_layout->addWidget(_folder_tree);

    // Storage info card
    auto* storage_card = new QFrame(tree_frame);
    auto* storage_layout = new QVBoxLayout(storage_card);
    storage_layout->setContentsMargins(12, 8, 12, 8);

    auto* storage_top = new QHBoxLayout();
    storage_top->addWidget(new QLabel("☁ 云盘空间", storage_card));
    _storage_value_label = new QLabel("-- / --", storage_card);
    _storage_value_label->setStyleSheet("font-size: 12px; color: gray;");
    storage_top->addWidget(_storage_value_label);
    storage_top->addStretch();
    storage_layout->addLayout(storage_top);

    _storage_progress_bar = new QProgressBar(storage_card);
    _storage_progress_bar->setRange(0, 100);
    _storage_progress_bar->setValue(0);
    _storage_progress_bar->setFixedHeight(6);
    _storage_progress_bar->setTextVisible(false);
    storage_layout->addWidget(_storage_progress_bar);

    tree_layout->addWidget(storage_card);

    content_layout->addWidget(tree_frame, 2);

    // Right: file table
    auto* list_frame = new QFrame(this);
    list_frame->setObjectName("frame");
    auto* list_layout = new QVBoxLayout(list_frame);
    list_layout->setContentsMargins(0, 8, 0, 0);

    _file_table = new QTableWidget(0, 3, list_frame);
    _file_table->setHorizontalHeaderLabels({"名称", "类型", "大小"});
    _file_table->setAlternatingRowColors(true);
    _file_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _file_table->setSelectionMode(QAbstractItemView::SingleSelection);
    _file_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _file_table->verticalHeader()->hide();

    auto* header = _file_table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    connect(header, &QHeaderView::sortIndicatorChanged,
            this, &FileInterface::on_header_sort_changed);

    connect(_file_table, &QTableWidget::itemDoubleClicked,
            this, &FileInterface::on_table_item_double_clicked);

    list_layout->addWidget(_file_table);

    content_layout->addWidget(list_frame, 5);
}

void FileInterface::load_pan_and_data() {
    auto logger = get_logger("ui");
    try {
        init_tree();
        load_current_list();
        update_breadcrumb();
        update_back_button_state();
        load_and_update_storage_info();
    } catch (const std::exception& e) {
        logger->error("初始化失败: {}", e.what());
        _back_button->setEnabled(false);
    }
}

// ============================================================
// Tree management
// ============================================================
void FileInterface::init_tree() {
    _folder_tree->clear();

    auto* root_item = new QTreeWidgetItem({"根目录"});
    root_item->setData(0, Qt::UserRole, QVariant::fromValue<int64_t>(0));
    root_item->setData(0, Qt::UserRole + 1, false);
    _folder_tree->addTopLevelItem(root_item);

    add_placeholder(root_item);
    _folder_tree->expandItem(root_item);
    _folder_tree->setCurrentItem(root_item);
}

void FileInterface::add_placeholder(QTreeWidgetItem* parent) {
    auto* placeholder = new QTreeWidgetItem({""});
    placeholder->setData(0, Qt::UserRole, QVariant());  // null
    parent->addChild(placeholder);
}

void FileInterface::on_tree_item_clicked(QTreeWidgetItem* item, int) {
    auto dir_id_var = item->data(0, Qt::UserRole);
    if (!dir_id_var.isValid()) return;

    int64_t dir_id = dir_id_var.value<int64_t>();
    ensure_tree_children_loaded(item);

    _current_dir_id = dir_id;
    _path_stack = build_path_stack_from_tree(item);
    load_current_list();
    update_breadcrumb();
    update_back_button_state();
}

void FileInterface::on_tree_item_expanded(QTreeWidgetItem* item) {
    ensure_tree_children_loaded(item);
}

void FileInterface::ensure_tree_children_loaded(QTreeWidgetItem* item) {
    if (_is_loading_tree) return;

    bool loaded = item->data(0, Qt::UserRole + 1).toBool();
    auto dir_id_var = item->data(0, Qt::UserRole);
    if (loaded || !dir_id_var.isValid()) return;

    int64_t dir_id = dir_id_var.value<int64_t>();
    _is_loading_tree = true;

    try {
        item->takeChildren();

        auto folder_list = fetch_dir_list(dir_id);
        for (auto& folder : folder_list) {
            if (folder.value("Type", folder.value("type", 0)).toInt() != 1) continue;

            auto* child = new QTreeWidgetItem({folder.value("FileName", folder.value("fileName", "")).toString()});
            int64_t child_id = folder.value("FileId", folder.value("fileId", 0)).toLongLong();
            child->setData(0, Qt::UserRole, QVariant::fromValue<int64_t>(child_id));
            child->setData(0, Qt::UserRole + 1, false);
            item->addChild(child);
            add_placeholder(child);
        }

        item->setData(0, Qt::UserRole + 1, true);
    } catch (...) {}

    _is_loading_tree = false;
}

std::vector<std::pair<int64_t, QString>> FileInterface::build_path_stack_from_tree(
    QTreeWidgetItem* item
) {
    std::vector<std::pair<int64_t, QString>> stack;
    auto* current = item;
    while (current) {
        QString name = current->text(0);
        int64_t dir_id = current->data(0, Qt::UserRole).value<int64_t>();
        stack.insert(stack.begin(), {dir_id, name});
        current = current->parent();
    }
    if (stack.empty()) stack.push_back({0, "根目录"});
    return stack;
}

QTreeWidgetItem* FileInterface::find_tree_item_by_id(int64_t dir_id) {
    QTreeWidgetItemIterator it(_folder_tree);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).value<int64_t>() == dir_id) {
            return *it;
        }
        ++it;
    }
    return nullptr;
}

// ============================================================
// Breadcrumb
// ============================================================
void FileInterface::update_breadcrumb() {
    _is_updating_breadcrumb = true;

    // Clear old buttons
    QLayoutItem* child;
    while ((child = _breadcrumb_layout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    for (auto& [dir_id, name] : _path_stack) {
        auto* btn = new QPushButton(name, _breadcrumb_frame);
        btn->setFlat(true);
        QString route_key = QString::number(dir_id);
        connect(btn, &QPushButton::clicked, this,
                [this, route_key]() { on_breadcrumb_changed(route_key); });
        _breadcrumb_layout->addWidget(btn);

        if (&dir_id != &_path_stack.back().first) {
            _breadcrumb_layout->addWidget(new QLabel(" > ", _breadcrumb_frame));
        }
    }

    _is_updating_breadcrumb = false;
}

void FileInterface::on_breadcrumb_changed(const QString& route_key) {
    if (_is_updating_breadcrumb) return;

    int64_t target_dir_id = route_key.toLongLong();
    int target_index = -1;
    for (size_t i = 0; i < _path_stack.size(); ++i) {
        if (_path_stack[i].first == target_dir_id) {
            target_index = static_cast<int>(i);
            break;
        }
    }

    if (target_index < 0) return;

    _path_stack.resize(target_index + 1);
    _current_dir_id = target_dir_id;
    load_current_list();
    update_back_button_state();

    auto* tree_item = find_tree_item_by_id(target_dir_id);
    if (tree_item) _folder_tree->setCurrentItem(tree_item);
}

void FileInterface::update_back_button_state() {
    _back_button->setEnabled(_path_stack.size() > 1);
}

void FileInterface::go_parent_dir() {
    if (_path_stack.size() <= 1) return;

    _path_stack.pop_back();
    _current_dir_id = _path_stack.back().first;
    load_current_list();
    update_breadcrumb();
    update_back_button_state();

    auto* tree_item = find_tree_item_by_id(_current_dir_id);
    if (tree_item) _folder_tree->setCurrentItem(tree_item);
}

// ============================================================
// File list management
// ============================================================
QList<QMap<QString, QVariant>> FileInterface::fetch_dir_list(int64_t dir_id) {
    if (!_pan) return {};

    auto logger = get_logger("ui");
    logger->debug("获取目录列表: dir_id={}", dir_id);

    int old_page = _pan->file_page;
    int old_total = _pan->total;
    bool old_all = _pan->all_file;

    _pan->file_page = 0;
    try {
        auto [code, items] = _pan->get_dir_by_id(dir_id, false, true, 100);
        QList<QMap<QString, QVariant>> result;
        if (code == 0) {
            for (auto& item_json : items) {
                QMap<QString, QVariant> map;
                for (auto& [key, val] : item_json.items()) {
                    if (val.is_string()) map[QString::fromStdString(key)] = QString::fromStdString(val.get<std::string>());
                    else if (val.is_number_integer()) map[QString::fromStdString(key)] = QVariant::fromValue(val.get<int64_t>());
                    else if (val.is_number_float()) map[QString::fromStdString(key)] = val.get<double>();
                    else if (val.is_boolean()) map[QString::fromStdString(key)] = val.get<bool>();
                }
                result.append(map);
            }
        }
        return result;
    } catch (...) {
        return {};
    }
    _pan->file_page = old_page;
    _pan->total = old_total;
    _pan->all_file = old_all;
}

std::pair<QList<QMap<QString, QVariant>>, QList<QMap<QString, QVariant>>>
FileInterface::reload_dir_data(int64_t dir_id) {
    QList<QMap<QString, QVariant>> items;
    QList<QMap<QString, QVariant>> folder_items;

    int old_page = _pan->file_page;
    int old_total = _pan->total;
    bool old_all = _pan->all_file;

    _pan->file_page = 0;
    try {
        auto [code, raw_items] = _pan->get_dir_by_id(dir_id, false, true, 100);
        if (code == 0) {
            for (auto& item_json : raw_items) {
                QMap<QString, QVariant> map;
                for (auto& [key, val] : item_json.items()) {
                    if (val.is_string()) map[QString::fromStdString(key)] = QString::fromStdString(val.get<std::string>());
                    else if (val.is_number_integer()) map[QString::fromStdString(key)] = QVariant::fromValue(val.get<int64_t>());
                    else if (val.is_boolean()) map[QString::fromStdString(key)] = val.get<bool>();
                }
                items.append(map);
                if (map.value("Type", map.value("type", 0)).toInt() == 1) {
                    folder_items.append(map);
                }
            }
        }
    } catch (...) {}

    _pan->file_page = old_page;
    _pan->total = old_total;
    _pan->all_file = old_all;

    return {items, folder_items};
}

QList<QMap<QString, QVariant>> FileInterface::sort_file_list(
    const QList<QMap<QString, QVariant>>& items
) {
    QList<QMap<QString, QVariant>> folders;
    QList<QMap<QString, QVariant>> files;

    for (auto& item : items) {
        if (item.value("Type", item.value("type", 0)).toInt() == 1) {
            folders.append(item);
        } else {
            files.append(item);
        }
    }

    // Sort by size if needed
    if (_sort_mode == 2) {
        auto sorter = [this](const QMap<QString, QVariant>& a,
                              const QMap<QString, QVariant>& b) -> bool {
            int64_t sa = a.value("Size", a.value("size", 0)).toLongLong();
            int64_t sb = b.value("Size", b.value("size", 0)).toLongLong();
            return _sort_ascending ? (sa < sb) : (sa > sb);
        };
        std::sort(folders.begin(), folders.end(), sorter);
        std::sort(files.begin(), files.end(), sorter);
    }

    QList<QMap<QString, QVariant>> result;
    result.append(folders);
    result.append(files);

    if (_sort_mode == 0 && !_sort_ascending) {
        std::reverse(result.begin(), result.end());
    }

    return result;
}

void FileInterface::load_current_list() {
    if (!_pan) {
        get_logger("ui")->warn("load_current_list: pan 未设置");
        return;
    }

    auto logger = get_logger("ui");
    logger->debug("加载文件列表: dir_id={}", _current_dir_id);

    _file_table->setRowCount(0);

    auto* task = new LoadListTask(
        [this](int64_t dir_id) -> QList<QMap<QString, QVariant>> {
            return fetch_dir_list(dir_id);
        },
        _current_dir_id, this
    );

    connect(task, &LoadListTask::finished,
            this, &FileInterface::on_load_list_finished);
    QThreadPool::globalInstance()->start(task);
}

void FileInterface::on_load_list_finished(
    const QList<QMap<QString, QVariant>>& items,
    const QString& error
) {
    if (!error.isEmpty()) {
        QMessageBox::warning(this, "加载失败",
                           "加载文件列表时发生错误: " + error);
        return;
    }

    auto sorted = sort_file_list(items);
    update_file_list_ui(sorted);
}

void FileInterface::on_header_sort_changed(int logical_index, Qt::SortOrder order) {
    if (logical_index == 0 || logical_index == 2) {
        if (logical_index == static_cast<int>(_sort_mode)) {
            _sort_ascending = !_sort_ascending;
        } else {
            _sort_mode = logical_index;
            _sort_ascending = (logical_index != 2);
        }
        load_current_list();
    }
}

// ============================================================
// Table double-click navigation
// ============================================================
void FileInterface::on_table_item_double_clicked(QTableWidgetItem* item) {
    int row = item->row();
    auto* name_item = _file_table->item(row, 0);
    if (!name_item) return;

    int item_type = name_item->data(Qt::UserRole + 1).toInt();
    if (item_type != 1) return;  // Not a folder

    int64_t file_id = name_item->data(Qt::UserRole).toLongLong();
    QString name = name_item->text();

    _current_dir_id = file_id;
    _path_stack.push_back({file_id, name});
    load_current_list();
    update_breadcrumb();
    update_back_button_state();

    auto* tree_item = find_tree_item_by_id(file_id);
    if (tree_item) {
        _folder_tree->setCurrentItem(tree_item);
        _folder_tree->expandItem(tree_item);
    }
}

// ============================================================
// File list UI update
// ============================================================
void FileInterface::update_file_list_ui(
    const QList<QMap<QString, QVariant>>& file_items
) {
    _file_table->setRowCount(file_items.size());

    for (int row = 0; row < file_items.size(); ++row) {
        auto& fi = file_items[row];
        QString file_name = fi.value("FileName", fi.value("fileName", "")).toString();
        int file_type = fi.value("Type", fi.value("type", 0)).toInt();
        int64_t file_size = fi.value("Size", fi.value("size", 0)).toLongLong();
        int64_t file_id = fi.value("FileId", fi.value("fileId", 0)).toLongLong();

        auto* name_item = new QTableWidgetItem(file_name);
        name_item->setData(Qt::UserRole, QVariant::fromValue<int64_t>(file_id));
        name_item->setData(Qt::UserRole + 1, file_type);

        QString type_text = (file_type == 1) ? "文件夹" : "文件";
        auto* type_item = new QTableWidgetItem(type_text);

        auto* size_item = new QTableWidgetItem(
            QString::fromStdString(format_file_size(file_size)));

        _file_table->setItem(row, 0, name_item);
        _file_table->setItem(row, 1, type_item);
        _file_table->setItem(row, 2, size_item);
    }
}

void FileInterface::update_tree_ui(
    const QList<QMap<QString, QVariant>>& folder_items
) {
    auto* current_item = find_tree_item_by_id(_current_dir_id);
    if (!current_item) return;

    // Remove placeholders
    for (int i = 0; i < current_item->childCount(); ++i) {
        auto* child = current_item->child(i);
        if (!child->data(0, Qt::UserRole).isValid()) {
            current_item->removeChild(child);
            break;
        }
    }

    // Track existing
    QSet<int64_t> existing_ids;
    for (int i = 0; i < current_item->childCount(); ++i) {
        existing_ids.insert(current_item->child(i)->data(0, Qt::UserRole).value<int64_t>());
    }

    // Add new folders
    for (auto& folder : folder_items) {
        int64_t file_id = folder.value("FileId", folder.value("fileId", 0)).toLongLong();
        if (existing_ids.contains(file_id)) continue;

        auto* child = new QTreeWidgetItem({folder.value("FileName", folder.value("fileName", "")).toString()});
        child->setData(0, Qt::UserRole, QVariant::fromValue<int64_t>(file_id));
        child->setData(0, Qt::UserRole + 1, false);
        current_item->addChild(child);
        add_placeholder(child);
    }
}

// ============================================================
// User actions
// ============================================================
void FileInterface::create_new_folder() {
    InputDialog dlg("新建文件夹", "请输入文件夹名称", "新建文件夹", this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString folder_name = dlg.get_input_text();
    if (folder_name.trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入文件夹名称");
        return;
    }

    auto* task = new CreateFolderTask(_pan, folder_name, _current_dir_id, this);
    connect(task, &CreateFolderTask::finished,
            this, &FileInterface::on_operation_finished);
    QThreadPool::globalInstance()->start(task);
}

void FileInterface::upload_file() {
    QStringList file_paths = QFileDialog::getOpenFileNames(this, "选择要上传的文件");
    if (file_paths.isEmpty()) return;

    auto logger = get_logger("ui");
    logger->info("用户选择了 {} 个文件上传", file_paths.size());

    for (auto& fp : file_paths) {
        std::filesystem::path p(fp.toStdString());
        std::string name = p.filename().string();
        int64_t size = std::filesystem::file_size(p);

        if (_transfer_interface) {
            _transfer_interface->add_upload_task(name, size, fp.toStdString(), _current_dir_id);
        }
    }

    QMessageBox::information(this, "上传文件",
                            QString("已添加 %1 个上传任务").arg(file_paths.size()));
}

void FileInterface::download_file() {
    auto selected = _file_table->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "下载错误", "请选择要下载的文件");
        return;
    }

    int row = selected.first()->row();
    auto* name_item = _file_table->item(row, 0);
    int64_t file_id = name_item->data(Qt::UserRole).toLongLong();
    QString file_name = name_item->text();
    int file_type = name_item->data(Qt::UserRole + 1).toInt();

    if (file_type == 1) {
        file_name += ".zip";
    }

    bool ask_location = ConfigManager::get_setting<bool>("askDownloadLocation", true);
    std::string default_path = ConfigManager::get_setting<std::string>(
        "defaultDownloadPath",
        (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / "Downloads").string()
    );

    QString save_path;
    if (!ask_location) {
        save_path = QString::fromStdString(default_path) + "/" + file_name;
    } else {
        save_path = QFileDialog::getSaveFileName(
            this, "保存文件",
            QString::fromStdString(default_path) + "/" + file_name
        );
    }

    if (save_path.isEmpty()) return;

    int64_t file_size = 0;
    for (auto& f : _pan->list) {
        int64_t fid = f.contains("FileId") ? f["FileId"].get<int64_t>()
                                             : f.value("fileId", 0);
        if (fid == file_id) {
            file_size = f.contains("Size") ? f["Size"].get<int64_t>()
                                             : f.value("size", 0);
            break;
        }
    }

    if (_transfer_interface) {
        _transfer_interface->add_download_task(
            file_name.toStdString(), file_size, file_id,
            save_path.toStdString(), _current_dir_id
        );
    }

    QMessageBox::information(this, "下载文件",
                            "已添加下载任务: " + file_name);
}

void FileInterface::delete_file() {
    auto selected = _file_table->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "删除错误", "请选择要删除的文件");
        return;
    }

    int row = selected.first()->row();
    auto* name_item = _file_table->item(row, 0);
    int64_t file_id = name_item->data(Qt::UserRole).toLongLong();
    QString file_name = name_item->text();

    auto* task = new DeleteFileTask(_pan, file_id, file_name, _current_dir_id, this);
    connect(task, &DeleteFileTask::finished,
            this, &FileInterface::on_operation_finished);
    QThreadPool::globalInstance()->start(task);
}

void FileInterface::refresh_file_list() {
    load_current_list();
    load_and_update_storage_info();
}

void FileInterface::rename_file() {
    auto selected = _file_table->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "重命名错误", "请选择要重命名的文件");
        return;
    }

    int row = selected.first()->row();
    auto* name_item = _file_table->item(row, 0);
    int64_t file_id = name_item->data(Qt::UserRole).toLongLong();
    QString old_name = name_item->text();

    InputDialog dlg("重命名", "请输入新的名称", old_name, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString new_name = dlg.get_input_text();
    if (new_name.isEmpty()) {
        QMessageBox::warning(this, "重命名错误", "名称不能为空");
        return;
    }
    if (new_name == old_name) {
        QMessageBox::warning(this, "重命名错误", "新名称与旧名称相同");
        return;
    }

    auto* task = new RenameFileTask(_pan, file_id, old_name, new_name,
                                      _current_dir_id, this);
    connect(task, &RenameFileTask::finished,
            this, &FileInterface::on_operation_finished);
    QThreadPool::globalInstance()->start(task);
}

void FileInterface::on_operation_finished(
    bool success, const QString& name1, const QString& name2,
    const QString& error,
    const QList<QMap<QString, QVariant>>& file_items,
    const QList<QMap<QString, QVariant>>& folder_items
) {
    if (success) {
        if (!file_items.isEmpty()) {
            auto sorted = sort_file_list(file_items);
            update_file_list_ui(sorted);
        }
        if (!folder_items.isEmpty()) {
            update_tree_ui(folder_items);
        }
        auto* tree_item = find_tree_item_by_id(_current_dir_id);
        if (tree_item) _folder_tree->setCurrentItem(tree_item);
    } else {
        if (!error.isEmpty()) {
            QMessageBox::warning(this, "操作失败", error);
        }
    }
}

// ============================================================
// Context menu
// ============================================================
void FileInterface::on_context_menu(const QPoint& pos) {
    QModelIndex index = _file_table->indexAt(pos);
    if (!index.isValid()) return;

    _file_table->selectRow(index.row());

    QMenu menu(this);
    menu.addAction("获取下载链接", this, &FileInterface::copy_download_link);
    menu.addAction("分享", this, &FileInterface::share_file);
    menu.addAction("重命名", this, &FileInterface::rename_file);
    menu.addAction("删除", this, &FileInterface::delete_file);
    menu.exec(_file_table->mapToGlobal(pos));
}

void FileInterface::copy_download_link() {
    auto selected = _file_table->selectedItems();
    if (selected.isEmpty()) return;

    int row = selected.first()->row();
    auto* name_item = _file_table->item(row, 0);
    int64_t file_id = name_item->data(Qt::UserRole).toLongLong();

    json file_detail;
    for (auto& item : _pan->list) {
        int64_t fid = item.contains("FileId") ? item["FileId"].get<int64_t>()
                                                : item.value("fileId", 0);
        if (fid == file_id) {
            file_detail = item;
            break;
        }
    }

    if (file_detail.is_null()) {
        QMessageBox::warning(this, "错误", "无法找到文件详情");
        return;
    }

    try {
        std::string url = _pan->link_by_fileDetail(file_detail, false);
        if (!url.empty() && url[0] != '-') {
            QApplication::clipboard()->setText(QString::fromStdString(url));
            QMessageBox::information(this, "复制成功", "已复制下载链接到剪贴板");
        } else {
            QMessageBox::warning(this, "错误", "获取下载链接失败");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", QString("发生错误: ") + e.what());
    }
}

void FileInterface::share_file() {
    auto selected = _file_table->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "分享失败", "请选择一个文件或文件夹");
        return;
    }

    int row = selected.first()->row();
    auto* name_item = _file_table->item(row, 0);
    int64_t file_id = name_item->data(Qt::UserRole).toLongLong();

    bool ok;
    QString pwd = QInputDialog::getText(
        this, "设置分享密码(可选)", "分享密码 (留空则无密码):",
        QLineEdit::Normal, "", &ok
    );

    if (!ok) return;

    try {
        std::string share_url = _pan->share({file_id}, pwd.toStdString());
        if (!share_url.empty()) {
            QApplication::clipboard()->setText(QString::fromStdString(share_url));
            QMessageBox::information(this, "分享成功",
                                    "已生成分享链接并复制到剪贴板：\n" +
                                    QString::fromStdString(share_url));
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "分享失败", e.what());
    }
}

// ============================================================
// Storage info
// ============================================================
void FileInterface::update_storage_info(const QString& used_text) {
    QString used = used_text.isEmpty() ? "0 B" : used_text;
    int64_t max_capacity = MAX_STORAGE_CAPACITY;
    QString total_text = QString::fromStdString(format_file_size(max_capacity));

    // Parse used text for progress bar
    int usage_pct = 0;
    QStringList parts = used.split(' ');
    if (parts.size() == 2) {
        bool ok;
        double value = parts[0].toDouble(&ok);
        if (ok) {
            QString unit = parts[1].toUpper();
            int64_t used_bytes = 0;
            if (unit == "GB") used_bytes = static_cast<int64_t>(value * 1024 * 1024 * 1024);
            else if (unit == "MB") used_bytes = static_cast<int64_t>(value * 1024 * 1024);
            else if (unit == "KB") used_bytes = static_cast<int64_t>(value * 1024);
            else used_bytes = static_cast<int64_t>(value);

            if (max_capacity > 0) {
                usage_pct = static_cast<int>(used_bytes * 100 / max_capacity);
            }
        }
    }

    _storage_progress_bar->setValue(usage_pct);
    _storage_value_label->setText(used + " / " + total_text);
}

QString FileInterface::calculate_total_storage(int64_t dir_id) {
    double total_size_mb = 0.0;
    try {
        auto [code, items] = _pan->get_dir_by_id(dir_id, false, true, 1000);
        if (code == 0 && !items.empty()) {
            for (auto& item : items) {
                int64_t sz = item.contains("Size") ? item["Size"].get<int64_t>()
                                                     : item.value("size", 0);
                total_size_mb += sz / (1024.0 * 1024.0);
            }
        }
    } catch (...) {
        return "0 B";
    }

    int64_t total_bytes = static_cast<int64_t>(total_size_mb * 1024 * 1024);
    return QString::fromStdString(format_file_size(total_bytes));
}

void FileInterface::load_and_update_storage_info() {
    if (!_pan) return;

    auto* task = new StorageTask(this);
    task->_calculate_func = [this]() -> QString {
        return calculate_total_storage(0);
    };
    connect(task, &StorageTask::finished,
            this, &FileInterface::update_storage_info);
    QThreadPool::globalInstance()->start(task);
}

}  // namespace app
