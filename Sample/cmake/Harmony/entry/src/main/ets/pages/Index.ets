import { common } from '@kit.AbilityKit';
import { BusinessError, deviceInfo, zlib } from '@kit.BasicServicesKit';
import { fileIo } from '@kit.CoreFileKit';
import { image } from '@kit.ImageKit';
import { KeyCode, pointer } from '@kit.InputKit';
import { i18n, resourceManager } from '@kit.LocalizationKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { display, window } from '@kit.ArkUI';

const DOMAIN = 0x4C44;
const TAG = 'Ludork-Harmony';
const RUNTIME_ARCHIVE = 'ludork-runtime.zip';
const RUNTIME_HASH = '__LUDORK_RUNTIME_HASH__';
const RUNTIME_MARKER = '.ludork-runtime-complete';
const COPY_CHUNK_SIZE = 1024 * 1024;

const SYSTEM_POINTER_STYLES: Array<pointer.PointerStyle> = [
  pointer.PointerStyle.DEFAULT,
  pointer.PointerStyle.RUNNING,
  pointer.PointerStyle.LOADING,
  pointer.PointerStyle.TEXT_CURSOR,
  pointer.PointerStyle.HAND_POINTING,
  pointer.PointerStyle.RESIZE_LEFT_RIGHT,
  pointer.PointerStyle.RESIZE_UP_DOWN,
  pointer.PointerStyle.NORTH_WEST_SOUTH_EAST,
  pointer.PointerStyle.NORTH_EAST_SOUTH_WEST,
  pointer.PointerStyle.WEST,
  pointer.PointerStyle.EAST,
  pointer.PointerStyle.NORTH,
  pointer.PointerStyle.SOUTH,
  pointer.PointerStyle.NORTH_WEST,
  pointer.PointerStyle.SOUTH_EAST,
  pointer.PointerStyle.SOUTH_WEST,
  pointer.PointerStyle.NORTH_EAST,
  pointer.PointerStyle.MOVE,
  pointer.PointerStyle.CROSS,
  pointer.PointerStyle.HELP,
  pointer.PointerStyle.CURSOR_FORBID
];

class SfmlHostCallbacks {
  setPointerVisible: (visible: boolean) => void;
  setSystemPointer: (type: number) => void;
  setCustomPointer: (
    pixels: Uint8Array,
    width: number,
    height: number,
    hotspotX: number,
    hotspotY: number
  ) => void;
  setVirtualKeyboardVisible: (visible: boolean) => void;
  configureWindow: (fullscreen: boolean, keepScreenOn: boolean) => void;
  requestExit: () => void;

  constructor(
    setPointerVisible: (visible: boolean) => void,
    setSystemPointer: (type: number) => void,
    setCustomPointer: (
      pixels: Uint8Array,
      width: number,
      height: number,
      hotspotX: number,
      hotspotY: number
    ) => void,
    setVirtualKeyboardVisible: (visible: boolean) => void,
    configureWindow: (fullscreen: boolean, keepScreenOn: boolean) => void,
    requestExit: () => void
  ) {
    this.setPointerVisible = setPointerVisible;
    this.setSystemPointer = setSystemPointer;
    this.setCustomPointer = setCustomPointer;
    this.setVirtualKeyboardVisible = setVirtualKeyboardVisible;
    this.configureWindow = configureWindow;
    this.requestExit = requestExit;
  }
}

interface SfmlNativeContext {
  configureRuntimePaths(runtimeRoot: string, userDataRoot: string): void;
  configureSystemLocale(systemLocale: string): void;
  configureDisplayScaleHost(
    configurable: boolean,
    maximumWindowedWidth: number,
    maximumWindowedHeight: number,
    callback: (scale: number, gameWidth: number, gameHeight: number) => void
  ): boolean;
  clearDisplayScaleHost(): void;
  submitKeyEvent(
    keyCode: number,
    pressed: boolean,
    alt: boolean,
    control: boolean,
    shift: boolean,
    system: boolean
  ): boolean;
  submitSystemBack(): boolean;
  initializeHost(
    manager: resourceManager.ResourceManager,
    windowId: number,
    deviceType: string,
    callbacks: SfmlHostCallbacks
  ): boolean;
  onForeground(): void;
  onBackground(): void;
  onXComponentDestroy(): void;
  onDestroy(): void;
  submitText(text: string): void;
  submitKeyText(text: string): void;
  submitTextEdit(backwardDeletions: number, forwardDeletions: number, text: string): boolean;
}

class SurfaceNodeBinding {
  context?: SfmlNativeContext;
  registered: boolean = false;
  retired: boolean = false;
  destroyed: boolean = false;
}

@Component
struct SfmlSurfaceNode {
  private binding: SurfaceNodeBinding = new SurfaceNodeBinding();
  nodeAppeared: (binding: SurfaceNodeBinding) => void = (_binding: SurfaceNodeBinding): void => {};
  surfaceLoaded: (binding: SurfaceNodeBinding) => void = (_binding: SurfaceNodeBinding): void => {};
  surfaceDestroyed: (binding: SurfaceNodeBinding) => void = (_binding: SurfaceNodeBinding): void => {};
  surfaceFocusLost: (binding: SurfaceNodeBinding) => void = (_binding: SurfaceNodeBinding): void => {};
  surfacePreImeKeyEvent: (binding: SurfaceNodeBinding, event: KeyEvent) => boolean = (
    _binding: SurfaceNodeBinding,
    _event: KeyEvent
  ): boolean => false;
  surfaceKeyEvent: (binding: SurfaceNodeBinding, event: KeyEvent) => boolean = (
    _binding: SurfaceNodeBinding,
    _event: KeyEvent
  ): boolean => false;

  aboutToAppear(): void {
    if (!this.binding.registered) {
      this.binding.registered = true;
      this.nodeAppeared(this.binding);
    }
  }

  private handleLoad(context?: object): void {
    if (!context) {
      hilog.error(DOMAIN, TAG, 'XComponent onLoad did not provide a native context');
      return;
    }

    const nextContext = context as SfmlNativeContext;
    if (this.binding.destroyed || this.binding.retired) {
      nextContext.onXComponentDestroy();
      return;
    }

    if (this.binding.context === nextContext) {
      return;
    }
    this.binding.context?.onXComponentDestroy();
    this.binding.context = nextContext;
    this.surfaceLoaded(this.binding);
  }

  private handleDestroy(): void {
    if (this.binding.destroyed) {
      return;
    }
    this.binding.destroyed = true;
    this.surfaceDestroyed(this.binding);
  }

  build() {
    XComponent({
      id: 'sfml-surface',
      type: XComponentType.SURFACE,
      libraryname: 'entry'
    })
      .width('100%')
      .height('100%')
      .focusable(true)
      .defaultFocus(true)
      .onLoad((context?: object) => this.handleLoad(context))
      .onBlur(() => this.surfaceFocusLost(this.binding))
      .onKeyPreIme((event: KeyEvent) => this.surfacePreImeKeyEvent(this.binding, event))
      .onKeyEvent((event: KeyEvent) => this.surfaceKeyEvent(this.binding, event))
      .onDestroy(() => this.handleDestroy());
  }
}

class ImeEditDelta {
  backwardDeletions: number;
  forwardDeletions: number;
  insertedText: string;
  selectionStart: number;
  selectionEnd: number;

  constructor(
    backwardDeletions: number,
    forwardDeletions: number,
    insertedText: string,
    selectionStart: number,
    selectionEnd: number
  ) {
    this.backwardDeletions = backwardDeletions;
    this.forwardDeletions = forwardDeletions;
    this.insertedText = insertedText;
    this.selectionStart = selectionStart;
    this.selectionEnd = selectionEnd;
  }
}

@Entry
@Component
struct Index {
  @StorageLink('sfmlWindowId') @Watch('windowIdChanged') windowId: number = 0;
  @StorageLink('sfmlLifecycle') @Watch('lifecycleChanged') lifecycle: string = 'created';
  @State status: string = 'Preparing the Ludork runtime';
  @State imeText: string = '';
  @State hostInitialized: boolean = false;
  private nativeContext?: SfmlNativeContext;
  private lifecycleContext?: SfmlNativeContext;
  private latestSurfaceBinding?: SurfaceNodeBinding;
  private activeSurfaceBinding?: SurfaceNodeBinding;
  private nativeHostDestroyed: boolean = false;
  private runtimeRoot: string = '';
  private userDataRoot: string = '';
  private runtimePreparation?: Promise<void>;
  private imeController: TextInputController = new TextInputController();
  private imeCommittedText: string = '';
  private imeSelectionStart: number = 0;
  private imeSelectionEnd: number = 0;
  private customPointer?: image.PixelMap;
  private customPointerRequest: number = 0;
  private exitRequested: boolean = false;
  private heldSurfaceKeys: Set<number> = new Set<number>();
  private windowCommandQueue: Promise<void> = Promise.resolve();
  private windowHostGeneration: number = 0;
  private displayScaleRequest: number = 0;
  private keepScreenOn: boolean = true;

  private describeError(error: Error): string {
    const code = (error as BusinessError).code;
    return code ? `${error.message} (code ${code})` : error.message;
  }

  private logHostFailure(operation: string, error: Error): void {
    hilog.warn(DOMAIN, TAG, '%{public}s failed: %{public}s', operation, this.describeError(error));
  }

  private releasePixelMap(pixelMap?: image.PixelMap): void {
    if (!pixelMap) {
      return;
    }
    pixelMap.release().catch((error: Error) => {
      this.logHostFailure('Releasing the custom pointer', error);
    });
  }

  private releaseCustomPointer(): void {
    const previous = this.customPointer;
    this.customPointer = undefined;
    this.releasePixelMap(previous);
  }

  private setPointerVisible(visible: boolean): void {
    try {
      pointer.setPointerVisibleSync(visible);
    } catch (error) {
      this.logHostFailure('Changing pointer visibility', error as Error);
    }
  }

  private setSystemPointer(type: number): void {
    ++this.customPointerRequest;
    const style = SYSTEM_POINTER_STYLES[type] ?? pointer.PointerStyle.DEFAULT;
    try {
      pointer.setPointerStyleSync(this.windowId, style);
      this.releaseCustomPointer();
    } catch (error) {
      this.logHostFailure('Changing the system pointer', error as Error);
    }
  }

  private setCustomPointer(
    pixels: Uint8Array,
    width: number,
    height: number,
    hotspotX: number,
    hotspotY: number
  ): void {
    if (width <= 0 || height <= 0 || pixels.length !== width * height * 4) {
      hilog.warn(DOMAIN, TAG, 'Rejected an invalid custom pointer');
      return;
    }

    const request = ++this.customPointerRequest;
    const options: image.InitializationOptions = {
      size: {
        width: width,
        height: height
      },
      srcPixelFormat: image.PixelMapFormat.RGBA_8888,
      pixelFormat: image.PixelMapFormat.RGBA_8888,
      alphaType: image.AlphaType.UNPREMUL,
      editable: false
    };

    image.createPixelMap(pixels.buffer, options)
      .then((pixelMap: image.PixelMap) => {
        if (request !== this.customPointerRequest) {
          this.releasePixelMap(pixelMap);
          return;
        }

        try {
          pointer.setCustomCursorSync(this.windowId, pixelMap, hotspotX, hotspotY);
          const previous = this.customPointer;
          this.customPointer = pixelMap;
          this.releasePixelMap(previous);
        } catch (error) {
          this.releasePixelMap(pixelMap);
          this.logHostFailure('Changing the custom pointer', error as Error);
        }
      })
      .catch((error: Error) => {
        this.logHostFailure('Creating the custom pointer image', error);
      });
  }

  private setVirtualKeyboardVisible(visible: boolean): void {
    try {
      if (visible) {
        this.resetImeEditor();
        this.getUIContext().getFocusController().requestFocus('sfml-ime');
      } else {
        this.imeController.stopEditing();
        this.getUIContext().getFocusController().requestFocus('sfml-surface');
        this.resetImeEditor();
      }
    } catch (error) {
      this.logHostFailure('Changing virtual keyboard visibility', error as Error);
    }
  }

  private queueWindowCommand(
    context: common.UIAbilityContext,
    operation: string,
    command: (mainWindow: window.Window, generation: number) => Promise<void>
  ): void {
    const generation = this.windowHostGeneration;
    this.windowCommandQueue = this.windowCommandQueue.then(async () => {
      if (generation !== this.windowHostGeneration) {
        return;
      }
      const mainWindow = await window.getLastWindow(context);
      if (generation !== this.windowHostGeneration) {
        return;
      }
      await command(mainWindow, generation);
    }).catch((error: Error) => {
      this.logHostFailure(operation, error);
    });
  }

  private async applyWindowPresentation(
    mainWindow: window.Window,
    fullscreen: boolean,
    keepScreenOn: boolean
  ): Promise<void> {
    const systemBars: Array<'status' | 'navigation'> = fullscreen ? [] : ['status', 'navigation'];
    await Promise.all([
      mainWindow.setWindowLayoutFullScreen(fullscreen),
      mainWindow.setWindowSystemBarEnable(systemBars),
      mainWindow.setWindowKeepScreenOn(keepScreenOn)
    ]);
  }

  private configureWindow(
    context: common.UIAbilityContext,
    fullscreen: boolean,
    keepScreenOn: boolean
  ): void {
    this.keepScreenOn = keepScreenOn;
    this.queueWindowCommand(context, 'Configuring the Stage window', async (
      mainWindow: window.Window,
      generation: number
    ) => {
      if (generation !== this.windowHostGeneration) {
        return;
      }
      await this.applyWindowPresentation(mainWindow, fullscreen, keepScreenOn);
    });
  }

  private requestDisplayScale(
    context: common.UIAbilityContext,
    scale: number,
    gameWidth: number,
    gameHeight: number
  ): void {
    const request = ++this.displayScaleRequest;
    this.queueWindowCommand(context, 'Changing the Stage window scale', async (
      mainWindow: window.Window,
      generation: number
    ) => {
      if (request !== this.displayScaleRequest || generation !== this.windowHostGeneration) {
        return;
      }

      if (scale === 0) {
        const status = mainWindow.getWindowStatus();
        if (status !== window.WindowStatusType.MAXIMIZE &&
          status !== window.WindowStatusType.FULL_SCREEN) {
          await mainWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE);
        }
        if (request !== this.displayScaleRequest || generation !== this.windowHostGeneration) {
          return;
        }
        await this.applyWindowPresentation(mainWindow, true, this.keepScreenOn);
        return;
      }

      if (!Number.isFinite(scale) || scale <= 0 || gameWidth <= 0 || gameHeight <= 0) {
        throw new Error('Invalid display scale request');
      }
      if (mainWindow.getWindowStatus() !== window.WindowStatusType.FLOATING) {
        await mainWindow.recover();
      }
      if (request !== this.displayScaleRequest || generation !== this.windowHostGeneration) {
        return;
      }
      const width = Math.max(1, Math.round(gameWidth * scale));
      const height = Math.max(1, Math.round(gameHeight * scale));
      await mainWindow.resizeAsync(width, height);
      if (request !== this.displayScaleRequest || generation !== this.windowHostGeneration) {
        return;
      }
      await this.applyWindowPresentation(mainWindow, false, this.keepScreenOn);
    });
  }

  private requestExit(context: common.UIAbilityContext): void {
    if (this.exitRequested) {
      return;
    }
    this.exitRequested = true;
    context.terminateSelf().catch((error: Error) => {
      this.exitRequested = false;
      this.logHostFailure('Terminating the UIAbility', error);
    });
  }

  private createHostCallbacks(context: common.UIAbilityContext): SfmlHostCallbacks {
    return new SfmlHostCallbacks(
      (visible: boolean) => this.setPointerVisible(visible),
      (type: number) => this.setSystemPointer(type),
      (pixels: Uint8Array, width: number, height: number, hotspotX: number, hotspotY: number) =>
        this.setCustomPointer(pixels, width, height, hotspotX, hotspotY),
      (visible: boolean) => this.setVirtualKeyboardVisible(visible),
      (fullscreen: boolean, keepScreenOn: boolean) => this.configureWindow(context, fullscreen, keepScreenOn),
      () => this.requestExit(context)
    );
  }

  private isSurfacePreImeKey(keyCode: number): boolean {
    return keyCode === KeyCode.KEYCODE_SPACE || keyCode === KeyCode.KEYCODE_ENTER ||
      keyCode === KeyCode.KEYCODE_NUMPAD_ENTER || keyCode === KeyCode.KEYCODE_ESCAPE;
  }

  private isSurfaceModifierHeld(leftKeyCode: number, rightKeyCode: number): boolean {
    return this.heldSurfaceKeys.has(leftKeyCode) || this.heldSurfaceKeys.has(rightKeyCode);
  }

  private getSurfaceModifierState(
    event: KeyEvent | undefined,
    modifier: string,
    leftKeyCode: number,
    rightKeyCode: number
  ): boolean {
    let reported: boolean = false;
    if (event !== undefined && event.getModifierKeyState !== undefined) {
      reported = event.getModifierKeyState([modifier]);
    }
    return reported || this.isSurfaceModifierHeld(leftKeyCode, rightKeyCode);
  }

  private submitSurfaceKeyEvent(
    context: SfmlNativeContext,
    keyCode: number,
    pressed: boolean,
    event?: KeyEvent
  ): boolean {
    const alt = this.getSurfaceModifierState(
      event, 'Alt', KeyCode.KEYCODE_ALT_LEFT, KeyCode.KEYCODE_ALT_RIGHT
    );
    const control = this.getSurfaceModifierState(
      event, 'Ctrl', KeyCode.KEYCODE_CTRL_LEFT, KeyCode.KEYCODE_CTRL_RIGHT
    );
    const shift = this.getSurfaceModifierState(
      event, 'Shift', KeyCode.KEYCODE_SHIFT_LEFT, KeyCode.KEYCODE_SHIFT_RIGHT
    );
    const system = this.isSurfaceModifierHeld(
      KeyCode.KEYCODE_META_LEFT, KeyCode.KEYCODE_META_RIGHT
    );
    return context.submitKeyEvent(keyCode, pressed, alt, control, shift, system);
  }

  private releaseHeldSurfaceKeys(context?: SfmlNativeContext): void {
    const keyCodes: number[] = Array.from(this.heldSurfaceKeys);
    for (let index = keyCodes.length - 1; index >= 0; --index) {
      const keyCode = keyCodes[index];
      this.heldSurfaceKeys.delete(keyCode);
      if (context !== undefined) {
        this.submitSurfaceKeyEvent(context, keyCode, false);
      }
    }
    this.heldSurfaceKeys.clear();
  }

  private invalidateNativeBridges(context?: SfmlNativeContext): void {
    ++this.windowHostGeneration;
    ++this.displayScaleRequest;
    this.releaseHeldSurfaceKeys(context);
    context?.clearDisplayScaleHost();
  }

  private async removeTree(path: string): Promise<void> {
    const names = await fileIo.listFile(path);
    for (const name of names) {
      const child = `${path}/${name}`;
      const childStat = await fileIo.lstat(child);
      if (childStat.isDirectory()) {
        await this.removeTree(child);
      } else {
        await fileIo.unlink(child);
      }
    }
    await fileIo.rmdir(path);
  }

  private async writeRuntimeMarker(path: string): Promise<void> {
    const marker = await fileIo.open(
      path,
      fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC
    );
    try {
      const written = await fileIo.write(marker.fd, RUNTIME_HASH);
      if (written !== RUNTIME_HASH.length) {
        throw new Error('Unable to write the complete runtime marker');
      }
    } finally {
      await fileIo.close(marker);
    }
  }

  private async copyRuntimeArchive(
    manager: resourceManager.ResourceManager,
    destination: string
  ): Promise<void> {
    const descriptor = await manager.getRawFd(RUNTIME_ARCHIVE);
    try {
      const output = await fileIo.open(
        destination,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC
      );
      try {
        const buffer = new ArrayBuffer(COPY_CHUNK_SIZE);
        let copied = 0;
        while (copied < descriptor.length) {
          const requested = Math.min(COPY_CHUNK_SIZE, descriptor.length - copied);
          const bytesRead = await fileIo.read(descriptor.fd, buffer, {
            offset: descriptor.offset + copied,
            length: requested
          });
          if (bytesRead <= 0 || bytesRead > requested) {
            throw new Error('Unable to read the packaged Ludork runtime');
          }

          let written = 0;
          while (written < bytesRead) {
            const bytesWritten = await fileIo.write(output.fd, buffer.slice(written, bytesRead));
            if (bytesWritten <= 0) {
              throw new Error('Unable to copy the packaged Ludork runtime');
            }
            written += bytesWritten;
          }
          copied += bytesRead;
        }
      } finally {
        await fileIo.close(output);
      }
    } finally {
      await manager.closeRawFd(RUNTIME_ARCHIVE);
    }
  }

  private async installRuntime(
    manager: resourceManager.ResourceManager,
    versionsRoot: string,
    destination: string
  ): Promise<void> {
    const staging = await fileIo.mkdtemp(`${versionsRoot}/.${RUNTIME_HASH}-XXXXXX`);
    const archive = `${staging}.zip`;
    let installed = false;
    try {
      await this.copyRuntimeArchive(manager, archive);
      await zlib.decompressFile(archive, staging);
      await this.writeRuntimeMarker(`${staging}/${RUNTIME_MARKER}`);
      await fileIo.rename(staging, destination);
      installed = true;
    } finally {
      if (await fileIo.access(archive)) {
        await fileIo.unlink(archive);
      }
      if (!installed && await fileIo.access(staging)) {
        await this.removeTree(staging);
      }
    }
  }

  private async removeOldRuntimeVersions(versionsRoot: string): Promise<void> {
    const names = await fileIo.listFile(versionsRoot);
    for (const name of names) {
      if (name === RUNTIME_HASH) {
        continue;
      }
      const path = `${versionsRoot}/${name}`;
      const pathStat = await fileIo.lstat(path);
      if (pathStat.isDirectory()) {
        await this.removeTree(path);
      } else if (name.startsWith('.')) {
        await fileIo.unlink(path);
      }
    }
  }

  private async ensureDirectory(path: string): Promise<void> {
    if (!await fileIo.access(path)) {
      await fileIo.mkdir(path, true);
    }
  }

  private async prepareRuntime(context: common.UIAbilityContext): Promise<void> {
    const ludorkRoot = `${context.filesDir}/ludork`;
    const versionsRoot = `${ludorkRoot}/runtime`;
    const runtimeRoot = `${versionsRoot}/${RUNTIME_HASH}`;
    const userDataRoot = `${ludorkRoot}/user-data`;
    const marker = `${runtimeRoot}/${RUNTIME_MARKER}`;

    await this.ensureDirectory(versionsRoot);
    await this.ensureDirectory(userDataRoot);
    if (!await fileIo.access(marker)) {
      await this.installRuntime(context.resourceManager, versionsRoot, runtimeRoot);
    }
    await this.removeOldRuntimeVersions(versionsRoot);

    this.runtimeRoot = runtimeRoot;
    this.userDataRoot = userDataRoot;
  }

  private windowIdChanged(): void {
    this.tryInitializeNative();
  }

  private lifecycleChanged(): void {
    if (this.lifecycle === 'destroyed' || this.lifecycle === 'surface-destroyed') {
      this.destroyNativeHost();
      return;
    }
    const context = this.nativeContext ?? this.lifecycleContext;
    if (!context) {
      return;
    }

    if (this.lifecycle === 'foreground') {
      context.onForeground();
    } else if (this.lifecycle === 'background') {
      this.releaseHeldSurfaceKeys(context);
      context.onBackground();
    }
  }

  private tryInitializeNative(): void {
    try {
      this.initializeNativeHost();
    } catch (error) {
      this.status = `Unable to initialize the Ludork runtime: ${this.describeError(error as Error)}`;
      hilog.error(DOMAIN, TAG, '%{public}s', this.status);
    }
  }

  private initializeNativeHost(): void {
    const nativeContext = this.nativeContext;
    if (!nativeContext || this.hostInitialized) {
      return;
    }
    if (this.windowId <= 0) {
      this.status = 'Waiting for the Stage window ID';
      return;
    }

    const hostContext = this.getUIContext().getHostContext();
    if (!hostContext) {
      this.status = 'Unable to obtain the UIAbility context';
      return;
    }
    const abilityContext = hostContext as common.UIAbilityContext;
    if (this.runtimeRoot.length === 0 || this.userDataRoot.length === 0) {
      if (!this.runtimePreparation) {
        this.status = 'Preparing the Ludork runtime';
        this.runtimePreparation = this.prepareRuntime(abilityContext);
        this.runtimePreparation.then(
          () => {
            this.tryInitializeNative();
          },
          (error: Error) => {
            this.status = `Unable to prepare the Ludork runtime: ${this.describeError(error)}`;
            hilog.error(DOMAIN, TAG, '%{public}s', this.status);
          }
        );
      }
      return;
    }

    const systemLocale = i18n.System.getSystemLocaleInstance();
    const region: string = systemLocale.region ?? '';
    const systemLocaleName: string = region.length > 0 ?
      `${systemLocale.language}_${region}` : systemLocale.language;
    hilog.info(DOMAIN, TAG, 'Configuring system locale %{public}s', systemLocaleName);
    nativeContext.configureRuntimePaths(this.runtimeRoot, this.userDataRoot);
    nativeContext.configureSystemLocale(systemLocaleName);
    const hostGeneration = ++this.windowHostGeneration;
    const displayScaleConfigurable = deviceInfo.deviceType === 'tablet';
    let maximumWindowedWidth = 0;
    let maximumWindowedHeight = 0;
    if (displayScaleConfigurable) {
      try {
        const mainWindow = abilityContext.windowStage.getMainWindowSync();
        const displayId = mainWindow.getWindowProperties().displayId;
        const hostDisplay = displayId === undefined ? display.getDefaultDisplaySync() :
          display.getDisplayByIdSync(displayId);
        const windowLimits = mainWindow.getWindowLimits();
        const limitWidth = windowLimits.maxWidth !== undefined && windowLimits.maxWidth > 0 ?
          windowLimits.maxWidth : hostDisplay.availableWidth;
        const limitHeight = windowLimits.maxHeight !== undefined && windowLimits.maxHeight > 0 ?
          windowLimits.maxHeight : hostDisplay.availableHeight;
        maximumWindowedWidth = Math.max(0, Math.min(hostDisplay.availableWidth, limitWidth));
        maximumWindowedHeight = Math.max(0, Math.min(hostDisplay.availableHeight, limitHeight));
      } catch (error) {
        this.logHostFailure('Obtaining maximum window size', error as Error);
      }
    }
    nativeContext.configureDisplayScaleHost(
      displayScaleConfigurable,
      maximumWindowedWidth,
      maximumWindowedHeight,
      (scale: number, gameWidth: number, gameHeight: number) => {
        if (hostGeneration === this.windowHostGeneration) {
          this.requestDisplayScale(abilityContext, scale, gameWidth, gameHeight);
        }
      }
    );
    const initialized = nativeContext.initializeHost(
      abilityContext.resourceManager,
      this.windowId,
      deviceInfo.deviceType,
      this.createHostCallbacks(abilityContext)
    );
    if (!initialized) {
      nativeContext.clearDisplayScaleHost();
    }

    this.hostInitialized = initialized;
    this.status = initialized ? '' : 'Unsupported device or native host failure';
    if (this.lifecycle === 'foreground') {
      nativeContext.onForeground();
    } else if (this.lifecycle === 'background') {
      nativeContext.onBackground();
    }
    if (this.status.length > 0) {
      hilog.error(DOMAIN, TAG, '%{public}s', this.status);
    }
  }

  private registerSurfaceNode(binding: SurfaceNodeBinding): void {
    this.latestSurfaceBinding = binding;
  }

  private initializeNative(binding: SurfaceNodeBinding): boolean {
    const nextContext = binding.context;
    if (!nextContext) {
      hilog.error(DOMAIN, TAG, 'XComponent load binding has no native context');
      return false;
    }

    if (binding !== this.latestSurfaceBinding || binding.destroyed || binding.retired ||
      this.nativeHostDestroyed || this.lifecycle === 'destroyed' || this.lifecycle === 'surface-destroyed') {
      binding.context = undefined;
      binding.retired = true;
      nextContext.onXComponentDestroy();
      return false;
    }

    const previousBinding = this.activeSurfaceBinding;
    if (previousBinding && previousBinding !== binding) {
      const previousContext = previousBinding.context;
      previousBinding.context = undefined;
      previousBinding.retired = true;
      this.invalidateNativeBridges(previousContext);
      previousContext?.onXComponentDestroy();
    }

    this.activeSurfaceBinding = binding;
    this.nativeContext = nextContext;
    this.lifecycleContext = nextContext;
    this.tryInitializeNative();
    return true;
  }

  private destroyNativeHost(fallbackContext?: SfmlNativeContext): void {
    if (this.nativeHostDestroyed) {
      return;
    }
    this.nativeHostDestroyed = true;
    const context = this.nativeContext ?? this.lifecycleContext ?? fallbackContext;
    this.nativeContext = undefined;
    this.lifecycleContext = undefined;
    this.latestSurfaceBinding = undefined;
    this.activeSurfaceBinding = undefined;
    this.hostInitialized = false;
    ++this.customPointerRequest;
    this.releaseCustomPointer();
    this.invalidateNativeBridges(context);
    context?.onDestroy();
  }

  private handleSurfaceNodeDestroy(binding: SurfaceNodeBinding): void {
    const context = binding.context;
    binding.context = undefined;
    binding.retired = true;
    const wasActive = binding === this.activeSurfaceBinding;

    if (this.lifecycle === 'destroyed' || this.lifecycle === 'surface-destroyed') {
      this.destroyNativeHost(context);
      return;
    }

    if (wasActive) {
      this.invalidateNativeBridges(context);
    }
    context?.onXComponentDestroy();
    if (!wasActive) {
      return;
    }

    this.nativeContext = undefined;
    this.activeSurfaceBinding = undefined;
    this.hostInitialized = false;
    ++this.customPointerRequest;
    this.releaseCustomPointer();
    this.status = 'Native surface destroyed';
  }

  private handleSurfaceNodeLoad(binding: SurfaceNodeBinding): void {
    this.initializeNative(binding);
  }

  private handleSurfaceFocusLost(binding: SurfaceNodeBinding): void {
    if (binding === this.activeSurfaceBinding) {
      this.releaseHeldSurfaceKeys(this.nativeContext);
    }
  }

  private handleImeKeyEvent(event: KeyEvent): boolean {
    const binding = this.activeSurfaceBinding;
    if (binding === undefined || event.keySource === KeySource.JOYSTICK) {
      return false;
    }
    if (!this.isSurfacePreImeKey(event.keyCode)) {
      return false;
    }
    const isEnter = event.keyCode === KeyCode.KEYCODE_ENTER ||
      event.keyCode === KeyCode.KEYCODE_NUMPAD_ENTER;
    if (event.type === KeyType.Down && isEnter &&
      !this.heldSurfaceKeys.has(event.keyCode)) {
      this.nativeContext?.submitTextEdit(0, 0, '\r');
    }
    const consumeEvent = isEnter || event.keyCode === KeyCode.KEYCODE_ESCAPE;
    return this.handleSurfaceKeyEvent(binding, event, false, consumeEvent);
  }

  private handleSurfacePreImeKeyEvent(binding: SurfaceNodeBinding, event: KeyEvent): boolean {
    if (binding !== this.activeSurfaceBinding || event.keySource === KeySource.JOYSTICK ||
      !this.isSurfacePreImeKey(event.keyCode)) {
      return false;
    }
    return this.handleSurfaceKeyEvent(binding, event);
  }

  private handleSurfaceKeyEvent(
    binding: SurfaceNodeBinding,
    event: KeyEvent,
    forwardKeyText: boolean = true,
    consumeEvent: boolean = true
  ): boolean {
    if (binding !== this.activeSurfaceBinding) {
      return false;
    }
    if (forwardKeyText && event.type === KeyType.Down && event.keyCode !== KeyCode.KEYCODE_UNKNOWN &&
      event.keySource !== KeySource.JOYSTICK && event.keyText.length > 0) {
      this.nativeContext?.submitKeyText(event.keyText);
    }

    if (event.keySource === KeySource.JOYSTICK) {
      return false;
    }
    const context = this.nativeContext;
    if (context === undefined) {
      return false;
    }
    let forwarded = false;
    if (event.type === KeyType.Down) {
      if (this.heldSurfaceKeys.has(event.keyCode)) {
        forwarded = true;
      } else {
        this.heldSurfaceKeys.add(event.keyCode);
        forwarded = this.submitSurfaceKeyEvent(context, event.keyCode, true, event);
        if (!forwarded) {
          this.heldSurfaceKeys.delete(event.keyCode);
        }
      }
    } else if (event.type === KeyType.Up && this.heldSurfaceKeys.delete(event.keyCode)) {
      forwarded = this.submitSurfaceKeyEvent(context, event.keyCode, false, event);
    }
    if (forwarded && consumeEvent) {
      event.stopPropagation();
    }
    return forwarded && consumeEvent;
  }

  onBackPress(): boolean {
    this.nativeContext?.submitSystemBack();
    return true;
  }

  private resetImeEditor(): void {
    this.imeCommittedText = '';
    this.imeSelectionStart = 0;
    this.imeSelectionEnd = 0;
    this.imeText = '';
  }

  private clampImeOffset(value: number | undefined, length: number, fallback: number): number {
    if (value === undefined || !Number.isFinite(value)) {
      return fallback;
    }
    return Math.max(0, Math.min(length, Math.trunc(value)));
  }

  private isHighSurrogate(value: number): boolean {
    return value >= 0xD800 && value <= 0xDBFF;
  }

  private isLowSurrogate(value: number): boolean {
    return value >= 0xDC00 && value <= 0xDFFF;
  }

  private isImeBoundary(text: string, offset: number): boolean {
    return offset <= 0 || offset >= text.length ||
      !this.isHighSurrogate(text.charCodeAt(offset - 1)) || !this.isLowSurrogate(text.charCodeAt(offset));
  }

  private countImeCodePoints(text: string): number {
    let count = 0;
    let offset = 0;
    while (offset < text.length) {
      const first = text.charCodeAt(offset++);
      if (this.isHighSurrogate(first) && offset < text.length && this.isLowSurrogate(text.charCodeAt(offset))) {
        ++offset;
      }
      ++count;
    }
    return count;
  }

  private classifyImeDeletion(start: number, end: number, count: number): ImeEditDelta {
    let backward = count;
    let forward = 0;
    if (count > 0 && this.imeSelectionStart === this.imeSelectionEnd &&
      start === this.imeSelectionEnd && end > this.imeSelectionEnd) {
      backward = 0;
      forward = count;
    }
    return new ImeEditDelta(backward, forward, '', start, start);
  }

  private deriveImeEdit(value: string, options?: TextChangeOptions): ImeEditDelta {
    const previous = this.imeCommittedText;
    let beforeStart = 0;
    let beforeEnd = 0;
    let afterStart = 0;
    let afterEnd = 0;
    let hasExactRanges = false;

    if (options !== undefined && options.oldContent === previous) {
      beforeStart = this.clampImeOffset(options.rangeBefore.start, previous.length, 0);
      beforeEnd = this.clampImeOffset(options.rangeBefore.end, previous.length, previous.length);
      afterStart = this.clampImeOffset(options.rangeAfter.start, value.length, 0);
      afterEnd = this.clampImeOffset(options.rangeAfter.end, value.length, value.length);
      hasExactRanges = beforeStart <= beforeEnd && afterStart <= afterEnd &&
        this.isImeBoundary(previous, beforeStart) && this.isImeBoundary(previous, beforeEnd) &&
        this.isImeBoundary(value, afterStart) && this.isImeBoundary(value, afterEnd) &&
        previous.substring(0, beforeStart) === value.substring(0, afterStart) &&
        previous.substring(beforeEnd) === value.substring(afterEnd);
    }

    if (!hasExactRanges) {
      while (beforeStart < previous.length && beforeStart < value.length &&
        previous.charCodeAt(beforeStart) === value.charCodeAt(beforeStart)) {
        ++beforeStart;
      }
      while (beforeStart > 0 &&
        (!this.isImeBoundary(previous, beforeStart) || !this.isImeBoundary(value, beforeStart))) {
        --beforeStart;
      }

      beforeEnd = previous.length;
      afterEnd = value.length;
      while (beforeEnd > beforeStart && afterEnd > beforeStart &&
        previous.charCodeAt(beforeEnd - 1) === value.charCodeAt(afterEnd - 1)) {
        --beforeEnd;
        --afterEnd;
      }
      while ((!this.isImeBoundary(previous, beforeEnd) || !this.isImeBoundary(value, afterEnd)) &&
        beforeEnd < previous.length && afterEnd < value.length) {
        ++beforeEnd;
        ++afterEnd;
      }
      afterStart = beforeStart;
    }

    const removed = previous.substring(beforeStart, beforeEnd);
    const inserted = value.substring(afterStart, afterEnd);
    const deletion = this.classifyImeDeletion(beforeStart, beforeEnd, this.countImeCodePoints(removed));
    const nextSelection = afterStart + inserted.length;
    return new ImeEditDelta(
      deletion.backwardDeletions,
      deletion.forwardDeletions,
      inserted,
      nextSelection,
      nextSelection
    );
  }

  private handleImeChange(value: string, options?: TextChangeOptions): void {
    const edit = this.deriveImeEdit(value, options);
    this.imeCommittedText = value;
    this.imeText = value;
    this.imeSelectionStart = edit.selectionStart;
    this.imeSelectionEnd = edit.selectionEnd;

    if (edit.backwardDeletions === 0 && edit.forwardDeletions === 0 && edit.insertedText.length === 0) {
      return;
    }

    this.nativeContext?.submitTextEdit(
      edit.backwardDeletions,
      edit.forwardDeletions,
      edit.insertedText
    );
  }

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      SfmlSurfaceNode({
        nodeAppeared: (binding: SurfaceNodeBinding) => this.registerSurfaceNode(binding),
        surfaceLoaded: (binding: SurfaceNodeBinding) => this.handleSurfaceNodeLoad(binding),
        surfaceDestroyed: (binding: SurfaceNodeBinding) => this.handleSurfaceNodeDestroy(binding),
        surfaceFocusLost: (binding: SurfaceNodeBinding) => this.handleSurfaceFocusLost(binding),
        surfacePreImeKeyEvent: (binding: SurfaceNodeBinding, event: KeyEvent) =>
          this.handleSurfacePreImeKeyEvent(binding, event),
        surfaceKeyEvent: (binding: SurfaceNodeBinding, event: KeyEvent) =>
          this.handleSurfaceKeyEvent(binding, event)
      })
        .width('100%')
        .height('100%');

      TextInput({ text: this.imeText, controller: this.imeController })
        .id('sfml-ime')
        .width(1)
        .height(1)
        .opacity(0.01)
        .focusable(true)
        .maxLength(8192)
        .onBlur(() => {
          this.releaseHeldSurfaceKeys(this.nativeContext);
        })
        .onKeyPreIme((event: KeyEvent) => this.handleImeKeyEvent(event))
        .onChange((value: string, _previewText?: PreviewText, options?: TextChangeOptions) => {
          this.handleImeChange(value, options);
        })
        .onTextSelectionChange((selectionStart: number, selectionEnd: number) => {
          this.imeSelectionStart = selectionStart;
          this.imeSelectionEnd = selectionEnd;
        })
        .onSubmit(() => {
          this.nativeContext?.submitTextEdit(0, 0, '\r');
        });

      if (this.status.length > 0) {
        Text(this.status)
          .fontSize(12)
          .fontColor('#D1D5DB')
          .backgroundColor('#80000000')
          .padding(6)
          .margin(8)
          .hitTestBehavior(HitTestMode.None);
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#111827');
  }
}
