#include "app/view/dialogs.hpp"

#include <QWidget>

namespace app {

InputDialog::InputDialog(const QString& title, const QString& hint,
                        const QString& default_text, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    resize(400, 180);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 30, 40, 30);
    layout->setSpacing(20);

    auto* title_label = new QLabel(title, this);
    QFont title_font;
    title_font.setPointSize(16);
    title_font.setBold(true);
    title_label->setFont(title_font);
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    auto* hint_label = new QLabel(hint, this);
    hint_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint_label);

    _input = new QLineEdit(default_text, this);
    _input->selectAll();
    layout->addWidget(_input);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    auto* cancel_button = new QPushButton("取消", this);
    cancel_button->setMinimumWidth(100);
    connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);

    auto* ok_button = new QPushButton("确定", this);
    ok_button->setMinimumWidth(100);
    ok_button->setDefault(true);
    connect(ok_button, &QPushButton::clicked, this, &QDialog::accept);
    connect(_input, &QLineEdit::returnPressed, this, &QDialog::accept);

    button_layout->addWidget(cancel_button);
    button_layout->addWidget(ok_button);
    layout->addLayout(button_layout);
}

QString InputDialog::get_input_text() const {
    return _input->text().trimmed();
}

}  // namespace app
