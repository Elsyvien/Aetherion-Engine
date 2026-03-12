#include "Aetherion/Editor/TabPanelManager.h"

#include <QGraphicsOpacityEffect>
#include <QMainWindow>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSettings>
#include <QSplitter>
#include <QTabBar>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QList>
#include <algorithm>

namespace Aetherion::Editor
{
namespace
{
bool HasNativeChild(QWidget* widget)
{
    if (!widget) {
        return false;
    }

    if (widget->testAttribute(Qt::WA_NativeWindow) ||
        widget->testAttribute(Qt::WA_PaintOnScreen)) {
        return true;
    }

    const auto children =
        widget->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    for (const auto* child : children) {
        if (!child) {
            continue;
        }
        if (child->testAttribute(Qt::WA_NativeWindow) ||
            child->testAttribute(Qt::WA_PaintOnScreen)) {
            return true;
        }
    }

    return false;
}
} // namespace

TabPanelManager::TabPanelManager(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setObjectName("EditorWorkspace");

    m_verticalSplitter = new QSplitter(Qt::Vertical, this);
    m_verticalSplitter->setObjectName("WorkspaceVerticalSplitter");
    m_verticalSplitter->setHandleWidth(5);

    m_horizontalSplitter = new QSplitter(Qt::Horizontal, m_verticalSplitter);
    m_horizontalSplitter->setObjectName("WorkspaceHorizontalSplitter");
    m_horizontalSplitter->setHandleWidth(5);

    m_leftPanel = new QTabWidget(this);
    m_leftPanel->setObjectName("LeftPanelTabs");
    m_leftPanel->setProperty("panelRole", "left");
    m_leftPanel->setTabPosition(QTabWidget::North);
    m_leftPanel->setDocumentMode(true);
    m_leftPanel->setMinimumWidth(220);
    m_leftPanel->setMaximumWidth(600);

    m_centerContainer = new QWidget(this);
    m_centerContainer->setObjectName("ViewportHost");
    auto* centerLayout = new QVBoxLayout(m_centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    m_rightPanel = new QTabWidget(this);
    m_rightPanel->setObjectName("RightPanelTabs");
    m_rightPanel->setProperty("panelRole", "right");
    m_rightPanel->setTabPosition(QTabWidget::North);
    m_rightPanel->setDocumentMode(true);
    m_rightPanel->setMinimumWidth(260);
    m_rightPanel->setMaximumWidth(800);

    m_horizontalSplitter->addWidget(m_leftPanel);
    m_horizontalSplitter->addWidget(m_centerContainer);
    m_horizontalSplitter->addWidget(m_rightPanel);
    m_horizontalSplitter->setStretchFactor(0, 1);
    m_horizontalSplitter->setStretchFactor(1, 3);
    m_horizontalSplitter->setStretchFactor(2, 1);
    m_horizontalSplitter->setCollapsible(0, true);
    m_horizontalSplitter->setCollapsible(1, false);
    m_horizontalSplitter->setCollapsible(2, true);

    m_bottomPanel = new QTabWidget(this);
    m_bottomPanel->setObjectName("BottomPanelTabs");
    m_bottomPanel->setProperty("panelRole", "bottom");
    m_bottomPanel->setTabPosition(QTabWidget::North);
    m_bottomPanel->setDocumentMode(true);
    m_bottomPanel->setMinimumHeight(170);
    m_bottomPanel->setMaximumHeight(500);

    m_verticalSplitter->addWidget(m_horizontalSplitter);
    m_verticalSplitter->addWidget(m_bottomPanel);
    m_verticalSplitter->setStretchFactor(0, 3);
    m_verticalSplitter->setStretchFactor(1, 1);
    m_verticalSplitter->setCollapsible(1, true);

    mainLayout->addWidget(m_verticalSplitter);

    SetupPanelStyle();
    ApplyDefaultPanelState();
    SetupPanelAnimations();
}

TabPanelManager::~TabPanelManager() = default;

void TabPanelManager::CreatePanelGroups(QMainWindow* mainWindow)
{
    if (!mainWindow) {
        return;
    }
    ApplyDefaultPanelState();
}

void TabPanelManager::SetupPanelAnimations()
{
    for (auto* tabWidget : { m_leftPanel, m_rightPanel, m_bottomPanel }) {
        if (!tabWidget) {
            continue;
        }

        connect(tabWidget, &QTabWidget::currentChanged, this,
                [this, tabWidget](int index) {
                    AnimateTabChange(tabWidget, index);
                });
    }
}

void TabPanelManager::AnimateTabChange(QTabWidget* tabWidget, int index)
{
    if (!tabWidget || index < 0) {
        return;
    }

    auto* page = tabWidget->widget(index);
    if (!page || !page->isVisible()) {
        return;
    }

    const QRect endRect = page->geometry();
    if (!endRect.isValid()) {
        return;
    }

    const int offset = 8;
    const QRect startRect = endRect.translated(0, offset);
    page->setGeometry(startRect);

    auto* slide = new QPropertyAnimation(page, "geometry", page);
    slide->setDuration(220);
    slide->setStartValue(startRect);
    slide->setEndValue(endRect);
    slide->setEasingCurve(QEasingCurve::OutCubic);

    if (HasNativeChild(page)) {
        connect(slide, &QPropertyAnimation::finished, page, [page, endRect] {
            page->setGeometry(endRect);
        });
        slide->start(QAbstractAnimation::DeleteWhenStopped);
        return;
    }

    auto* opacityEffect =
        qobject_cast<QGraphicsOpacityEffect*>(page->graphicsEffect());
    if (page->graphicsEffect() && !opacityEffect) {
        connect(slide, &QPropertyAnimation::finished, page, [page, endRect] {
            page->setGeometry(endRect);
        });
        slide->start(QAbstractAnimation::DeleteWhenStopped);
        return;
    }

    if (!opacityEffect) {
        opacityEffect = new QGraphicsOpacityEffect(page);
        page->setGraphicsEffect(opacityEffect);
    }

    opacityEffect->setOpacity(0.0);

    auto* fade = new QPropertyAnimation(opacityEffect, "opacity", page);
    fade->setDuration(180);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);

    auto* group = new QParallelAnimationGroup(page);
    group->addAnimation(fade);
    group->addAnimation(slide);
    connect(group, &QParallelAnimationGroup::finished, page,
            [page, opacityEffect, endRect] {
                page->setGeometry(endRect);
                opacityEffect->setOpacity(1.0);
            });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void TabPanelManager::AddToLeftPanel(QWidget* widget, const QString& tabName)   
{
    if (m_leftPanel && widget) {
        m_leftPanel->addTab(widget, tabName);
    }
}

void TabPanelManager::AddToRightPanel(QWidget* widget, const QString& tabName)
{
    if (m_rightPanel && widget) {
        m_rightPanel->addTab(widget, tabName);
    }
}

void TabPanelManager::AddToBottomPanel(QWidget* widget, const QString& tabName)
{
    if (m_bottomPanel && widget) {
        m_bottomPanel->addTab(widget, tabName);
    }
}

void TabPanelManager::SetCentralWidget(QWidget* widget)
{
    if (!m_centerContainer) {
        return;
    }

    auto* layout = qobject_cast<QVBoxLayout*>(m_centerContainer->layout());
    if (!layout) {
        layout = new QVBoxLayout(m_centerContainer);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    // Remove any existing central widget
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (auto* oldWidget = item->widget()) {
            oldWidget->setParent(nullptr);
        }
        delete item;
    }

    if (widget) {
        widget->setParent(m_centerContainer);
        layout->addWidget(widget);
    }
}

void TabPanelManager::RemoveFromLeftPanel(int index)
{
    if (m_leftPanel && index >= 0 && index < m_leftPanel->count()) {
        m_leftPanel->removeTab(index);
    }
}

void TabPanelManager::RemoveFromRightPanel(int index)
{
    if (m_rightPanel && index >= 0 && index < m_rightPanel->count()) {
        m_rightPanel->removeTab(index);
    }
}

void TabPanelManager::RemoveFromBottomPanel(int index)
{
    if (m_bottomPanel && index >= 0 && index < m_bottomPanel->count()) {
        m_bottomPanel->removeTab(index);
    }
}

void TabPanelManager::SetActiveLeftTab(int index)
{
    if (m_leftPanel) {
        m_leftPanel->setCurrentIndex(index);
    }
}

void TabPanelManager::SetActiveRightTab(int index)
{
    if (m_rightPanel) {
        m_rightPanel->setCurrentIndex(index);
    }
}

void TabPanelManager::SetActiveBottomTab(int index)
{
    if (m_bottomPanel) {
        m_bottomPanel->setCurrentIndex(index);
    }
}

int TabPanelManager::GetActiveLeftTabIndex() const
{
    return m_leftPanel ? m_leftPanel->currentIndex() : -1;
}

int TabPanelManager::GetActiveRightTabIndex() const
{
    return m_rightPanel ? m_rightPanel->currentIndex() : -1;
}

int TabPanelManager::GetActiveBottomTabIndex() const
{
    return m_bottomPanel ? m_bottomPanel->currentIndex() : -1;
}

void TabPanelManager::SetBottomPanelVisible(bool visible)
{
    if (!m_bottomPanel || !m_verticalSplitter) {
        if (m_bottomPanel) {
            m_bottomPanel->setVisible(visible);
        }
        return;
    }

    QList<int> startSizes = m_verticalSplitter->sizes();
    if (startSizes.size() < 2) {
        m_bottomPanel->setVisible(visible);
        return;
    }

    const int total = startSizes[0] + startSizes[1];
    QList<int> endSizes = startSizes;

    if (!visible) {
        endSizes[0] = total;
        endSizes[1] = 0;
    } else {
        if (startSizes[1] == 0) {
            const int bottom = std::max(1, total / 4);
            endSizes[1] = bottom;
            endSizes[0] = std::max(1, total - bottom);
        }
    }

    if (startSizes == endSizes) {
        m_bottomPanel->setVisible(visible);
        m_verticalSplitter->setSizes(endSizes);
        return;
    }

    if (m_bottomPanelAnimation) {
        m_bottomPanelAnimation->stop();
        m_bottomPanelAnimation->deleteLater();
        m_bottomPanelAnimation = nullptr;
    }

    if (visible) {
        m_bottomPanel->setVisible(true);
    }

    auto* animation = new QVariantAnimation(this);
    m_bottomPanelAnimation = animation;
    animation->setDuration(220);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this,
            [this, startSizes, endSizes](const QVariant& value) {
                const double t = value.toDouble();
                QList<int> sizes;
                sizes.reserve(startSizes.size());
                for (int i = 0; i < startSizes.size(); ++i) {
                    const int start = startSizes[i];
                    const int end = endSizes[i];
                    sizes.append(static_cast<int>(start + (end - start) * t));
                }
                m_verticalSplitter->setSizes(sizes);
            });

    connect(animation, &QVariantAnimation::finished, this,
            [this, visible, endSizes] {
                if (m_verticalSplitter) {
                    m_verticalSplitter->setSizes(endSizes);
                }
                if (m_bottomPanel) {
                    m_bottomPanel->setVisible(visible);
                }
                if (m_bottomPanelAnimation) {
                    m_bottomPanelAnimation->deleteLater();
                    m_bottomPanelAnimation = nullptr;
                }
            });

    animation->start();
}

bool TabPanelManager::IsBottomPanelVisible() const
{
    return m_bottomPanel && m_bottomPanel->isVisible();
}

void TabPanelManager::ApplyDefaultPanelState()
{
    if (m_horizontalSplitter) {
        m_horizontalSplitter->setSizes({280, 1080, 360});
    }

    if (m_verticalSplitter) {
        m_verticalSplitter->setSizes({760, 240});
    }

    if (m_leftPanel && m_leftPanel->count() > 0) {
        m_leftPanel->setCurrentIndex(0);
    }
    if (m_rightPanel && m_rightPanel->count() > 0) {
        m_rightPanel->setCurrentIndex(0);
    }
    if (m_bottomPanel && m_bottomPanel->count() > 0) {
        m_bottomPanel->setCurrentIndex(0);
        m_bottomPanel->setVisible(true);
    }
}

void TabPanelManager::SavePanelState(QSettings& settings, const QString& group) const
{
    settings.beginGroup(group);
    
    if (m_verticalSplitter) {
        settings.setValue("VerticalSplitterState", m_verticalSplitter->saveState());
    }
    if (m_horizontalSplitter) {
        settings.setValue("HorizontalSplitterState", m_horizontalSplitter->saveState());
    }

    if (m_leftPanel) {
        settings.setValue("LeftPanelCurrentIndex", m_leftPanel->currentIndex());
    }
    if (m_rightPanel) {
        settings.setValue("RightPanelCurrentIndex", m_rightPanel->currentIndex());
    }
    if (m_bottomPanel) {
        settings.setValue("BottomPanelCurrentIndex", m_bottomPanel->currentIndex());
        settings.setValue("BottomPanelVisible", m_bottomPanel->isVisible());
    }
    
    settings.endGroup();
}

void TabPanelManager::RestorePanelState(QSettings& settings, const QString& group)
{
    settings.beginGroup(group);
    
    if (m_verticalSplitter) {
        const QByteArray data = settings.value("VerticalSplitterState").toByteArray();
        if (!data.isEmpty()) {
            m_verticalSplitter->restoreState(data);
        }
    }

    if (m_horizontalSplitter) {
        const QByteArray data = settings.value("HorizontalSplitterState").toByteArray();
        if (!data.isEmpty()) {
            m_horizontalSplitter->restoreState(data);
        }
    }

    if (m_leftPanel) {
        int index = settings.value("LeftPanelCurrentIndex", 0).toInt();
        if (index >= 0 && index < m_leftPanel->count()) {
            m_leftPanel->setCurrentIndex(index);
        }
    }
    if (m_rightPanel) {
        int index = settings.value("RightPanelCurrentIndex", 0).toInt();
        if (index >= 0 && index < m_rightPanel->count()) {
            m_rightPanel->setCurrentIndex(index);
        }
    }
    if (m_bottomPanel) {
        int index = settings.value("BottomPanelCurrentIndex", 0).toInt();
        if (index >= 0 && index < m_bottomPanel->count()) {
            m_bottomPanel->setCurrentIndex(index);
        }
        bool visible = settings.value("BottomPanelVisible", true).toBool();
        m_bottomPanel->setVisible(visible);
    }
    
    settings.endGroup();
}

void TabPanelManager::SetupPanelStyle()
{
    for (auto* tabWidget : {m_leftPanel, m_rightPanel, m_bottomPanel}) {
        if (!tabWidget) {
            continue;
        }
        tabWidget->tabBar()->setExpanding(false);
        tabWidget->tabBar()->setUsesScrollButtons(true);
        tabWidget->tabBar()->setDrawBase(false);
    }

    setStyleSheet(QStringLiteral(
        "#ViewportHost { background-color: #151A20; }"
        "#LeftPanelTabs::pane, #RightPanelTabs::pane, #BottomPanelTabs::pane { margin-top: 0px; }"));
}

} // namespace Aetherion::Editor
