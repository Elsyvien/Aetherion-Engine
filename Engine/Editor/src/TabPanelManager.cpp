#include "Aetherion/Editor/TabPanelManager.h"

#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QSplitter>
#include <QStyleFactory>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QList>

namespace Aetherion::Editor
{

TabPanelManager::TabPanelManager(QWidget* parent)
    : QWidget(parent)
{
    // Main layout for the panel groups
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Vertical splitter separates main area and bottom panel
    m_verticalSplitter = new QSplitter(Qt::Vertical, this);
    m_verticalSplitter->setStyleSheet("QSplitter::handle { background: #3a3a3a; height: 4px; }");
    m_verticalSplitter->setHandleWidth(4);

    // Horizontal splitter for left / center / right
    m_horizontalSplitter = new QSplitter(Qt::Horizontal, m_verticalSplitter);
    m_horizontalSplitter->setStyleSheet("QSplitter::handle { background: #3a3a3a; width: 4px; }");
    m_horizontalSplitter->setHandleWidth(4);

    // Left panel (Assets & Hierarchy)
    m_leftPanel = new QTabWidget(this);
    m_leftPanel->setTabPosition(QTabWidget::North);
    m_leftPanel->setDocumentMode(true);
    m_leftPanel->setMinimumWidth(200);
    m_leftPanel->setMaximumWidth(600);

    // Center container (viewport goes here)
    m_centerContainer = new QWidget(this);
    auto* centerLayout = new QVBoxLayout(m_centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    // Right panel (Inspector & Previews & Copilot)
    m_rightPanel = new QTabWidget(this);
    m_rightPanel->setTabPosition(QTabWidget::North);
    m_rightPanel->setDocumentMode(true);
    m_rightPanel->setMinimumWidth(250);
    m_rightPanel->setMaximumWidth(800);

    // Assemble horizontal splitter
    m_horizontalSplitter->addWidget(m_leftPanel);
    m_horizontalSplitter->addWidget(m_centerContainer);
    m_horizontalSplitter->addWidget(m_rightPanel);
    m_horizontalSplitter->setStretchFactor(0, 1);
    m_horizontalSplitter->setStretchFactor(1, 3);
    m_horizontalSplitter->setStretchFactor(2, 1);
    m_horizontalSplitter->setCollapsible(0, true);
    m_horizontalSplitter->setCollapsible(1, false);
    m_horizontalSplitter->setCollapsible(2, true);

    // Bottom panel (Console & Statistics & Animation & Logic Copilot)
    m_bottomPanel = new QTabWidget(this);
    m_bottomPanel->setTabPosition(QTabWidget::North);
    m_bottomPanel->setDocumentMode(true);
    m_bottomPanel->setMinimumHeight(150);
    m_bottomPanel->setMaximumHeight(500);

    // Assemble vertical splitter
    m_verticalSplitter->addWidget(m_horizontalSplitter);
    m_verticalSplitter->addWidget(m_bottomPanel);
    m_verticalSplitter->setStretchFactor(0, 3);
    m_verticalSplitter->setStretchFactor(1, 1);
    m_verticalSplitter->setCollapsible(1, true);

    mainLayout->addWidget(m_verticalSplitter);

    SetupPanelStyle();

    setLayout(mainLayout);
}

TabPanelManager::~TabPanelManager() = default;

void TabPanelManager::CreatePanelGroups(QMainWindow* mainWindow)
{
    if (!mainWindow) {
        return;
    }

    // Configure tab widgets styling
    for (auto* tabWidget : { m_leftPanel, m_rightPanel, m_bottomPanel }) {
        if (tabWidget) {
            tabWidget->setStyleSheet(
                "QTabWidget::pane { border: none; margin: 0px; padding: 0px; } "
                "QTabBar::tab { "
                "    background-color: #2d2d2d; "
                "    color: #cccccc; "
                "    padding: 6px 12px; "
                "    border: 1px solid #1a1a1a; "
                "    margin: 1px; "
                "} "
                "QTabBar::tab:selected { "
                "    background-color: #0e47a1; "
                "    color: white; "
                "    border-bottom: 2px solid #1976d2; "
                "} "
                "QTabBar::tab:hover:!selected { "
                "    background-color: #3d3d3d; "
                "}"
            );
        }
    }
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
    if (m_bottomPanel) {
        m_bottomPanel->setVisible(visible);
    }

    if (m_verticalSplitter && m_bottomPanel) {
        QList<int> sizes = m_verticalSplitter->sizes();
        if (!visible) {
            if (sizes.size() >= 2) {
                sizes[0] = sizes[0] + sizes[1];
                sizes[1] = 0;
                m_verticalSplitter->setSizes(sizes);
            }
        } else {
            if (sizes.size() >= 2 && sizes[1] == 0) {
                sizes[0] = 3;
                sizes[1] = 1;
                m_verticalSplitter->setSizes(sizes);
            }
        }
    }
}

bool TabPanelManager::IsBottomPanelVisible() const
{
    return m_bottomPanel && m_bottomPanel->isVisible();
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
    // Apply a clean, modern stylesheet to all panels
    for (auto* tabWidget : { m_leftPanel, m_rightPanel, m_bottomPanel }) {
        if (tabWidget) {
            tabWidget->setStyleSheet(
                "QTabWidget::pane { "
                "    border-top: 1px solid #1a1a1a; "
                "    background-color: #1e1e1e; "
                "} "
                "QTabBar::tab { "
                "    background-color: #2d2d2d; "
                "    color: #aaaaaa; "
                "    padding: 8px 16px; "
                "    border: none; "
                "    border-bottom: 2px solid transparent; "
                "    margin-right: 2px; "
                "    min-width: 80px; "
                "} "
                "QTabBar::tab:hover { "
                "    background-color: #3d3d3d; "
                "    color: #cccccc; "
                "} "
                "QTabBar::tab:selected { "
                "    background-color: #0d47a1; "
                "    color: white; "
                "    border-bottom: 3px solid #1976d2; "
                "}"
            );
        }
    }
}

} // namespace Aetherion::Editor
