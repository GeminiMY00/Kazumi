import 'dart:io';

import 'package:kazumi/bean/dialog/dialog_helper.dart';
import 'package:kazumi/services/player/external_player.dart';

class ExternalPlaybackLauncher {
  final String Function() videoUrl;
  final String Function() referer;

  ExternalPlaybackLauncher({
    required this.videoUrl,
    required this.referer,
  });

  bool _launchingPotPlayer = false;

  Future<void> launchPotPlayer() async {
    if (!Platform.isWindows || _launchingPotPlayer) return;
    _launchingPotPlayer = true;
    try {
      final result = await ExternalPlayer.launchPotPlayer(videoUrl(), referer());
      KazumiDialog.showToast(message: switch (result) {
        PotPlayerResult.launched => '已请求 PotPlayer 播放；若无法播放，请继续使用内置播放器',
        PotPlayerResult.notInstalled =>
          '未找到 PotPlayer，请安装到默认目录或注册 Windows App Paths 后重试',
        PotPlayerResult.unsupportedSource =>
          '当前视频地址尚未就绪或需要特殊请求头/本地播放，请使用内置播放器',
        PotPlayerResult.failed => '启动 PotPlayer 失败，请检查安装或继续使用内置播放器',
      });
    } finally {
      _launchingPotPlayer = false;
    }
  }

  Future<void> launch() async {
    final currentVideoUrl = videoUrl();
    final currentReferer = referer();
    if ((Platform.isAndroid || Platform.isWindows) && currentReferer.isEmpty) {
      if (await ExternalPlayer.launchUrlWithMime(
          currentVideoUrl, 'video/mp4')) {
        KazumiDialog.showToast(
          message: '尝试唤起外部播放器',
        );
      } else {
        KazumiDialog.showToast(
          message: '唤起外部播放器失败',
        );
      }
    } else if (Platform.isMacOS || Platform.isIOS) {
      if (await ExternalPlayer.launchUrlWithReferer(
          currentVideoUrl, currentReferer)) {
        KazumiDialog.showToast(
          message: '尝试唤起外部播放器',
        );
      } else {
        KazumiDialog.showToast(
          message: '唤起外部播放器失败',
        );
      }
    } else if (Platform.isLinux && currentReferer.isEmpty) {
      final result =
          await ExternalPlayer.launchLinuxDesktopPlayer(currentVideoUrl);
      switch (result) {
        case LinuxExternalPlayerResult.launched:
          KazumiDialog.showToast(message: '尝试唤起外部播放器');
        case LinuxExternalPlayerResult.cancelled:
          break;
        case LinuxExternalPlayerResult.unavailable:
          KazumiDialog.showToast(message: '系统应用选择器不可用');
        case LinuxExternalPlayerResult.failed:
          KazumiDialog.showToast(message: '唤起外部播放器失败');
      }
    } else {
      if (currentReferer.isEmpty) {
        KazumiDialog.showToast(
          message: '暂不支持该设备',
        );
      } else {
        KazumiDialog.showToast(
          message: '暂不支持该规则',
        );
      }
    }
  }
}
