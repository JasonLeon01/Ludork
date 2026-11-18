import {
  AbilityConstant,
  PermissionRequestResult,
  Permissions,
  UIAbility,
  Want,
  abilityAccessCtrl,
  bundleManager
} from '@kit.AbilityKit';
import { deviceInfo } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';

const DOMAIN = 0x4C44;
const TAG = 'Ludork-Harmony';

export default class EntryAbility extends UIAbility {
  private configureSupportedWindowModes(windowStage: window.WindowStage): Promise<void> {
    if (deviceInfo.deviceType !== '2in1') {
      return Promise.resolve();
    }

    return windowStage.setSupportedWindowModes([
      bundleManager.SupportWindowMode.FULL_SCREEN,
      bundleManager.SupportWindowMode.FLOATING
    ]).then(() => {
      hilog.info(DOMAIN, TAG, 'Enabled full-screen and floating window modes');
    }).catch((error: Error) => {
      hilog.warn(DOMAIN, TAG, 'Unable to set supported window modes: %{public}s', error.message);
    });
  }

  private requestRuntimePermissions(): void {
    const permissions: Array<Permissions> = ['ohos.permission.MICROPHONE'];
    try {
      const info = bundleManager.getBundleInfoForSelfSync(
        bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_REQUESTED_PERMISSION
      );
      if (info.reqPermissionDetails.some(permission => permission.name === 'ohos.permission.READ_PASTEBOARD')) {
        permissions.push('ohos.permission.READ_PASTEBOARD');
      }
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'Unable to query declared permissions: %{public}s', (error as Error).message);
    }

    abilityAccessCtrl.createAtManager().requestPermissionsFromUser(this.context, permissions)
      .then((result: PermissionRequestResult) => {
        const denied: Array<string> = [];
        for (let index = 0; index < result.permissions.length; ++index) {
          if (result.authResults[index] !== 0) {
            denied.push(result.permissions[index]);
          }
        }

        if (denied.length > 0) {
          hilog.warn(DOMAIN, TAG, 'Runtime permission denied: %{public}s', denied.join(', '));
        }
      })
      .catch((error: Error) => {
        hilog.warn(DOMAIN, TAG, 'Unable to request runtime permissions: %{public}s', error.message);
      });
  }

  onCreate(_want: Want, _launchParam: AbilityConstant.LaunchParam): void {
    AppStorage.setOrCreate<number>('sfmlWindowId', 0);
    AppStorage.setOrCreate<string>('sfmlLifecycle', 'created');
    hilog.info(DOMAIN, TAG, 'EntryAbility created');
  }

  onDestroy(): void {
    AppStorage.set<string>('sfmlLifecycle', 'destroyed');
    hilog.info(DOMAIN, TAG, 'EntryAbility destroyed');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    this.requestRuntimePermissions();

    this.configureSupportedWindowModes(windowStage).then(() => {
      return windowStage.getMainWindow().then((mainWindow: window.Window) => {
        AppStorage.set<number>('sfmlWindowId', mainWindow.getWindowProperties().id);
        if (deviceInfo.deviceType !== '2in1') {
          return mainWindow.setPreferredOrientation(window.Orientation.LANDSCAPE);
        }
        return Promise.resolve();
      }).catch((error: Error) => {
        hilog.error(DOMAIN, TAG, 'Unable to obtain main window: %{public}s', error.message);
      });
    }).then(() => {
      windowStage.loadContent('pages/Index', (error) => {
        if (error.code) {
          hilog.error(DOMAIN, TAG, 'Unable to load page: %{public}s', JSON.stringify(error));
          return;
        }

        hilog.info(DOMAIN, TAG, 'Ludork XComponent page loaded');
      });
    });
  }

  onWindowStageDestroy(): void {
    AppStorage.set<string>('sfmlLifecycle', 'surface-destroyed');
  }

  onForeground(): void {
    AppStorage.set<string>('sfmlLifecycle', 'foreground');
  }

  onBackground(): void {
    AppStorage.set<string>('sfmlLifecycle', 'background');
  }
}
