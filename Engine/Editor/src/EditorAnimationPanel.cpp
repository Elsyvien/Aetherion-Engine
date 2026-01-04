#include "Aetherion/Editor/EditorAnimationPanel.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/AnimatorComponent.h"
#include "Aetherion/Assets/Animation.h"
#include "Aetherion/Assets/AnimationLoader.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QTimerEvent>
#include <QStyle>

namespace Aetherion::Editor
{

EditorAnimationPanel::EditorAnimationPanel(QWidget* parent)
    : QDockWidget(tr("Animation"), parent)
{
    SetupUI();

    // Start update timer for live playback updates
    m_updateTimerId = startTimer(33); // ~30 FPS updates
}

void EditorAnimationPanel::SetupUI()
{
    auto* mainWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(mainWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Entity info
    auto* infoGroup = new QGroupBox(tr("Entity"), this);
    auto* infoLayout = new QGridLayout(infoGroup);
    
    m_entityLabel = new QLabel(tr("No entity selected"), this);
    m_entityLabel->setStyleSheet("font-weight: bold;");
    infoLayout->addWidget(new QLabel(tr("Entity:"), this), 0, 0);
    infoLayout->addWidget(m_entityLabel, 0, 1);

    m_boneCountLabel = new QLabel(tr("-"), this);
    infoLayout->addWidget(new QLabel(tr("Bones:"), this), 1, 0);
    infoLayout->addWidget(m_boneCountLabel, 1, 1);

    layout->addWidget(infoGroup);

    // Clip selection
    auto* clipGroup = new QGroupBox(tr("Animation Clips"), this);
    auto* clipLayout = new QVBoxLayout(clipGroup);

    m_clipSelector = new QComboBox(this);
    m_clipSelector->setPlaceholderText(tr("Select animation..."));
    connect(m_clipSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EditorAnimationPanel::OnClipSelected);
    clipLayout->addWidget(m_clipSelector);

    auto* clipButtonLayout = new QHBoxLayout();
    auto* addClipBtn = new QPushButton(tr("Add"), this);
    auto* removeClipBtn = new QPushButton(tr("Remove"), this);
    connect(addClipBtn, &QPushButton::clicked, this, &EditorAnimationPanel::OnAddClipClicked);
    connect(removeClipBtn, &QPushButton::clicked, this, &EditorAnimationPanel::OnRemoveClipClicked);
    clipButtonLayout->addWidget(addClipBtn);
    clipButtonLayout->addWidget(removeClipBtn);
    clipButtonLayout->addStretch();
    clipLayout->addLayout(clipButtonLayout);

    layout->addWidget(clipGroup);

    // Playback controls
    auto* playbackGroup = new QGroupBox(tr("Playback"), this);
    auto* playbackLayout = new QVBoxLayout(playbackGroup);

    // Transport buttons
    auto* transportLayout = new QHBoxLayout();
    
    m_playButton = new QPushButton(this);
    m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_playButton->setToolTip(tr("Play"));
    connect(m_playButton, &QPushButton::clicked, this, &EditorAnimationPanel::OnPlayClicked);
    
    m_pauseButton = new QPushButton(this);
    m_pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseButton->setToolTip(tr("Pause"));
    connect(m_pauseButton, &QPushButton::clicked, this, &EditorAnimationPanel::OnPauseClicked);
    
    m_stopButton = new QPushButton(this);
    m_stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopButton->setToolTip(tr("Stop"));
    connect(m_stopButton, &QPushButton::clicked, this, &EditorAnimationPanel::OnStopClicked);

    transportLayout->addWidget(m_playButton);
    transportLayout->addWidget(m_pauseButton);
    transportLayout->addWidget(m_stopButton);
    transportLayout->addStretch();
    playbackLayout->addLayout(transportLayout);

    // Time slider
    auto* timeLayout = new QHBoxLayout();
    m_timeLabel = new QLabel(tr("0.00s"), this);
    m_timeLabel->setMinimumWidth(50);
    m_timeSlider = new QSlider(Qt::Horizontal, this);
    m_timeSlider->setRange(0, 1000);
    m_timeSlider->setValue(0);
    connect(m_timeSlider, &QSlider::valueChanged, this, &EditorAnimationPanel::OnTimeSliderChanged);
    m_durationLabel = new QLabel(tr("/ 0.00s"), this);
    m_durationLabel->setMinimumWidth(60);
    
    timeLayout->addWidget(m_timeLabel);
    timeLayout->addWidget(m_timeSlider);
    timeLayout->addWidget(m_durationLabel);
    playbackLayout->addLayout(timeLayout);

    // Speed and loop mode
    auto* optionsLayout = new QHBoxLayout();
    
    optionsLayout->addWidget(new QLabel(tr("Speed:"), this));
    m_speedSpinBox = new QDoubleSpinBox(this);
    m_speedSpinBox->setRange(0.01, 10.0);
    m_speedSpinBox->setValue(1.0);
    m_speedSpinBox->setSingleStep(0.1);
    connect(m_speedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EditorAnimationPanel::OnSpeedChanged);
    optionsLayout->addWidget(m_speedSpinBox);
    
    optionsLayout->addWidget(new QLabel(tr("Loop:"), this));
    m_loopModeCombo = new QComboBox(this);
    m_loopModeCombo->addItem(tr("Once"), static_cast<int>(Assets::AnimationWrapMode::Once));
    m_loopModeCombo->addItem(tr("Loop"), static_cast<int>(Assets::AnimationWrapMode::Loop));
    m_loopModeCombo->addItem(tr("Ping-Pong"), static_cast<int>(Assets::AnimationWrapMode::PingPong));
    m_loopModeCombo->addItem(tr("Clamp"), static_cast<int>(Assets::AnimationWrapMode::ClampForever));
    m_loopModeCombo->setCurrentIndex(1); // Loop by default
    connect(m_loopModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EditorAnimationPanel::OnLoopModeChanged);
    optionsLayout->addWidget(m_loopModeCombo);
    
    optionsLayout->addStretch();
    playbackLayout->addLayout(optionsLayout);

    layout->addWidget(playbackGroup);

    // Keyframe list (simplified view)
    auto* keyframeGroup = new QGroupBox(tr("Channels"), this);
    auto* keyframeLayout = new QVBoxLayout(keyframeGroup);
    
    m_keyframeList = new QListWidget(this);
    m_keyframeList->setMaximumHeight(150);
    keyframeLayout->addWidget(m_keyframeList);

    layout->addWidget(keyframeGroup);

    layout->addStretch();
    setWidget(mainWidget);
    setMinimumWidth(280);
}

void EditorAnimationPanel::SetSelectedEntity(std::shared_ptr<Scene::Entity> entity)
{
    m_selectedEntity = entity;
    Refresh();
}

void EditorAnimationPanel::Refresh()
{
    auto entity = m_selectedEntity.lock();
    
    if (!entity)
    {
        m_entityLabel->setText(tr("No entity selected"));
        m_boneCountLabel->setText(tr("-"));
        m_clipSelector->clear();
        m_keyframeList->clear();
        return;
    }

    m_entityLabel->setText(QString::fromStdString(entity->GetName()));

    // Check for skeleton
    auto skeleton = entity->GetComponent<Scene::SkeletonComponent>();
    if (skeleton && skeleton->GetSkeleton())
    {
        m_boneCountLabel->setText(QString::number(skeleton->GetSkeleton()->GetBoneCount()));
    }
    else
    {
        m_boneCountLabel->setText(tr("No skeleton"));
    }

    // Update clip list
    UpdateClipList();
    UpdatePlaybackControls();
}

void EditorAnimationPanel::UpdateClipList()
{
    m_clipSelector->blockSignals(true);
    m_clipSelector->clear();

    auto entity = m_selectedEntity.lock();
    if (!entity)
    {
        m_clipSelector->blockSignals(false);
        return;
    }

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator)
    {
        m_clipSelector->addItem(tr("(No Animator component)"));
        m_clipSelector->blockSignals(false);
        return;
    }

    const auto& clips = animator->GetClips();
    for (const auto& [name, clip] : clips)
    {
        m_clipSelector->addItem(QString::fromStdString(name));
    }

    // Select current clip if playing
    QString currentClip = QString::fromStdString(animator->GetCurrentClipName());
    int idx = m_clipSelector->findText(currentClip);
    if (idx >= 0)
    {
        m_clipSelector->setCurrentIndex(idx);
    }

    m_clipSelector->blockSignals(false);
}

void EditorAnimationPanel::UpdateTimeDisplay()
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    float currentTime = animator->GetCurrentTime();
    float normalizedTime = animator->GetNormalizedTime();

    auto layer = animator->GetLayer(0);
    float duration = 0.0f;
    if (layer && layer->state.clip)
    {
        duration = layer->state.clip->GetDuration();
    }

    m_timeLabel->setText(QString("%1s").arg(currentTime, 0, 'f', 2));
    m_durationLabel->setText(QString("/ %1s").arg(duration, 0, 'f', 2));

    m_timeSlider->blockSignals(true);
    m_timeSlider->setValue(static_cast<int>(normalizedTime * 1000));
    m_timeSlider->blockSignals(false);
}

void EditorAnimationPanel::UpdatePlaybackControls()
{
    auto entity = m_selectedEntity.lock();
    bool hasAnimator = entity && entity->GetComponent<Scene::AnimatorComponent>();

    m_playButton->setEnabled(hasAnimator);
    m_pauseButton->setEnabled(hasAnimator);
    m_stopButton->setEnabled(hasAnimator);
    m_timeSlider->setEnabled(hasAnimator);
    m_speedSpinBox->setEnabled(hasAnimator);
    m_loopModeCombo->setEnabled(hasAnimator);

    if (hasAnimator)
    {
        auto animator = entity->GetComponent<Scene::AnimatorComponent>();
        m_speedSpinBox->blockSignals(true);
        m_speedSpinBox->setValue(animator->GetSpeed());
        m_speedSpinBox->blockSignals(false);
    }
}

void EditorAnimationPanel::OnPlayClicked()
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    QString clipName = m_clipSelector->currentText();
    if (clipName.isEmpty()) return;

    if (!animator->IsPlaying())
    {
        animator->Play(clipName.toStdString());
    }
    else
    {
        animator->Resume();
    }

    m_isPlaying = true;
    emit playbackStateChanged(true);
}

void EditorAnimationPanel::OnPauseClicked()
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    animator->Pause();
    m_isPlaying = false;
    emit playbackStateChanged(false);
}

void EditorAnimationPanel::OnStopClicked()
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    animator->Stop();
    m_isPlaying = false;
    emit playbackStateChanged(false);
    UpdateTimeDisplay();
}

void EditorAnimationPanel::OnClipSelected(int index)
{
    if (index < 0) return;

    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    QString clipName = m_clipSelector->itemText(index);
    
    // Update keyframe list
    m_keyframeList->clear();
    auto clip = animator->GetClip(clipName.toStdString());
    if (clip)
    {
        for (const auto& channel : clip->GetChannels())
        {
            QString info = QString("%1 (P:%2 R:%3 S:%4)")
                .arg(QString::fromStdString(channel.boneName))
                .arg(channel.positionKeys.size())
                .arg(channel.rotationKeys.size())
                .arg(channel.scaleKeys.size());
            m_keyframeList->addItem(info);
        }

        m_durationLabel->setText(QString("/ %1s").arg(clip->GetDuration(), 0, 'f', 2));

        // Set loop mode combo
        m_loopModeCombo->blockSignals(true);
        for (int i = 0; i < m_loopModeCombo->count(); ++i)
        {
            if (m_loopModeCombo->itemData(i).toInt() == static_cast<int>(clip->GetWrapMode()))
            {
                m_loopModeCombo->setCurrentIndex(i);
                break;
            }
        }
        m_loopModeCombo->blockSignals(false);
    }

    emit animationChanged(clipName);
}

void EditorAnimationPanel::OnTimeSliderChanged(int value)
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    float normalized = value / 1000.0f;
    
    auto layer = animator->GetLayer(0);
    if (layer && layer->state.clip)
    {
        layer->state.time = normalized * layer->state.clip->GetDuration();
    }

    emit timeChanged(normalized);
    UpdateTimeDisplay();
}

void EditorAnimationPanel::OnSpeedChanged(double value)
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (animator)
    {
        animator->SetSpeed(static_cast<float>(value));
    }
}

void EditorAnimationPanel::OnLoopModeChanged(int index)
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    QString clipName = m_clipSelector->currentText();
    auto clip = animator->GetClip(clipName.toStdString());
    if (clip)
    {
        auto mode = static_cast<Assets::AnimationWrapMode>(m_loopModeCombo->itemData(index).toInt());
        clip->SetWrapMode(mode);
    }
}

void EditorAnimationPanel::OnAddClipClicked()
{
    // TODO: Open file dialog to load animation clip
    // For now, create a test animation
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator)
    {
        animator = entity->AddComponent<Scene::AnimatorComponent>();
    }

    // Add test skeleton if needed
    auto skeleton = entity->GetComponent<Scene::SkeletonComponent>();
    if (!skeleton)
    {
        skeleton = entity->AddComponent<Scene::SkeletonComponent>();
        skeleton->SetSkeleton(Assets::AnimationLoader::CreateTestSkeleton());
    }

    // Add test animation
    auto testClip = Assets::AnimationLoader::CreateTestAnimation(2.0f, "Root");
    animator->AddClip("TestAnim", testClip);

    Refresh();
}

void EditorAnimationPanel::OnRemoveClipClicked()
{
    auto entity = m_selectedEntity.lock();
    if (!entity) return;

    auto animator = entity->GetComponent<Scene::AnimatorComponent>();
    if (!animator) return;

    QString clipName = m_clipSelector->currentText();
    if (!clipName.isEmpty())
    {
        animator->RemoveClip(clipName.toStdString());
        Refresh();
    }
}

void EditorAnimationPanel::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_updateTimerId)
    {
        if (m_isPlaying)
        {
            UpdateTimeDisplay();
        }
    }

    QDockWidget::timerEvent(event);
}

} // namespace Aetherion::Editor
