# Elysia 引擎相机槽平滑过渡需求

实现状态：新版引擎已通过 `SceneCameraRuntime` 提供 `cut_to`、`blend_to`、`move_to` 与完成回调。本文保留原始接口设计提案；当前游戏流程以 [gameplay.md](gameplay.md) 为准：发射与弹丸失效时立即切镜，失败演出使用平滑切镜和拉远。

## 1. 需求背景

游戏需要在玩家控制镜头与演出镜头之间切换。例如：玩家使用 Main 相机观察和瞄准；发射后切换到 Cinematic 相机跟随弹丸；命中后由 Cinematic 完成停留和回镜；演出结束后恢复 Main 原有的中心与缩放。

引擎目前提供多个 `CameraSlot`，每个槽拥有独立的 `CameraController`、焦点、跟随策略、缩放和特效状态，但 `Scene::set_render_camera_slot()` 只会立即替换渲染槽。引擎缺少跨相机槽的中心与缩放插值，因此游戏层无法直接获得平滑镜头交接。

本需求要求引擎提供通用的相机槽过渡能力。具体游戏只负责配置各槽状态并请求过渡，不应自行维护临时混合相机、过渡计时器或插值公式。

## 2. 功能目标

- 在任意两个有效 `CameraSlot` 之间平滑切换。
- 过渡期间同时保留并更新源槽和目标槽的独立状态。
- 插值相机中心与缩放，UI 渲染不受影响。
- 支持立即切换、定时过渡、缓动和完成通知。
- 支持过渡过程中被新的过渡请求安全打断。
- 场景退出、重置或切换时不会残留过渡状态。
- 保持现有即时 `set_render_camera_slot()` 行为兼容。

## 3. 建议公共接口

在场景层提供渲染相机过渡接口：

```cpp
namespace elysia::camera
{
enum class CameraTransitionEasing : unsigned char
{
    Linear,
    SmoothStep,
    EaseInOutCubic
};

struct CameraTransitionSpec
{
    double duration_seconds = 0.35;
    CameraTransitionEasing easing = CameraTransitionEasing::SmoothStep;
};
}
```

```cpp
class Scene
{
protected:
    void set_render_camera_slot(CameraSlot slot) noexcept;
    void transition_render_camera_slot(
        CameraSlot target,
        const CameraTransitionSpec& spec);

    [[nodiscard]] bool render_camera_transition_active() const noexcept;
    [[nodiscard]] std::optional<CameraSlot>
        render_camera_transition_target() const noexcept;

    virtual void on_render_camera_transition_completed(CameraSlot target) {}
};
```

接口语义：

- `set_render_camera_slot()` 保留为立即切换，并取消当前过渡。
- `transition_render_camera_slot()` 从当前实际显示构图过渡到目标槽。
- `duration_seconds <= 0` 等价于立即切换。
- `CameraSlot::Count` 属于无效目标；Debug 构建断言，Release 构建忽略请求。
- 目标槽与当前槽相同时不创建过渡，并保持当前画面不变。
- 完成通知仅在过渡自然结束时调用；取消或场景清理不调用。

## 4. 引擎内部模型

Scene 保存一个可选的活动过渡状态和一台仅用于呈现的混合相机：

```cpp
struct ActiveCameraTransition
{
    CameraSlot source_slot;
    CameraSlot target_slot;
    Camera source_snapshot;
    double elapsed_seconds;
    CameraTransitionSpec spec;
};

std::optional<ActiveCameraTransition> _camera_transition;
Camera _transition_camera;
```

`Scene::camera()` 行为：

- 没有活动过渡时，返回当前 `render_camera_slot()` 对应的相机。
- 存在活动过渡时，返回 `_transition_camera`。

这样世界对象、物理调试绘制和其他世界空间渲染会自动使用同一过渡构图，不需要修改 RenderCommand API。

## 5. 过渡计算

每帧在所有 `CameraController` 更新完成后计算过渡：

1. 更新经过时间并计算归一化进度 `t`。
2. 根据 `CameraTransitionEasing` 得到缓动进度。
3. 从 `source_snapshot` 插值到目标槽当前的最终呈现状态。
4. 写入 `_transition_camera`。
5. 进度到达 1 时，将当前渲染槽设置为目标槽并清除过渡。

首版插值字段：

- 世界中心：线性插值 `Vector2`。
- 缩放：线性插值后使用 `Camera::clamp_zoom()`。
- 视口尺寸：始终使用目标槽视口；所有槽正常情况下应拥有相同逻辑视口。

首版不处理相机旋转，因为当前 `Camera` 没有旋转状态。震屏已经反映在各槽最终相机中心中，因此目标槽震屏可以自然参与最终构图。

## 6. 源构图快照与目标实时更新

- 过渡开始时捕获当前实际显示相机，而不是只记录源槽名称。
- 源端使用固定快照，避免源槽继续移动导致插值起点漂移。
- 目标端每帧读取目标槽最新状态。
- 目标槽的焦点、跟随策略、缩放过渡和震屏在混合期间继续更新。

这允许目标 Cinematic 相机在过渡期间继续跟随移动中的弹丸，也能保证过渡从玩家当时看到的精确构图开始。

## 7. 过渡中断规则

活动过渡期间收到新的 `transition_render_camera_slot()` 请求时：

- 捕获当前 `_transition_camera` 作为新的 `source_snapshot`。
- 新请求的目标槽成为新的 `target_slot`。
- 经过时间重新从 0 开始。
- 画面不能跳回原始源槽。

活动过渡期间调用 `set_render_camera_slot()` 时：

- 立即取消过渡。
- 直接使用指定槽渲染。
- 不调用完成通知。

相机槽被重置且它是当前过渡目标时：

- 取消过渡并保持当前源渲染槽。
- 不保留指向已重置控制器状态的引用。

## 8. 场景焦点路由

`Scene::on_update()` 当前把 `resolve_camera_focus()` 固定写入 `CameraSlot::Main`，这与多槽渲染不兼容。

需要调整为：

- 无过渡时，将场景解析出的焦点写入当前 `render_camera_slot()`。
- 有过渡时，将焦点写入过渡目标槽。
- 源槽保留切换前的焦点和控制状态，不被目标演出覆盖。

游戏层仍可直接通过 `CameraManager` 为非活动槽预先配置中心、缩放、世界边界和跟随策略。

## 9. 生命周期与清理

- Scene 进入新场景前不得继承上一场景的活动过渡。
- `Scene::reset()`、SceneManager 场景切换和应用关闭必须取消活动过渡。
- SceneManager 应重置场景使用的所有相机槽，而不只重置 Main；首版可在场景切换时调用 `CameraManager::reset_all()`。
- 重置后所有槽清除焦点、跟随策略、震屏和缩放过渡，并恢复默认缩放与中心。
- 过渡状态只能保存槽 ID 和相机值快照，不得保存场景对象或弹丸指针。

## 10. 与现有接口的兼容性

- `CameraManager::camera(slot)` 继续返回对应槽的真实相机，不返回混合相机。
- `Scene::camera()` 在过渡期间返回混合呈现相机。
- `Scene::render_camera_slot()` 在过渡完成前继续返回源槽。
- 新增 `render_camera_transition_target()` 用于读取目标槽。
- 现有未调用过渡接口的场景保持原有行为。
- UI 继续使用逻辑屏幕坐标，不参与相机插值。
- 不修改世界 RenderCommand、物理世界或输入路由公共接口。

## 11. Orbital Strike 使用示例

```cpp
camera_manager.set_world_bounds(CameraSlot::Cinematic, activity_bounds);
camera_manager.set_zoom(CameraSlot::Cinematic, flight_zoom);
camera_manager.set_follow_strategy(
    CameraSlot::Cinematic,
    std::make_unique<SmoothFollowStrategy>(1800.0));

transition_render_camera_slot(
    CameraSlot::Cinematic,
    {.duration_seconds = 0.20,
     .easing = CameraTransitionEasing::SmoothStep});
```

弹丸结算后，游戏为 Cinematic 配置 MoonCell 焦点，然后请求：

```cpp
transition_render_camera_slot(
    CameraSlot::Main,
    {.duration_seconds = 0.35,
     .easing = CameraTransitionEasing::SmoothStep});
```

Main 的玩家缩放和镜头状态在整个演出期间保持独立，因此游戏层不需要手动恢复缩放。

## 12. 验收标准

- Main 与 Cinematic 在不同中心和缩放下可以平滑互相切换。
- 过渡第一帧与请求前画面一致，不发生起始跳变。
- 过渡最后一帧与目标槽画面一致，不发生结束跳变。
- 目标槽跟随移动对象时，过渡能够追随其最新构图。
- `Linear`、`SmoothStep` 和 `EaseInOutCubic` 产生可验证的不同进度曲线。
- 时长为 0 或负值时立即切换。
- 向当前槽发起过渡不会修改画面或创建活动状态。
- 过渡过程中发起新过渡时，从当前混合画面连续转向新目标。
- 过渡过程中立即切槽时正确取消，不触发完成通知。
- 场景退出、重置和重新进入后没有旧过渡、旧焦点或旧特效。
- 世界对象与物理调试绘制在过渡期间使用相同混合相机。
- UI 在整个过渡期间位置和缩放保持不变。
- 现有只使用 Main 的场景行为不变。

## 13. 暂不包含

- 相机旋转插值。
- 多台相机画面的纹理交叉淡化。
- 后处理效果混合。
- 分屏与画中画。
- 贝塞尔轨迹、路径关键帧或完整时间轴系统。
- 相机优先级栈和自动镜头导演系统。
