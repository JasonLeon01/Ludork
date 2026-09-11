package com.ludork.android;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Rect;
import android.net.Uri;
import android.net.http.SslError;
import android.os.Build;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.window.OnBackInvokedDispatcher;

final class LudorkWebViewDialog {
    private final LudorkActivity activity;
    private Dialog dialog;
    private WebView browser;
    private FrameLayout page;
    private TextView error;
    private long sessionId;
    private String navigationUrl = "";

    LudorkWebViewDialog(LudorkActivity activity) {
        this.activity = activity;
    }

    @SuppressLint("SetJavaScriptEnabled")
    void show(long id, String url) {
        if (!activity.isTextInputAvailable() || !isWebUrl(url)) {
            LudorkActivity.completeWebView(id);
            return;
        }
        if (dialog != null && browser != null) {
            browser.stopLoading();
            sessionId = id;
            navigationUrl = url;
            error.setVisibility(View.GONE);
            browser.setVisibility(View.VISIBLE);
            try {
                browser.loadUrl(url);
            } catch (RuntimeException failure) {
                showError("Unable to load this page.\n" + failure.getMessage());
            }
            return;
        }
        finish(false);
        sessionId = id;
        navigationUrl = url;
        dialog = new Dialog(activity, android.R.style.Theme_Material_Light_NoActionBar_Fullscreen) {
            @Override
            @SuppressLint("GestureBackNavigation")
            @SuppressWarnings("deprecation")
            public void onBackPressed() {
                back();
            }

            @Override
            public boolean onKeyUp(int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ESCAPE) {
                    back();
                    return true;
                }
                return super.onKeyUp(keyCode, event);
            }
        };
        dialog.setCanceledOnTouchOutside(false);
        dialog.setOnCancelListener(ignored -> finish(true));
        LinearLayout content = new LinearLayout(activity);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setBackgroundColor(Color.WHITE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            content.setOnApplyWindowInsetsListener((view, insets) -> {
                int types = WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout()
                        | WindowInsets.Type.ime();
                Insets padding = insets.getInsets(types);
                view.setPadding(padding.left, padding.top, padding.right, padding.bottom);
                return new WindowInsets.Builder(insets).setInsets(types, Insets.NONE).build();
            });
        } else {
            content.setFitsSystemWindows(true);
        }
        LinearLayout header = new LinearLayout(activity);
        header.setGravity(Gravity.END | Gravity.CENTER_VERTICAL);
        Button close = new Button(activity);
        close.setText("Close");
        close.setContentDescription("Close web page");
        close.setOnClickListener(ignored -> finish(true));
        header.addView(close);
        content.addView(header, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        page = new FrameLayout(activity);
        content.addView(page, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        error = new TextView(activity);
        error.setTextColor(Color.DKGRAY);
        error.setBackgroundColor(Color.WHITE);
        error.setGravity(Gravity.CENTER);
        int padding = Math.round(24 * activity.getResources().getDisplayMetrics().density);
        error.setPadding(padding, padding, padding, padding);
        error.setVisibility(View.GONE);
        try {
            browser = new WebView(activity);
            WebSettings settings = browser.getSettings();
            settings.setJavaScriptEnabled(true);
            settings.setDomStorageEnabled(true);
            settings.setAllowFileAccess(false);
            settings.setAllowContentAccess(false);
            settings.setSupportMultipleWindows(false);
            settings.setJavaScriptCanOpenWindowsAutomatically(true);
            settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
            settings.setBuiltInZoomControls(true);
            settings.setDisplayZoomControls(false);
            browser.setWebChromeClient(new WebChromeClient());
            browser.setWebViewClient(new WebViewClient() {
                @Override
                public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                    String address = request.getUrl().toString();
                    if (!isWebUrl(address)) {
                        return true;
                    }
                    if (view == browser && request.isForMainFrame()) {
                        navigationUrl = address;
                    }
                    return false;
                }

                @Override
                public void onPageStarted(WebView view, String address, android.graphics.Bitmap icon) {
                    if (isCurrentMainFrame(view, address) && error != null) {
                        error.setVisibility(View.GONE);
                    }
                }

                @Override
                public void onReceivedError(WebView view, WebResourceRequest request,
                                            WebResourceError failure) {
                    if (request.isForMainFrame()
                            && isCurrentMainFrame(view, request.getUrl().toString())
                            && !failure.getDescription().toString().contains("ERR_ABORTED")) {
                        showError("Unable to load this page.\n" + failure.getDescription());
                    }
                }

                @Override
                public void onReceivedHttpError(WebView view, WebResourceRequest request,
                                                WebResourceResponse response) {
                    if (request.isForMainFrame()
                            && isCurrentMainFrame(view, request.getUrl().toString())) {
                        showError("Unable to load this page (HTTP " + response.getStatusCode() + ").");
                    }
                }

                @Override
                public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError failure) {
                    handler.cancel();
                    if (isCurrentMainFrame(view, failure.getUrl())) {
                        showError("Unable to verify this page's certificate.");
                    }
                }

                @Override
                public boolean onRenderProcessGone(WebView view, RenderProcessGoneDetail detail) {
                    if (view == browser) {
                        browser = null;
                        page.removeView(view);
                        view.destroy();
                        showError("The web page stopped unexpectedly. Close it and open it again.");
                    }
                    return true;
                }
            });
            page.addView(browser, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        } catch (RuntimeException failure) {
            if (browser != null) {
                browser.destroy();
                browser = null;
            }
            showError("The system WebView is unavailable.\n" + failure.getMessage());
        }
        page.addView(error, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        dialog.setContentView(content);
        Window window = dialog.getWindow();
        if (window != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                window.setDecorFitsSystemWindows(false);
            }
            window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }
        try {
            dialog.show();
            if (window != null) {
                window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                dialog.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                        OnBackInvokedDispatcher.PRIORITY_DEFAULT, this::back);
            }
            if (browser != null) {
                browser.loadUrl(url);
                browser.requestFocus();
            }
        } catch (RuntimeException failure) {
            finish(true);
        }
    }

    boolean isVisible() {
        return sessionId != 0;
    }

    boolean back() {
        if (sessionId == 0) {
            return false;
        }
        Window window = dialog == null ? null : dialog.getWindow();
        if (window != null) {
            View decor = window.getDecorView();
            boolean keyboardVisible;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                WindowInsets insets = decor.getRootWindowInsets();
                keyboardVisible = insets != null && insets.isVisible(WindowInsets.Type.ime());
            } else {
                Rect visible = new Rect();
                decor.getWindowVisibleDisplayFrame(visible);
                keyboardVisible = decor.getRootView().getHeight() - visible.height()
                        > 100 * activity.getResources().getDisplayMetrics().density;
            }
            if (keyboardVisible) {
                InputMethodManager input = activity.getSystemService(InputMethodManager.class);
                if (input != null) {
                    input.hideSoftInputFromWindow(decor.getWindowToken(), 0);
                }
                return true;
            }
        }
        finish(true);
        return true;
    }

    void dismiss(long id) {
        if (sessionId == id) {
            finish(false);
        }
    }

    void cancel() {
        finish(true);
    }

    private void showError(String message) {
        if (error != null) {
            error.setText(message);
            error.setVisibility(View.VISIBLE);
        }
    }

    private void finish(boolean notify) {
        long previousId = sessionId;
        Dialog previousDialog = dialog;
        WebView previousBrowser = browser;
        sessionId = 0;
        navigationUrl = "";
        dialog = null;
        browser = null;
        error = null;
        if (page != null) {
            page.removeAllViews();
            page = null;
        }
        if (previousDialog != null) {
            previousDialog.setOnCancelListener(null);
            previousDialog.dismiss();
        }
        if (previousBrowser != null) {
            previousBrowser.stopLoading();
            previousBrowser.setWebChromeClient(null);
            previousBrowser.setWebViewClient(new WebViewClient());
            previousBrowser.destroy();
        }
        if (notify && previousId != 0) {
            LudorkActivity.completeWebView(previousId);
        }
    }

    private boolean isCurrentMainFrame(WebView view, String address) {
        return view == browser && (sameUrl(address, navigationUrl)
                || (sameUrl(view.getOriginalUrl(), navigationUrl) && sameUrl(address, view.getUrl())));
    }

    private static boolean sameUrl(String left, String right) {
        return left != null && right != null
                && Uri.parse(left).normalizeScheme().buildUpon().fragment(null).build()
                .equals(Uri.parse(right).normalizeScheme().buildUpon().fragment(null).build());
    }

    private static boolean isWebUrl(String url) {
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        return ("http".equalsIgnoreCase(scheme) || "https".equalsIgnoreCase(scheme))
                && uri.getHost() != null && !uri.getHost().isEmpty();
    }
}
