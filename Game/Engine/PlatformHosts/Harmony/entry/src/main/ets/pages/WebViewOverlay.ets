import { webview } from '@kit.ArkWeb';
import { KeyCode } from '@kit.InputKit';

@Component
export struct WebViewOverlay {
  @Prop url: string = '';
  @Prop @Watch('navigate') request: number = 0;
  @State private error: string = '';
  private controller: webview.WebviewController = new webview.WebviewController();
  private controllerReady: boolean = false;
  private navigationUrl: string = "";
  close: () => void = (): void => {};
  back: () => void = (): void => {};

  private navigate(): void {
    this.error = '';
    this.navigationUrl = this.url;
    if (this.controllerReady) {
      try {
        this.controller.loadUrl(this.url);
      } catch (failure) {
        this.error = `Unable to load this page.\n${(failure as Error).message}`;
      }
    }
  }

  private isCurrentMainFrame(address: string): boolean {
    const target = this.navigationUrl.length > 0 ? this.navigationUrl : this.url;
    if (address.split('#')[0] === target.split('#')[0]) {
      return true;
    }
    if (!this.controllerReady) {
      return false;
    }
    try {
      return this.controller.getOriginalUrl().split('#')[0] === target.split('#')[0] &&
        this.controller.getUrl().split('#')[0] === address.split('#')[0];
    } catch (failure) {
      return false;
    }
  }

  private handleKey(event: KeyEvent): boolean {
    if (event.keyCode === KeyCode.KEYCODE_ESCAPE) {
      if (event.type === KeyType.Up) {
        this.back();
      }
      event.stopPropagation();
      return true;
    }
    return false;
  }

  build() {
    Column() {
      Row() {
        Button('Close')
          .onClick(() => this.close())
          .margin(8);
      }
      .width('100%')
      .justifyContent(FlexAlign.End)
      .backgroundColor('#FFFFFF');
      Stack() {
        Web({ src: this.url, controller: this.controller })
          .width('100%')
          .height('100%')
          .defaultFocus(true)
          .javaScriptAccess(true)
          .domStorageAccess(true)
          .fileAccess(false)
          .mixedMode(MixedMode.None)
          .multiWindowAccess(false)
          .allowWindowOpenMethod(true)
          .onControllerAttached(() => {
            this.controllerReady = true;
          })
          .onLoadIntercept((event: OnLoadInterceptEvent): boolean => {
            const address = event.data.getRequestUrl();
            const scheme = address.toLowerCase();
            if (!scheme.startsWith('https://') && !scheme.startsWith('http://')) {
              return true;
            }
            if (event.data.isMainFrame()) {
              this.navigationUrl = address;
            }
            return false;
          })
          .onPageBegin((event: OnPageBeginEvent) => {
            if (this.isCurrentMainFrame(event.url)) {
              this.error = '';
            }
          })
          .onErrorReceive((event: OnErrorReceiveEvent): void => {
            if (event.request.isMainFrame() && this.isCurrentMainFrame(event.request.getRequestUrl()) &&
              !event.error.getErrorInfo().includes('ERR_ABORTED')) {
              this.error = `Unable to load this page.\n${event.error.getErrorInfo()}`;
            }
          })
          .onHttpErrorReceive((event: OnHttpErrorReceiveEvent): void => {
            if (event.request.isMainFrame() && this.isCurrentMainFrame(event.request.getRequestUrl())) {
              this.error = `Unable to load this page (HTTP ${event.response.getResponseCode()}).`;
            }
          })
          .onSslErrorEvent((event: SslErrorEvent): void => {
            event.handler.handleCancel();
            if (this.isCurrentMainFrame(event.url)) {
              this.error = "Unable to verify this page's certificate.";
            }
          })
          .onRenderExited(() => {
            this.error = 'The web page stopped unexpectedly. Close it and open it again.';
          })
          .onInterceptKeyEvent((event: KeyEvent): boolean => this.handleKey(event));
        if (this.error.length > 0) {
          Text(this.error)
            .width('100%')
            .height('100%')
            .textAlign(TextAlign.Center)
            .padding(24)
            .fontColor('#374151')
            .backgroundColor('#FFFFFF')
            .hitTestBehavior(HitTestMode.Block);
        }
      }
      .width('100%')
      .layoutWeight(1);
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#FFFFFF')
    .hitTestBehavior(HitTestMode.BLOCK_HIERARCHY)
    .onKeyEvent((event: KeyEvent) => {
      this.handleKey(event);
      event.stopPropagation();
    });
  }
}
