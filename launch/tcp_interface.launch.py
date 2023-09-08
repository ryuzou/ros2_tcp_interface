from launch import LaunchDescription, EventHandler
from launch.actions import EmitEvent, LogInfo, RegisterEventHandler
from launch.events import Shutdown, matches_action
from launch_ros.actions import LifecycleNode
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition

def generate_launch_description():

    # LifecycleNodeの定義
    lifecycle_node = LifecycleNode(
        name='tcp_interface_node',
        namespace='tcp_interface',
        package='tcp_interface',
        executable='tcp_interface_node_exec',
        output='screen'
    )

    # 以下は、ライフサイクルノードの状態遷移の定義です。
    def emit_event(transition_id):
        return EmitEvent(event=ChangeState(
            lifecycle_node_matcher=matches_action(lifecycle_node),
            transition_id=transition_id
        ))

    # 状態遷移のイベントハンドラを登録する関数
    def register_event_handler(goal_state, transition_id, msg):
        return RegisterEventHandler(
            OnStateTransition(
                target_lifecycle_node=lifecycle_node,
                goal_state=goal_state,
                entities=[
                    # LogInfo(msg=msg),
                    emit_event(transition_id)
                ]
            )
        )

    # 各状態遷移のイベントハンドラ
    from_unconfigured_to_inactive = register_event_handler('unconfigured', Transition.TRANSITION_CONFIGURE, "<< Unconfigured >>")
    from_inactive_to_active = register_event_handler('inactive', Transition.TRANSITION_ACTIVATE, "<< Inactive >>")
    from_active_to_inactive = register_event_handler('active', Transition.TRANSITION_DEACTIVATE, "<< Active >>")
    from_inactive_to_finalized = register_event_handler('inactive', Transition.TRANSITION_INACTIVE_SHUTDOWN, "<< Inactive >>")
    from_finalized_to_exit = register_event_handler('finalized', Transition.TRANSITION_UNCONFIGURED_SHUTDOWN, "<< Finalized >>")

    # 最初の状態遷移イベント
    to_unconfigured = emit_event(Transition.TRANSITION_CONFIGURE)

    # シャットダウンイベントのイミット
    emit_shutdown = EmitEvent(event=Shutdown())

    # LaunchDescriptionにイベントハンドラとノードを追加
    return LaunchDescription([
        to_unconfigured,
        from_unconfigured_to_inactive,
        from_inactive_to_active,
        lifecycle_node,
    ])