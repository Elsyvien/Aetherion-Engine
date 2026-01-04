#pragma once

#include <QDockWidget>
#include <memory>

class QComboBox;
class QSlider;
class QLabel;
class QPushButton;
class QSpinBox;
class QListWidget;
class QDoubleSpinBox;

namespace Aetherion::Assets
{
class AnimationClip;
class Skeleton;
}

namespace Aetherion::Scene
{
class Entity;
class AnimatorComponent;
class SkeletonComponent;
}

namespace Aetherion::Editor
{

class EditorAnimationPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit EditorAnimationPanel(QWidget* parent = nullptr);
    ~EditorAnimationPanel() override = default;

    /// @brief Set the currently selected entity to display animation controls
    void SetSelectedEntity(std::shared_ptr<Scene::Entity> entity);

    /// @brief Refresh the panel with current state
    void Refresh();

signals:
    void animationChanged(const QString& clipName);
    void playbackStateChanged(bool playing);
    void timeChanged(float normalizedTime);

public slots:
    void OnPlayClicked();
    void OnPauseClicked();
    void OnStopClicked();
    void OnClipSelected(int index);
    void OnTimeSliderChanged(int value);
    void OnSpeedChanged(double value);
    void OnLoopModeChanged(int index);
    void OnAddClipClicked();
    void OnRemoveClipClicked();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void SetupUI();
    void UpdateClipList();
    void UpdateTimeDisplay();
    void UpdatePlaybackControls();

    // UI Elements
    QComboBox* m_clipSelector{nullptr};
    QSlider* m_timeSlider{nullptr};
    QLabel* m_timeLabel{nullptr};
    QLabel* m_durationLabel{nullptr};
    QPushButton* m_playButton{nullptr};
    QPushButton* m_pauseButton{nullptr};
    QPushButton* m_stopButton{nullptr};
    QDoubleSpinBox* m_speedSpinBox{nullptr};
    QComboBox* m_loopModeCombo{nullptr};
    QListWidget* m_keyframeList{nullptr};
    QLabel* m_entityLabel{nullptr};
    QLabel* m_boneCountLabel{nullptr};

    // State
    std::weak_ptr<Scene::Entity> m_selectedEntity;
    int m_updateTimerId{0};
    bool m_isPlaying{false};
};

} // namespace Aetherion::Editor
