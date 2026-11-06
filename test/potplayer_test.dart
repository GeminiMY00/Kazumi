import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kazumi/services/player/external_player.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('com.predidit.kazumi/intent');
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;

  tearDown(() => messenger.setMockMethodCallHandler(channel, null));

  test('passes the exact current URL including signed query and Unicode',
      () async {
    const url = 'https://example.com/当前选集.m3u8?token=a%2Fb&part=2';
    messenger.setMockMethodCallHandler(channel, (call) async {
      expect(call.method, 'openWithPotPlayer');
      expect(call.arguments, {'url': url});
      return 'launched';
    });
    expect(await ExternalPlayer.launchPotPlayer(url, ''),
        PotPlayerResult.launched);
  });

  test('rejects unavailable, local, unsafe, or header-dependent sources',
      () async {
    messenger.setMockMethodCallHandler(channel, (_) async {
      fail('Unsupported sources must not reach the native launcher');
    });
    for (final url in [
      '',
      '/downloads/episode.mp4',
      r'C:\downloads\episode.mp4',
      'file:///C:/episode.mp4',
      'blob:https://example.com/id',
      'ftp://example.com/video.mp4',
      'https:///episode.mp4',
      'https://example.com/video" /malicious',
      'https://example.com/video\n#EXTINF',
      'https://example.com/video\\',
    ]) {
      expect(await ExternalPlayer.launchPotPlayer(url, ''),
          PotPlayerResult.unsupportedSource,
          reason: url);
    }
    expect(
      await ExternalPlayer.launchPotPlayer(
          'https://example.com/video.m3u8', 'https://example.com'),
      PotPlayerResult.unsupportedSource,
    );
  });

  for (final entry in {
    'notInstalled': PotPlayerResult.notInstalled,
    'failed': PotPlayerResult.failed,
    'unsupportedSource': PotPlayerResult.unsupportedSource,
    'unknown': PotPlayerResult.failed,
  }.entries) {
    test('handles native result ${entry.key}', () async {
      messenger.setMockMethodCallHandler(channel, (_) async => entry.key);
      expect(
        await ExternalPlayer.launchPotPlayer('https://example.com/video', ''),
        entry.value,
      );
    });
  }

  test('handles native launch exceptions', () async {
    messenger.setMockMethodCallHandler(channel, (_) async {
      throw PlatformException(code: 'LaunchFailed');
    });
    expect(
      await ExternalPlayer.launchPotPlayer('https://example.com/video', ''),
      PotPlayerResult.failed,
    );
  });

  test('handles an unavailable native method', () async {
    messenger.setMockMethodCallHandler(channel, (_) async => null);
    expect(
      await ExternalPlayer.launchPotPlayer('https://example.com/video', ''),
      PotPlayerResult.failed,
    );
  });
}
