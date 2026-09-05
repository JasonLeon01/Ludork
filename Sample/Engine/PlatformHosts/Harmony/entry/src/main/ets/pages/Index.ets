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
const GRAPHICS_API: string = '__LUDORK_GRAPHICS_API__';

enum SfmlWindowCommandType {
  Configure = 0,
  Restore = 1,
  SetPosition = 2,
  SetSize = 3,
  SetMinimumSize = 4,
  SetMaximumSize = 5,
  SetTitle = 6,
  SetIcon = 7,
  SetVisible = 8,
  RequestFocus = 9
}

class SfmlWindowCommand {
  requestId: number = 0;
  timeoutMs: number = 0;
  type: SfmlWindowCommandType = SfmlWindowCommandType.Configure;
  x: number = 0;
  y: number = 0;
  width: number = 0;
  height: number = 0;
  hasSizeLimit: boolean = false;
  limitWidth: number = 0;
  limitHeight: number = 0;
  title: string = '';
  iconWidth: number = 0;
  iconHeight: number = 0;
  iconPixels: Uint8Array = new Uint8Array();
  style: number = 0;
  fullscreen: boolean = false;
  visible: boolean = true;
  keepScreenOn: boolean = false;
  expiresAt: number = 0;
}

class SfmlWindowState {
  x: number;
  y: number;
  width: number;
  height: number;
  visible: boolean;
  focused: boolean;
  fullscreen: boolean;

  constructor(
    x: number,
    y: number,
    width: number,
    height: number,
    visible: boolean,
    focused: boolean,
    fullscreen: boolean
  ) {
    this.x = x;
    this.y = y;
    this.width = width;
    this.height = height;
    this.visible = visible;
    this.focused = focused;
    this.fullscreen = fullscreen;
  }
}

class SfmlWindowPosition {
  x: number;
  y: number;

  constructor(x: number, y: number) {
    this.x = x;
    this.y = y;
  }
}

class SfmlWindowBaseline {
  rect: window.Rect;
  limits: window.WindowLimits;
  decorVisible: boolean;

  constructor(rect: window.Rect, limits: window.WindowLimits, decorVisible: boolean) {
    this.rect = rect;
    this.limits = limits;
    this.decorVisible = decorVisible;
  }
}

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
  executeWindowCommand: (command: SfmlWindowCommand) => void;
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
    executeWindowCommand: (command: SfmlWindowCommand) => void,
    requestExit: () => void
  ) {
    this.setPointerVisible = setPointerVisible;
    this.setSystemPointer = setSystemPointer;
    this.setCustomPointer = setCustomPointer;
    this.setVirtualKeyboardVisible = setVirtualKeyboardVisible;
    this.configureWindow = configureWindow;
    this.executeWindowCommand = executeWindowCommand;
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
  completeWindowCommand(
    requestId: number,
    success: boolean,
    x: number,
    y: number,
    width: number,
    height: number,
    visible: boolean,
    focused: boolean,
    fullscreen: boolean
  ): boolean;
  isWindowCommandPending(requestId: number): boolean;
  updateWindowState(
    x: number,
    y: number,
    width: number,
    height: number,
    visible: boolean,
    focused: boolean,
    fullscreen: boolean
  ): boolean;
  submitText(text: string): void;
  submitKeyText(text: string): void;
  submitTextEdit(backwardDeletions: number, forwardDeletions: number, text: string): boolean;
}

class SurfaceNodeBinding {
  context?: SfmlNativeContext;
  requestFocus?: () => void;
  registered: boolean = false;
  retired: boolean = false;
  destroyed: boolean = false;
}

@Component
struct SfmlSurfaceNode {
  private binding: SurfaceNodeBinding = new SurfaceNodeBinding();
  nodeAppeared: (binding: SurfaceNodeBinding) => void = (_binding: SurfaceNodeBinding): void => {};
  surfaceLoaded: (binding: SurfaceNodeBinding) => void = (_binding: SurfaceNodeBinding): void => {};
  surfaceLoadFailed: (binding: SurfaceNodeBinding) => void = (_binding: SurfaceNodeBinding): void => {};
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
    this.binding.requestFocus = (): void => {
      this.getUIContext().getFocusController().requestFocus('sfml-surface-focus');
    };
    if (!this.binding.registered) {
      this.binding.registered = true;
      this.nodeAppeared(this.binding);
    }
  }

  private handleLoad(context?: object): void {
    if (!context) {
      this.surfaceLoadFailed(this.binding);
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
    this.binding.requestFocus = undefined;
    this.surfaceDestroyed(this.binding);
  }

  build() {
    XComponent({
      id: 'sfml-surface',
      type: XComponentType.SURFACE,
      libraryname: 'entry'
    })
      .id('sfml-surface-focus')
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
  private mainWindow?: window.Window;
  private windowBaseline?: SfmlWindowBaseline;
  private windowStateObserversRegistered: boolean = false;
  private windowFocused: boolean = false;
  private windowStateGeneration: number = 0;
  private windowStatePublishSuppressed: boolean = false;
  private windowStatePublishDirty: boolean = false;
  private confirmedWindowTitle: string = "__LUDORK_GAME_NAME__";
  private confirmedWindowIcon?: SfmlWindowCommand;
  private confirmedWindowStyle: number = 0;
  private confirmedKeepScreenOn: boolean = false;
  private pendingFloatingPosition?: SfmlWindowPosition;
  private suspendPendingFloatingPosition: boolean = false;
  private windowCommandQueue: Promise<void> = Promise.resolve();
  private windowHostGeneration: number = 0;
  private displayScaleRequest: number = 0;
  private freeWindowModeWindow?: window.Window;
  private freeWindowModeChangeCallback?: Callback<boolean>;
  private freeWindowModeListenerGeneration: number = 0;
  private displayScaleConfigurable: boolean = false;
  private displayScaleHostConfigured: boolean = false;
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
        this.getUIContext().getFocusController().requestFocus('sfml-surface-focus');
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

  private queryWindowState(mainWindow: window.Window): SfmlWindowState {
    const properties = mainWindow.getWindowProperties();
    const rect = properties.globalDisplayRect ?? properties.windowRect;
    const drawable = properties.drawableRect;
    const status = mainWindow.getWindowStatus();
    this.windowFocused = mainWindow.isFocused();
    return new SfmlWindowState(
      rect.left,
      rect.top,
      drawable.width,
      drawable.height,
      mainWindow.isWindowShowing() && status !== window.WindowStatusType.MINIMIZE,
      this.windowFocused,
      properties.isFullScreen || properties.isLayoutFullScreen ||
        status === window.WindowStatusType.FULL_SCREEN
    );
  }

  private publishWindowState(mainWindow: window.Window): void {
    if (this.windowStatePublishSuppressed) {
      this.windowStatePublishDirty = true;
      return;
    }

    const nativeContext = this.nativeContext ?? this.lifecycleContext;
    if (!nativeContext || this.nativeHostDestroyed) {
      return;
    }

    try {
      const state = this.queryWindowState(mainWindow);
      nativeContext.updateWindowState(
        state.x,
        state.y,
        state.width,
        state.height,
        state.visible,
        state.focused,
        state.fullscreen
      );
    } catch (error) {
      this.logHostFailure('Publishing Stage window state', error as Error);
    }
  }

  private bindWindowStateObservers(mainWindow: window.Window): void {
    if (this.mainWindow === mainWindow && this.windowStateObserversRegistered) {
      return;
    }
    this.unbindWindowStateObservers();
    this.mainWindow = mainWindow;

    try {
      mainWindow.on('rectChangeInGlobalDisplay', (_options: window.RectChangeOptions) => {
        ++this.windowStateGeneration;
        this.publishWindowState(mainWindow);
      });
      mainWindow.on('windowStatusDidChange', (status: window.WindowStatusType) => {
        ++this.windowStateGeneration;
        this.publishWindowState(mainWindow);
        if (status === window.WindowStatusType.FLOATING && this.pendingFloatingPosition &&
          !this.suspendPendingFloatingPosition) {
          this.applyPendingFloatingPosition(mainWindow).catch((error: Error) => {
            this.logHostFailure('Applying deferred floating window position', error);
          });
        }
      });
      mainWindow.on('windowEvent', (event: window.WindowEventType) => {
        ++this.windowStateGeneration;
        if (event === window.WindowEventType.WINDOW_ACTIVE) {
          this.windowFocused = true;
        } else if (event === window.WindowEventType.WINDOW_INACTIVE ||
          event === window.WindowEventType.WINDOW_HIDDEN ||
          event === window.WindowEventType.WINDOW_DESTROYED) {
          this.windowFocused = false;
        }
        this.publishWindowState(mainWindow);
      });
      this.windowStateObserversRegistered = true;
      this.publishWindowState(mainWindow);
    } catch (error) {
      this.logHostFailure('Registering Stage window state observers', error as Error);
    }
  }

  private unbindWindowStateObservers(): void {
    const mainWindow = this.mainWindow;
    this.mainWindow = undefined;
    if (!mainWindow) {
      this.windowStateObserversRegistered = false;
      return;
    }

    try {
      mainWindow.off('rectChangeInGlobalDisplay');
    } catch (error) {
      this.logHostFailure('Unregistering global window rect observer', error as Error);
    }
    try {
      mainWindow.off('windowStatusDidChange');
    } catch (error) {
      this.logHostFailure('Unregistering window status observer', error as Error);
    }
    try {
      mainWindow.off('windowEvent');
    } catch (error) {
      this.logHostFailure('Unregistering window event observer', error as Error);
    }
    this.windowStateObserversRegistered = false;
  }

  private captureWindowBaseline(mainWindow: window.Window): void {
    if (this.windowBaseline) {
      return;
    }

    const properties = mainWindow.getWindowProperties();
    const currentRect = properties.globalDisplayRect ?? properties.windowRect;
    const rect: window.Rect = {
      left: currentRect.left,
      top: currentRect.top,
      width: currentRect.width,
      height: currentRect.height
    };
    const currentLimits = mainWindow.getWindowLimits();
    const limits: window.WindowLimits = {
      minWidth: currentLimits.minWidth,
      minHeight: currentLimits.minHeight,
      maxWidth: currentLimits.maxWidth,
      maxHeight: currentLimits.maxHeight
    };
    this.windowBaseline = new SfmlWindowBaseline(rect, limits, mainWindow.getWindowDecorVisible());
  }

  private cloneWindowBaseline(): SfmlWindowBaseline | undefined {
    const baseline = this.windowBaseline;
    if (!baseline) {
      return undefined;
    }

    const rect: window.Rect = {
      left: baseline.rect.left,
      top: baseline.rect.top,
      width: baseline.rect.width,
      height: baseline.rect.height
    };
    const limits: window.WindowLimits = {
      minWidth: baseline.limits.minWidth,
      minHeight: baseline.limits.minHeight,
      maxWidth: baseline.limits.maxWidth,
      maxHeight: baseline.limits.maxHeight
    };
    return new SfmlWindowBaseline(rect, limits, baseline.decorVisible);
  }

  private setFullscreenPolicy(
    mainWindow: window.Window,
    fullscreen: boolean,
    keepScreenOn: boolean
  ): Promise<void> {
    const systemBars: Array<'status' | 'navigation'> = fullscreen ? [] : ['status', 'navigation'];
    if (fullscreen) {
      mainWindow.setWindowDecorVisible(false);
      const alreadyMaximized = mainWindow.getWindowStatus() === window.WindowStatusType.MAXIMIZE ||
        mainWindow.getWindowStatus() === window.WindowStatusType.FULL_SCREEN;
      return Promise.all([
        mainWindow.setWindowLayoutFullScreen(true),
        mainWindow.setWindowSystemBarEnable(systemBars),
        mainWindow.setWindowKeepScreenOn(keepScreenOn)
      ]).then(() => alreadyMaximized ? Promise.resolve() :
        mainWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE)).then((): void => {});
    }

    let recover: Promise<void> = Promise.resolve();
    if (mainWindow.getWindowStatus() === window.WindowStatusType.MINIMIZE) {
      recover = mainWindow.restore();
    }
    recover = recover.then(() => mainWindow.getWindowStatus() === window.WindowStatusType.FLOATING ?
      Promise.resolve() : mainWindow.recover());
    return recover.then(() => Promise.all([
      mainWindow.setWindowLayoutFullScreen(fullscreen),
      mainWindow.setWindowSystemBarEnable(systemBars),
      mainWindow.setWindowKeepScreenOn(keepScreenOn)
    ])).then((): void => {});
  }

  private applyPendingFloatingPosition(mainWindow: window.Window): Promise<void> {
    const pending = this.pendingFloatingPosition;
    if (!pending || mainWindow.getWindowStatus() !== window.WindowStatusType.FLOATING) {
      return Promise.resolve();
    }
    this.pendingFloatingPosition = undefined;
    return mainWindow.moveWindowToGlobalDisplay(pending.x, pending.y).catch((error: Error) => {
      this.pendingFloatingPosition = pending;
      throw error;
    });
  }

  private applyWindowStyle(mainWindow: window.Window, style: number): Promise<void> {
    const decorated = (style & 0x7) !== 0;
    const resizable = (style & 0x2) !== 0;
    const closeable = (style & 0x4) !== 0;
    mainWindow.setWindowDecorVisible(decorated);
    mainWindow.setWindowTitleMoveEnabled(decorated);
    mainWindow.setWindowTitleButtonVisible(resizable, decorated, closeable);
    return mainWindow.setResizeByDragEnabled(resizable);
  }

  private resizeClientArea(mainWindow: window.Window, width: number, height: number): Promise<void> {
    const properties = mainWindow.getWindowProperties();
    const horizontalDecor = Math.max(0, properties.windowRect.width - properties.drawableRect.width);
    const verticalDecor = Math.max(0, properties.windowRect.height - properties.drawableRect.height);
    return mainWindow.resize(
      Math.max(1, Math.trunc(width) + horizontalDecor),
      Math.max(1, Math.trunc(height) + verticalDecor)
    );
  }

  private setWindowSizeLimit(
    mainWindow: window.Window,
    command: SfmlWindowCommand,
    minimum: boolean
  ): Promise<window.WindowLimits> {
    const current = mainWindow.getWindowLimits();
    const limits: window.WindowLimits = {
      minWidth: current.minWidth,
      minHeight: current.minHeight,
      maxWidth: current.maxWidth,
      maxHeight: current.maxHeight
    };
    const properties = mainWindow.getWindowProperties();
    const horizontalDecor = Math.max(0, properties.windowRect.width - properties.drawableRect.width);
    const verticalDecor = Math.max(0, properties.windowRect.height - properties.drawableRect.height);
    const baseline = this.windowBaseline?.limits;

    if (minimum) {
      limits.minWidth = command.hasSizeLimit ? command.limitWidth + horizontalDecor : baseline?.minWidth;
      limits.minHeight = command.hasSizeLimit ? command.limitHeight + verticalDecor : baseline?.minHeight;
    } else {
      limits.maxWidth = command.hasSizeLimit ? command.limitWidth + horizontalDecor : baseline?.maxWidth;
      limits.maxHeight = command.hasSizeLimit ? command.limitHeight + verticalDecor : baseline?.maxHeight;
    }
    return mainWindow.setWindowLimits(limits);
  }

  private async setMissionWindowIcon(
    context: common.UIAbilityContext,
    command: SfmlWindowCommand
  ): Promise<void> {
    if (command.iconWidth <= 0 || command.iconHeight <= 0 ||
      command.iconPixels.length !== command.iconWidth * command.iconHeight * 4) {
      throw new Error('Invalid RGBA mission window icon payload');
    }

    const options: image.InitializationOptions = {
      size: { width: command.iconWidth, height: command.iconHeight },
      srcPixelFormat: image.PixelMapFormat.RGBA_8888,
      pixelFormat: image.PixelMapFormat.RGBA_8888,
      alphaType: image.AlphaType.UNPREMUL,
      editable: false
    };
    const pixelMap = await image.createPixelMap(command.iconPixels.buffer, options);
    try {
      await context.setMissionWindowIcon(pixelMap);
    } finally {
      await pixelMap.release();
    }
  }

  private async restoreWindowBaseline(mainWindow: window.Window): Promise<void> {
    const baseline = this.windowBaseline;
    this.suspendPendingFloatingPosition = true;
    try {
      await this.setFullscreenPolicy(mainWindow, false, false);
      if (mainWindow.getWindowStatus() === window.WindowStatusType.MINIMIZE) {
        await mainWindow.restore();
      }

      if (baseline) {
        mainWindow.setWindowDecorVisible(baseline.decorVisible);
        mainWindow.setWindowTitleMoveEnabled(baseline.decorVisible);
        mainWindow.setWindowTitleButtonVisible(true, true, true);
        await mainWindow.setResizeByDragEnabled(true);
        await mainWindow.setWindowLimits(baseline.limits);
        await mainWindow.resize(baseline.rect.width, baseline.rect.height);
        await mainWindow.moveWindowToGlobalDisplay(baseline.rect.left, baseline.rect.top);
      }
    } finally {
      this.suspendPendingFloatingPosition = false;
    }
    await this.applyPendingFloatingPosition(mainWindow);
  }

  private async applyWindowCommand(
    context: common.UIAbilityContext,
    mainWindow: window.Window,
    command: SfmlWindowCommand
  ): Promise<void> {
    switch (command.type) {
      case SfmlWindowCommandType.Configure:
        this.captureWindowBaseline(mainWindow);
        await mainWindow.setWindowTitle(command.title);
        if (command.fullscreen) {
          await this.applyWindowStyle(mainWindow, command.style);
          await this.setFullscreenPolicy(mainWindow, true, command.keepScreenOn);
        } else {
          this.suspendPendingFloatingPosition = true;
          try {
            await this.setFullscreenPolicy(mainWindow, false, command.keepScreenOn);
            await this.applyWindowStyle(mainWindow, command.style);
            if (command.width > 0 && command.height > 0) {
              await this.resizeClientArea(mainWindow, command.width, command.height);
            }
          } finally {
            this.suspendPendingFloatingPosition = false;
          }
          await this.applyPendingFloatingPosition(mainWindow);
        }
        if (command.visible) {
          await mainWindow.showWindow({ focusOnShow: true });
        }
        break;
      case SfmlWindowCommandType.Restore:
        await this.restoreWindowBaseline(mainWindow);
        break;
      case SfmlWindowCommandType.SetPosition:
        if (mainWindow.getWindowStatus() === window.WindowStatusType.FLOATING) {
          await mainWindow.moveWindowToGlobalDisplay(command.x, command.y);
        } else {
          this.pendingFloatingPosition = new SfmlWindowPosition(command.x, command.y);
        }
        break;
      case SfmlWindowCommandType.SetSize:
        await this.resizeClientArea(mainWindow, command.width, command.height);
        break;
      case SfmlWindowCommandType.SetMinimumSize:
        await this.setWindowSizeLimit(mainWindow, command, true);
        break;
      case SfmlWindowCommandType.SetMaximumSize:
        await this.setWindowSizeLimit(mainWindow, command, false);
        break;
      case SfmlWindowCommandType.SetTitle:
        await mainWindow.setWindowTitle(command.title);
        break;
      case SfmlWindowCommandType.SetIcon:
        await this.setMissionWindowIcon(context, command);
        break;
      case SfmlWindowCommandType.SetVisible:
        if (command.visible) {
          if (mainWindow.getWindowStatus() === window.WindowStatusType.MINIMIZE) {
            await mainWindow.restore();
          }
          await mainWindow.showWindow({ focusOnShow: false });
        } else {
          await mainWindow.minimize();
        }
        break;
      case SfmlWindowCommandType.RequestFocus:
        if (!mainWindow.isWindowShowing() ||
          mainWindow.getWindowStatus() === window.WindowStatusType.MINIMIZE) {
          throw new Error('Cannot focus a hidden or minimized Stage window');
        }
        await mainWindow.showWindow({ focusOnShow: true });
        let focusError: Error | undefined;
        for (let attempt = 0; attempt < 25 && Date.now() + 100 < command.expiresAt; ++attempt) {
          try {
            const requestSurfaceFocus = this.activeSurfaceBinding?.requestFocus;
            if (!requestSurfaceFocus) {
              throw new Error('The SFML XComponent focus target is not available');
            }
            requestSurfaceFocus();
            focusError = undefined;
            break;
          } catch (error) {
            focusError = error as Error;
            await this.delayWindowStatePoll();
          }
        }
        if (focusError) {
          if (!mainWindow.isFocused()) {
            throw focusError;
          }
          hilog.warn(
            DOMAIN,
            TAG,
            'requestFocus(sfml-surface) failed while the Stage window is focused; using the default-focus XComponent: %{public}s',
            focusError.message
          );
        }
        break;
      default:
        throw new Error(`Unknown SFML window command ${command.type}`);
    }
  }

  private windowStateSignature(mainWindow: window.Window, state: SfmlWindowState): string {
    return `${state.x},${state.y},${state.width},${state.height},${state.visible},${state.focused},` +
      `${state.fullscreen},${mainWindow.getWindowStatus()}`;
  }

  private windowCommandStateMatches(
    mainWindow: window.Window,
    command: SfmlWindowCommand,
    state: SfmlWindowState
  ): boolean {
    const status = mainWindow.getWindowStatus();
    switch (command.type) {
      case SfmlWindowCommandType.Configure:
        if (command.fullscreen) {
          return state.fullscreen && (status === window.WindowStatusType.MAXIMIZE ||
            status === window.WindowStatusType.FULL_SCREEN);
        }
        return status === window.WindowStatusType.FLOATING &&
          (command.width === 0 || state.width === command.width) &&
          (command.height === 0 || state.height === command.height);
      case SfmlWindowCommandType.Restore:
        return status === window.WindowStatusType.FLOATING;
      case SfmlWindowCommandType.SetPosition:
        return status !== window.WindowStatusType.FLOATING || (state.x === command.x && state.y === command.y);
      case SfmlWindowCommandType.SetSize:
        return state.width === command.width && state.height === command.height;
      case SfmlWindowCommandType.SetVisible:
        return state.visible === command.visible;
      case SfmlWindowCommandType.RequestFocus:
        return state.focused;
      default:
        return true;
    }
  }

  private delayWindowStatePoll(): Promise<void> {
    return new Promise<void>((resolve: () => void) => {
      setTimeout(() => resolve(), 20);
    });
  }

  private async awaitWindowCommandState(
    nativeContext: SfmlNativeContext,
    mainWindow: window.Window,
    command: SfmlWindowCommand,
    startingGeneration: number,
    startingState: SfmlWindowState
  ): Promise<void> {
    if (command.type === SfmlWindowCommandType.SetMinimumSize ||
      command.type === SfmlWindowCommandType.SetMaximumSize ||
      command.type === SfmlWindowCommandType.SetTitle || command.type === SfmlWindowCommandType.SetIcon) {
      return;
    }

    let previousSignature = '';
    let stablePolls = 0;
    for (let attempt = 0; attempt < 80; ++attempt) {
      if (Date.now() >= command.expiresAt || !nativeContext.isWindowCommandPending(command.requestId)) {
        throw new Error(`Window command ${command.requestId} expired while waiting for Stage state`);
      }
      const state = this.queryWindowState(mainWindow);
      const signature = this.windowStateSignature(mainWindow, state);
      stablePolls = signature === previousSignature ? stablePolls + 1 : 0;
      previousSignature = signature;
      const observedEvent = this.windowStateGeneration !== startingGeneration;

      if (this.windowCommandStateMatches(mainWindow, command, state) && (observedEvent || attempt >= 2)) {
        return;
      }
      const clampableGeometry = command.type === SfmlWindowCommandType.SetSize ||
        (command.type === SfmlWindowCommandType.SetPosition &&
          mainWindow.getWindowStatus() === window.WindowStatusType.FLOATING) ||
        (command.type === SfmlWindowCommandType.Configure && !command.fullscreen);
      const relevantGeometryChanged = command.type === SfmlWindowCommandType.SetPosition ?
        (state.x !== startingState.x || state.y !== startingState.y) :
        (state.width !== startingState.width || state.height !== startingState.height);
      // Only geometry can legitimately settle at a manager-clamped value.
      // Do not treat an unrelated delayed event as acknowledgement of an
      // unchanged drawable/position from the preceding serialized command.
      if (clampableGeometry && relevantGeometryChanged &&
        ((observedEvent && stablePolls >= 2) || (attempt >= 25 && stablePolls >= 5))) {
        return;
      }
      await this.delayWindowStatePoll();
    }
    throw new Error(`Stage state did not settle for window command ${command.requestId}`);
  }

  private completeHostWindowCommand(
    nativeContext: SfmlNativeContext,
    command: SfmlWindowCommand,
    success: boolean,
    mainWindow?: window.Window
  ): boolean {
    let state = new SfmlWindowState(0, 0, 0, 0, false, false, false);
    try {
      if (mainWindow) {
        state = this.queryWindowState(mainWindow);
      }
    } catch (error) {
      this.logHostFailure('Querying Stage state for command acknowledgement', error as Error);
    }
    return nativeContext.completeWindowCommand(
      command.requestId,
      success,
      state.x,
      state.y,
      state.width,
      state.height,
      state.visible,
      state.focused,
      state.fullscreen
    );
  }

  private async compensateFailedConfigure(mainWindow: window.Window, startingTitle: string): Promise<void> {
    try {
      await this.restoreWindowBaseline(mainWindow);
      await mainWindow.setWindowTitle(startingTitle);
      this.windowBaseline = undefined;
    } catch (error) {
      this.logHostFailure('Compensating a failed or expired Configure command', error as Error);
    }
  }

  private async compensateExpiredRestore(
    mainWindow: window.Window,
    startingState: SfmlWindowState,
    startingLimits: window.WindowLimits,
    startingPendingPosition: SfmlWindowPosition | undefined,
    startingBaseline: SfmlWindowBaseline | undefined,
    startingStyle: number,
    startingKeepScreenOn: boolean,
    startingDecorVisible: boolean
  ): Promise<void> {
    this.suspendPendingFloatingPosition = true;
    try {
      await mainWindow.setWindowLimits(startingLimits);
      if (startingState.fullscreen) {
        await this.applyWindowStyle(mainWindow, startingStyle);
        await this.setFullscreenPolicy(mainWindow, true, startingKeepScreenOn);
      } else {
        await this.setFullscreenPolicy(mainWindow, false, startingKeepScreenOn);
        await this.applyWindowStyle(mainWindow, startingStyle);
        await this.resizeClientArea(mainWindow, startingState.width, startingState.height);
        await mainWindow.moveWindowToGlobalDisplay(startingState.x, startingState.y);
      }

      mainWindow.setWindowDecorVisible(startingDecorVisible);
      mainWindow.setWindowTitleMoveEnabled((startingStyle & 0x7) !== 0);
      if (startingState.visible) {
        await mainWindow.showWindow({ focusOnShow: startingState.focused });
      } else if (mainWindow.getWindowStatus() !== window.WindowStatusType.MINIMIZE) {
        await mainWindow.minimize();
      }
    } finally {
      this.suspendPendingFloatingPosition = false;
      // Applying Restore may consume a deferred position, but a rejected
      // Restore must retain the exact Stage baseline for the next attempt.
      this.pendingFloatingPosition = startingPendingPosition;
      this.windowBaseline = startingBaseline;
    }
  }

  private async compensateExpiredWindowCommand(
    context: common.UIAbilityContext,
    mainWindow: window.Window,
    command: SfmlWindowCommand,
    startingState: SfmlWindowState,
    startingLimits: window.WindowLimits,
    startingTitle: string,
    startingIcon: SfmlWindowCommand | undefined,
    startingPendingPosition: SfmlWindowPosition | undefined,
    startingBaseline: SfmlWindowBaseline | undefined,
    startingStyle: number,
    startingKeepScreenOn: boolean,
    startingDecorVisible: boolean
  ): Promise<void> {
    if (this.nativeHostDestroyed) {
      return;
    }

    try {
      switch (command.type) {
        case SfmlWindowCommandType.Configure:
          await this.compensateFailedConfigure(mainWindow, startingTitle);
          break;
        case SfmlWindowCommandType.Restore:
          await this.compensateExpiredRestore(
            mainWindow,
            startingState,
            startingLimits,
            startingPendingPosition,
            startingBaseline,
            startingStyle,
            startingKeepScreenOn,
            startingDecorVisible
          );
          break;
        case SfmlWindowCommandType.SetPosition:
          this.pendingFloatingPosition = startingPendingPosition;
          if (mainWindow.getWindowStatus() === window.WindowStatusType.FLOATING) {
            await mainWindow.moveWindowToGlobalDisplay(startingState.x, startingState.y);
          }
          break;
        case SfmlWindowCommandType.SetSize:
          await this.resizeClientArea(mainWindow, startingState.width, startingState.height);
          break;
        case SfmlWindowCommandType.SetMinimumSize:
        case SfmlWindowCommandType.SetMaximumSize:
          await mainWindow.setWindowLimits(startingLimits);
          break;
        case SfmlWindowCommandType.SetTitle:
          await mainWindow.setWindowTitle(startingTitle);
          break;
        case SfmlWindowCommandType.SetIcon:
          if (startingIcon) {
            await this.setMissionWindowIcon(context, startingIcon);
          }
          break;
        case SfmlWindowCommandType.SetVisible:
          if (startingState.visible) {
            if (mainWindow.getWindowStatus() === window.WindowStatusType.MINIMIZE) {
              await mainWindow.restore();
            }
            await mainWindow.showWindow({ focusOnShow: startingState.focused });
          } else if (mainWindow.getWindowStatus() !== window.WindowStatusType.MINIMIZE) {
            await mainWindow.minimize();
          }
          break;
        case SfmlWindowCommandType.RequestFocus:
          // Harmony has no public "restore prior focus owner" operation. The
          // command is not started without the native deadline safety margin.
          break;
      }
    } catch (error) {
      this.logHostFailure(`Compensating expired window command ${command.requestId}`, error as Error);
    }
  }

  private async runHostWindowCommand(
    context: common.UIAbilityContext,
    nativeContext: SfmlNativeContext,
    command: SfmlWindowCommand
  ): Promise<void> {
    let mainWindow: window.Window | undefined;
    let startingState: SfmlWindowState | undefined;
    let startingLimits: window.WindowLimits | undefined;
    let startingDecorVisible = false;
    let statePublicationSuppressed = false;
    let mutationStarted = false;
    const startingTitle = this.confirmedWindowTitle;
    const startingIcon = this.confirmedWindowIcon;
    const startingStyle = this.confirmedWindowStyle;
    const startingKeepScreenOn = this.confirmedKeepScreenOn;
    const startingPendingPosition = this.pendingFloatingPosition ?
      new SfmlWindowPosition(this.pendingFloatingPosition.x, this.pendingFloatingPosition.y) : undefined;
    const startingBaseline = this.cloneWindowBaseline();
    if (Date.now() >= command.expiresAt || !nativeContext.isWindowCommandPending(command.requestId)) {
      if (nativeContext.isWindowCommandPending(command.requestId)) {
        this.completeHostWindowCommand(nativeContext, command, false);
      }
      return;
    }
    try {
      if (this.nativeHostDestroyed) {
        throw new Error('Native host was destroyed');
      }
      mainWindow = await window.getLastWindow(context);
      this.bindWindowStateObservers(mainWindow);
      if (Date.now() >= command.expiresAt || !nativeContext.isWindowCommandPending(command.requestId)) {
        if (nativeContext.isWindowCommandPending(command.requestId)) {
          this.completeHostWindowCommand(nativeContext, command, false, mainWindow);
        }
        return;
      }
      const startingGeneration = this.windowStateGeneration;
      startingState = this.queryWindowState(mainWindow);
      const currentLimits = mainWindow.getWindowLimits();
      startingLimits = {
        minWidth: currentLimits.minWidth,
        minHeight: currentLimits.minHeight,
        maxWidth: currentLimits.maxWidth,
        maxHeight: currentLimits.maxHeight
      };
      startingDecorVisible = mainWindow.getWindowDecorVisible();
      if (Date.now() + 100 >= command.expiresAt) {
        throw new Error(`Window command ${command.requestId} has insufficient time before its native deadline`);
      }
      this.windowStatePublishSuppressed = true;
      this.windowStatePublishDirty = false;
      statePublicationSuppressed = true;
      mutationStarted = true;
      await this.applyWindowCommand(context, mainWindow, command);
      const stillPending = nativeContext.isWindowCommandPending(command.requestId);
      if (Date.now() >= command.expiresAt || !stillPending) {
        if (startingState && startingLimits) {
          await this.compensateExpiredWindowCommand(
            context,
            mainWindow,
            command,
            startingState,
            startingLimits,
            startingTitle,
            startingIcon,
            startingPendingPosition,
            startingBaseline,
            startingStyle,
            startingKeepScreenOn,
            startingDecorVisible
          );
        }
        if (nativeContext.isWindowCommandPending(command.requestId)) {
          this.completeHostWindowCommand(nativeContext, command, false, mainWindow);
        }
        return;
      }
      await this.awaitWindowCommandState(nativeContext, mainWindow, command, startingGeneration, startingState);
      const accepted = this.completeHostWindowCommand(nativeContext, command, true, mainWindow);
      if (accepted) {
        if (command.type === SfmlWindowCommandType.Configure || command.type === SfmlWindowCommandType.SetTitle) {
          this.confirmedWindowTitle = command.title;
        }
        if (command.type === SfmlWindowCommandType.Configure) {
          this.confirmedWindowStyle = command.style;
          this.confirmedKeepScreenOn = command.keepScreenOn;
        } else if (command.type === SfmlWindowCommandType.SetIcon) {
          this.confirmedWindowIcon = command;
        } else if (command.type === SfmlWindowCommandType.Restore) {
          this.windowBaseline = undefined;
        }
      } else if (startingState && startingLimits) {
        await this.compensateExpiredWindowCommand(
          context,
          mainWindow,
          command,
          startingState,
          startingLimits,
          startingTitle,
          startingIcon,
          startingPendingPosition,
          startingBaseline,
          startingStyle,
          startingKeepScreenOn,
          startingDecorVisible
        );
      }
    } catch (error) {
      this.logHostFailure(`Executing window command ${command.requestId}`, error as Error);
      if (mutationStarted && mainWindow && startingState && startingLimits) {
        await this.compensateExpiredWindowCommand(
          context,
          mainWindow,
          command,
          startingState,
          startingLimits,
          startingTitle,
          startingIcon,
          startingPendingPosition,
          startingBaseline,
          startingStyle,
          startingKeepScreenOn,
          startingDecorVisible
        );
      }
      if (nativeContext.isWindowCommandPending(command.requestId)) {
        this.completeHostWindowCommand(nativeContext, command, false, mainWindow);
      }
    } finally {
      if (statePublicationSuppressed) {
        // Successful acknowledgements already carry the final queried state;
        // failed commands retain the native side's last confirmed snapshot.
        this.windowStatePublishSuppressed = false;
        this.windowStatePublishDirty = false;
      }
    }
  }

  private executeWindowCommand(context: common.UIAbilityContext, command: SfmlWindowCommand): void {
    const nativeContext = this.nativeContext ?? this.lifecycleContext;
    if (!nativeContext) {
      hilog.warn(DOMAIN, TAG, 'Unable to acknowledge window command %{public}d without a native context', command.requestId);
      return;
    }
    // Keep a compensation margin before the native three-second deadline.
    command.expiresAt = Date.now() + Math.max(0, command.timeoutMs - 500);
    this.windowCommandQueue = this.windowCommandQueue.then(() =>
      this.runHostWindowCommand(context, nativeContext, command)
    ).catch((error: Error) => {
      this.logHostFailure(`Running window command ${command.requestId}`, error);
      if (nativeContext.isWindowCommandPending(command.requestId)) {
        this.completeHostWindowCommand(nativeContext, command, false);
      }
    });
  }

  private releaseFreeWindowModeListener(): void {
    const mainWindow = this.freeWindowModeWindow;
    const callback = this.freeWindowModeChangeCallback;
    ++this.freeWindowModeListenerGeneration;
    this.freeWindowModeWindow = undefined;
    this.freeWindowModeChangeCallback = undefined;
    if (mainWindow === undefined || callback === undefined) {
      return;
    }
    try {
      mainWindow.off('freeWindowModeChange', callback);
    } catch (error) {
      this.logHostFailure('Stopping free window mode monitoring', error as Error);
    }
  }

  private configureNativeDisplayScaleHost(
    nativeContext: SfmlNativeContext,
    context: common.UIAbilityContext,
    hostGeneration: number
  ): boolean {
    if (!this.displayScaleConfigurable) {
      this.displayScaleHostConfigured = false;
      nativeContext.clearDisplayScaleHost();
      return false;
    }

    let mainWindow: window.Window;
    try {
      mainWindow = context.windowStage.getMainWindowSync();
    } catch (error) {
      this.displayScaleHostConfigured = false;
      nativeContext.clearDisplayScaleHost();
      this.logHostFailure('Obtaining the Stage window for display scaling', error as Error);
      return false;
    }

    let maximumWindowedWidth = 0;
    let maximumWindowedHeight = 0;
    try {
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

    try {
      this.displayScaleHostConfigured = nativeContext.configureDisplayScaleHost(
        true,
        maximumWindowedWidth,
        maximumWindowedHeight,
        (scale: number, gameWidth: number, gameHeight: number) => {
          if (hostGeneration === this.windowHostGeneration && this.displayScaleConfigurable) {
            this.requestDisplayScale(context, scale, gameWidth, gameHeight);
          }
        }
      );
      if (!this.displayScaleHostConfigured) {
        nativeContext.clearDisplayScaleHost();
        hilog.warn(DOMAIN, TAG, 'Configuring display scaling failed');
      }
    } catch (error) {
      this.displayScaleHostConfigured = false;
      nativeContext.clearDisplayScaleHost();
      this.logHostFailure('Configuring display scaling', error as Error);
    }
    return this.displayScaleHostConfigured;
  }

  private updateDisplayScaleConfigurable(configurable: boolean): void {
    const changed = this.displayScaleConfigurable !== configurable;
    if (!changed && this.displayScaleHostConfigured === configurable) {
      return;
    }
    this.displayScaleConfigurable = configurable;
    if (changed) {
      ++this.displayScaleRequest;
    }
    const nativeContext = this.nativeContext;
    if (nativeContext === undefined || !this.hostInitialized) {
      this.displayScaleHostConfigured = false;
      return;
    }
    if (!configurable) {
      this.displayScaleHostConfigured = false;
      nativeContext.clearDisplayScaleHost();
      return;
    }
    const hostContext = this.getUIContext().getHostContext();
    if (hostContext === undefined) {
      this.displayScaleHostConfigured = false;
      nativeContext.clearDisplayScaleHost();
      return;
    }
    this.configureNativeDisplayScaleHost(
      nativeContext,
      hostContext as common.UIAbilityContext,
      this.windowHostGeneration
    );
  }

  private refreshFreeWindowMode(): void {
    const mainWindow = this.freeWindowModeWindow;
    if (mainWindow === undefined) {
      return;
    }
    try {
      this.updateDisplayScaleConfigurable(
        deviceInfo.deviceType === 'tablet' && mainWindow.isInFreeWindowMode()
      );
    } catch (error) {
      this.updateDisplayScaleConfigurable(false);
      this.logHostFailure('Querying free window mode', error as Error);
    }
  }

  private trackFreeWindowMode(mainWindow: window.Window): void {
    if (this.freeWindowModeWindow === mainWindow && this.freeWindowModeChangeCallback !== undefined) {
      this.refreshFreeWindowMode();
      return;
    }
    this.releaseFreeWindowModeListener();
    this.updateDisplayScaleConfigurable(false);
    if (deviceInfo.deviceType !== 'tablet') {
      return;
    }

    const listenerGeneration = ++this.freeWindowModeListenerGeneration;
    const callback = (enabled: boolean): void => {
      if (listenerGeneration === this.freeWindowModeListenerGeneration &&
        this.freeWindowModeWindow === mainWindow) {
        this.updateDisplayScaleConfigurable(enabled);
      }
    };
    try {
      mainWindow.on('freeWindowModeChange', callback);
      this.freeWindowModeWindow = mainWindow;
      this.freeWindowModeChangeCallback = callback;
      this.refreshFreeWindowMode();
    } catch (error) {
      this.updateDisplayScaleConfigurable(false);
      this.logHostFailure('Starting free window mode monitoring', error as Error);
    }
  }

  private refreshFreeWindowModeTracking(): void {
    const hostContext = this.getUIContext().getHostContext();
    if (hostContext === undefined) {
      this.updateDisplayScaleConfigurable(false);
      return;
    }
    try {
      this.trackFreeWindowMode(
        (hostContext as common.UIAbilityContext).windowStage.getMainWindowSync()
      );
    } catch (error) {
      this.updateDisplayScaleConfigurable(false);
      this.logHostFailure('Refreshing free window mode monitoring', error as Error);
    }
  }

  private isDisplayScaleRequestCurrent(request: number, generation: number): boolean {
    return this.displayScaleConfigurable && request === this.displayScaleRequest &&
      generation === this.windowHostGeneration;
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
      if (!this.isDisplayScaleRequestCurrent(request, generation)) {
        return;
      }

      if (scale === 0) {
        const status = mainWindow.getWindowStatus();
        if (status !== window.WindowStatusType.MAXIMIZE &&
          status !== window.WindowStatusType.FULL_SCREEN) {
          await mainWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE);
        }
        if (!this.isDisplayScaleRequestCurrent(request, generation)) {
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
      if (!this.isDisplayScaleRequestCurrent(request, generation)) {
        return;
      }
      const width = Math.max(1, Math.round(gameWidth * scale));
      const height = Math.max(1, Math.round(gameHeight * scale));
      await mainWindow.resizeAsync(width, height);
      if (!this.isDisplayScaleRequestCurrent(request, generation)) {
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
      (command: SfmlWindowCommand) => this.executeWindowCommand(context, command),
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
    this.displayScaleHostConfigured = false;
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
      this.refreshFreeWindowModeTracking();
      context.onForeground();
      if (this.mainWindow !== undefined) {
        this.publishWindowState(this.mainWindow);
      }
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
    const mainWindow = abilityContext.windowStage.getMainWindowSync();
    this.trackFreeWindowMode(mainWindow);
    const hostGeneration = ++this.windowHostGeneration;
    this.configureNativeDisplayScaleHost(nativeContext, abilityContext, hostGeneration);
    const initialized = nativeContext.initializeHost(
      abilityContext.resourceManager,
      this.windowId,
      deviceInfo.deviceType,
      this.createHostCallbacks(abilityContext)
    );
    if (!initialized) {
      this.displayScaleHostConfigured = false;
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
      this.hostInitialized = false;
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
    this.releaseFreeWindowModeListener();
    this.displayScaleConfigurable = false;
    this.displayScaleHostConfigured = false;
    ++this.customPointerRequest;
    this.releaseCustomPointer();
    this.invalidateNativeBridges(context);
    this.unbindWindowStateObservers();
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

  private handleSurfaceNodeLoadFailure(binding: SurfaceNodeBinding): void {
    if (binding !== this.latestSurfaceBinding || binding.destroyed || binding.retired) {
      return;
    }

    this.hostInitialized = false;
    if (GRAPHICS_API === 'OpenGL') {
      this.status = 'Native OpenGL module unavailable. This device may not provide libGLv4.so; ' +
        'export the OpenGL ES backend or use a desktop-OpenGL-capable 2in1 device.';
      hilog.error(
        DOMAIN,
        TAG,
        'XComponent native context is unavailable for OpenGL. The device may be missing libGLv4.so; ' +
          'no automatic OpenGL ES fallback was attempted.'
      );
      return;
    }

    this.status = 'Native OpenGL ES module unavailable. Check the device graphics runtime and HiLog.';
    hilog.error(DOMAIN, TAG, 'XComponent native context is unavailable for OpenGL ES');
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
        surfaceLoadFailed: (binding: SurfaceNodeBinding) => this.handleSurfaceNodeLoadFailure(binding),
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
