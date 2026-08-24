import {
  AbilityConstant,
  PermissionRequestResult,
  Permissions,
  UIAbility,
  Want,
  abilityAccessCtrl
} from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';

const DOMAIN = 0x4C44;
const TAG = 'Ludork-Harmony';

export default class EntryAbility extends UIAbility {
  private requestRuntimePermissions(): void {
    const permissions: Array<Permissions> = [
      'ohos.permission.MICROPHONE',
      'ohos.permission.READ_PASTEBOARD'
    ];

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

    windowStage.getMainWindow().then((mainWindow: window.Window) => {
      AppStorage.set<number>('sfmlWindowId', mainWindow.getWindowProperties().id);
    }).catch((error: Error) => {
      hilog.error(DOMAIN, TAG, 'Unable to obtain main window: %{public}s', error.message);
    });

    windowStage.loadContent('pages/Index', (error) => {
      if (error.code) {
        hilog.error(DOMAIN, TAG, 'Unable to load page: %{public}s', JSON.stringify(error));
        return;
      }

      hilog.info(DOMAIN, TAG, 'Ludork XComponent page loaded');
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
