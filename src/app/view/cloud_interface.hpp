#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <memory>

namespace app {

class Pan123;

/// 云盘信息页面。
class CloudInterface : public QWidget {
    Q_OBJECT
public:
    explicit CloudInterface(QWidget* parent = nullptr);

    /// 设置 Pan123 实例。
    void set_pan(std::shared_ptr<Pan123> pan);

signals:
    void logout_requested();
    void switch_account_requested();

private:
    QLabel* _username_label;
    std::shared_ptr<Pan123> _pan;
};

}  // namespace app
