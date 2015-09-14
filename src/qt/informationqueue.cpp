// Copyright (C) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "informationqueue.h"
#include "../ui_interface.h"

#include <forms/ui_informationqueuedialog.h>
#include <forms/ui_usermessagewidget.h>

#include <QWidget>
#include <QDebug>
#include <QTimer>
#include <QVBoxLayout>
#include <QResizeEvent>

namespace {

struct TextItem {
    TextItem() : repeated(0) {}
    QString text;
    int repeated;
};


class MyScrollArea : public QScrollArea {
public:
    MyScrollArea(QWidget *parent) : QScrollArea(parent) {}

    void resizeEvent(QResizeEvent *event) {
        QWidget *viewPort = widget();
        if (viewPort) {
            viewPort->setMinimumSize(QSize(event->size().width() - 5, 0));
            // viewPort->setMaximumSize(QSize(event->size().width() - 5, 1E5));
        }
    }
};
}


InformationQueue::InformationQueue(QWidget *parent)
    : QObject(parent),
    m_parent(parent),
    m_infoDialog(0),
    m_updateScheduled(false),
    m_lastMessageId(0)
{
}

void InformationQueue::addMessage(const UserMessage &incoming)
{
    Q_ASSERT(!incoming.message.isEmpty());
    QList<Message>::Iterator iter = m_messageQueue.begin();
    while (iter != m_messageQueue.end()) {
        const Message &message(*iter);
        if (incoming.flags == message.flags && message.title == incoming.title) {
            QString main(incoming.message);
            if (iter->messages.last() == incoming.message) // if identical, just increase refcount with one.
                main = iter->messages.last();
            iter->messages.append(main);
            UserMessageWidget *widget = message.widget;
            if (widget)
                widget->refresh();
            return;
        }

        ++iter;
    }

    Message m;
    m.flags = incoming.flags;
    m.title = incoming.title;
    m.messages.append(incoming.message);
    m_messageQueue.append(m);

    if (!m_updateScheduled) {
        m_updateScheduled = true;
        QTimer::singleShot(200, this, SLOT(updateUI()));
    }
}



void InformationQueue::updateUI()
{
    m_updateScheduled = false;
    if (m_messageQueue.isEmpty())
        return;
    if (m_infoDialog == 0)
        m_infoDialog = new InformationDialog(this, m_parent);

    for (int i = 0; i < m_messageQueue.count(); ++i) {
        if (m_messageQueue.at(i).widget.isNull()) {
            m_messageQueue[i].id = ++m_lastMessageId;
            UserMessageWidget *widget = new UserMessageWidget(this, m_lastMessageId);
            m_messageQueue[i].widget = widget;
            m_infoDialog->addWidget(widget);
        }
    }

    m_infoDialog->show();
}

void InformationQueue::hideDialog()
{
    if (m_infoDialog)
        m_infoDialog->hide();
}

void InformationQueue::removeMessages(const QSet<int> &messageIds)
{
    for (int i = 0; i < m_messageQueue.count(); ++i) {
        if (messageIds.contains(m_messageQueue.at(i).id)) {
            m_messageQueue.takeAt(i--);
        }
    }
}

UserMessageWidget::UserMessageWidget(InformationQueue *queue, int messageId)
    : m_parent(queue),
      m_messageId(messageId)
{
    m_widget.setupUi(this);
    foreach (const InformationQueue::Message &message, m_parent->m_messageQueue) {
        if (message.id == m_messageId) {
            m_widget.title->setText(QString("<b>%1</b>").arg(message.title));
            break;
        }
    }

    refresh();
}

void UserMessageWidget::refresh()
{
    foreach (const InformationQueue::Message &message, m_parent->m_messageQueue) {
        if (message.id == m_messageId) {
            QList<TextItem> list;
            TextItem last;
            foreach (const QString &text, message.messages) {
                Q_ASSERT(!text.isEmpty());
                if (last.text == text) {
                    last.repeated++;
                } else {
                    list.append(last);
                    last = TextItem();
                    last.text = text;
                }
            }
            if (!last.text.isEmpty())
                list.append(last);

            int i = -1;
            foreach (QObject *object, children()) {
                if (object->isWidgetType()) {
                    if (i++ == -1) // title
                        continue;
                    Q_ASSERT(list.count() > i);
                    const TextItem &textItem = list.at(i);
                    if (object->property("repeat").toInt() == textItem.repeated)
                        continue;
                    // one got added, change text.
                    QLabel *label = qobject_cast<QLabel*>(object);
                    Q_ASSERT(label);
                    label->setText(tr("%1 (repeated %2 times)")
                                   .arg(textItem.text)
                                   .arg(textItem.repeated + 1));
                    label->setProperty("repeat", textItem.repeated);
                }
            }
            while (++i < list.count()) {
                QLabel *message = new QLabel(this);
                message->setWordWrap(true);
                message->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);
                const TextItem &textItem = list.at(i);
                if (textItem.repeated == 0) {
                    message->setText(textItem.text);
                } else {
                    message->setText(tr("%1 (repeated %2 times)")
                                   .arg(textItem.text)
                                   .arg(textItem.repeated));
                    message->setProperty("repeat", textItem.repeated);
                }
                m_widget.mainLayout->addWidget(message);
            }
            return;
        }
    }
}

unsigned int UserMessageWidget::flags() const
{
    foreach (const InformationQueue::Message &message, m_parent->m_messageQueue) {
        if (message.id == m_messageId)
            return message.flags;
    }
    Q_ASSERT(false);
    return 0;
}


InformationDialog::InformationDialog(InformationQueue *queue, QWidget *parent)
    : QDialog(parent),
      m_queue(queue),
      m_infoPage(0),
      m_warningPage(0),
      m_errorPage(0)
{
    Q_ASSERT(queue);
    m_widget.setupUi(this);

    connect (m_widget.closeButtonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(closePressed()));
}

void InformationDialog::closePressed()
{
    m_queue->removeMessages(m_messages);

    // delete tabs
    delete m_infoPage;
    m_infoPage = 0;
    delete m_errorPage;
    m_errorPage = 0;
    delete m_warningPage;
    m_warningPage = 0;

    m_messages.clear();
}

void InformationDialog::addWidget(UserMessageWidget *widget)
{
    m_messages.insert(widget->messageId());


    QScrollArea **parent;
    QStyle::StandardPixmap type;
    QString tabName;
    const unsigned int flags = widget->flags();
    if (flags & CClientUIInterface::ICON_ERROR) {
        parent = &m_errorPage;
        type = QStyle::SP_MessageBoxCritical;
        tabName = tr("Error");
    } else if (flags & CClientUIInterface::ICON_WARNING) {
        parent = &m_warningPage;
        type = QStyle::SP_MessageBoxWarning;
        tabName = tr("Warning");
    } else { // information
        parent = &m_infoPage;
        type = QStyle::SP_MessageBoxInformation;
        tabName = tr("Information");
    }

    if (*parent == 0) {
        QScrollArea *area = new MyScrollArea(m_widget.tabWidget);
        area->setFrameShape(QScrollArea::NoFrame);
        area->setWidgetResizable(true);

        QWidget *viewport = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(viewport);
        viewport->setLayout(layout);

        widget->setParent(viewport);
        layout->addWidget(widget);
        layout->addStretch(1);

        QIcon icon(m_widget.tabWidget->style()->standardIcon(type));
        m_widget.tabWidget->addTab(area, icon, tabName);
        area->setWidget(viewport);
        *parent = area;
    }
    else {
        QWidget *viewport = (*parent)->widget();
        widget->setParent(viewport);
        QBoxLayout *layout = qobject_cast<QBoxLayout*>(viewport->layout());
        Q_ASSERT(layout);
        layout->insertWidget(layout->count() - 2, widget);
    }
}
